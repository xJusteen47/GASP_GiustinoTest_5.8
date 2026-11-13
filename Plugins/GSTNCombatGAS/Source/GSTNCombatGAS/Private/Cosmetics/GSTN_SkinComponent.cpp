// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Cosmetics/GSTN_SkinComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Cosmetics/GSTN_AttachmentProvider.h"
#include "Cosmetics/GSTN_EquipmentVisual.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Kits/GSTN_SkinDefinition.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

namespace
{
	/** Marks a piece authored bShownByDefault false, so a cleared slot override goes back to hidden */
	const FName HiddenByDefaultTag(TEXT("GSTN.HiddenByDefault"));
}

UGSTN_SkinComponent::UGSTN_SkinComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// UE5 mannequin bones (GASP): placeholder implants land on the right body part before the Implant_* sockets exist
	SocketFallbacks.Add(GSTNEquipment::Socket_ImplantBack, TEXT("spine_05"));
	SocketFallbacks.Add(GSTNEquipment::Socket_ImplantHandL, TEXT("hand_l"));
	SocketFallbacks.Add(GSTNEquipment::Socket_ImplantHandR, TEXT("hand_r"));
	SocketFallbacks.Add(GSTNEquipment::Socket_ImplantFootL, TEXT("foot_l"));
	SocketFallbacks.Add(GSTNEquipment::Socket_ImplantFootR, TEXT("foot_r"));
	SocketFallbacks.Add(TEXT("Implant_Arm_L"), TEXT("lowerarm_l"));
	SocketFallbacks.Add(TEXT("Implant_Arm_R"), TEXT("lowerarm_r"));
	SocketFallbacks.Add(TEXT("Implant_Leg_L"), TEXT("calf_l"));
	SocketFallbacks.Add(TEXT("Implant_Leg_R"), TEXT("calf_r"));
	SocketFallbacks.Add(GSTNEquipment::Socket_WeaponHandR, TEXT("hand_r"));
	SocketFallbacks.Add(TEXT("Weapon_Hand_L"), TEXT("hand_l"));
}

void UGSTN_SkinComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bAutoBind || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!TryBindToPlayerState() && MaxBindAttempts > 0 && GetWorld())
	{
		AttemptsLeft = MaxBindAttempts;
		GetWorld()->GetTimerManager().SetTimer(RetryHandle, this, &UGSTN_SkinComponent::RetryBind, BindRetryInterval, true);
	}
}

void UGSTN_SkinComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryHandle);
	}
	if (UGSTN_LoadoutComponent* Loadout = BoundLoadout.Get())
	{
		Loadout->OnLoadoutApplied.RemoveDynamic(this, &UGSTN_SkinComponent::HandleLoadoutApplied);
	}
	BoundLoadout.Reset();
	CancelPendingLoad();
	ClearAttachments();
	Super::EndPlay(EndPlayReason);
}

USkeletalMeshComponent* UGSTN_SkinComponent::GetTargetMesh() const
{
	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		return Character->GetMesh();
	}
	return GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

void UGSTN_SkinComponent::RetryBind()
{
	if (TryBindToPlayerState() || --AttemptsLeft <= 0)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RetryHandle);
		}
	}
}

bool UGSTN_SkinComponent::TryBindToPlayerState()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
	UGSTN_LoadoutComponent* Loadout = PlayerState ? PlayerState->FindComponentByClass<UGSTN_LoadoutComponent>() : nullptr;
	if (!Loadout)
	{
		return false;
	}
	BindToLoadout(Loadout);
	return true;
}

void UGSTN_SkinComponent::BindToLoadout(UGSTN_LoadoutComponent* Loadout)
{
	if (UGSTN_LoadoutComponent* Previous = BoundLoadout.Get())
	{
		Previous->OnLoadoutApplied.RemoveDynamic(this, &UGSTN_SkinComponent::HandleLoadoutApplied);
	}
	BoundLoadout = Loadout;
	if (!Loadout)
	{
		return;
	}
	Loadout->OnLoadoutApplied.AddUniqueDynamic(this, &UGSTN_SkinComponent::HandleLoadoutApplied);
	if (Loadout->HasLoadout())
	{
		ApplySkin(Loadout->GetResolvedSkin());
		RefreshAttachments();
	}
}

void UGSTN_SkinComponent::HandleLoadoutApplied(UGSTN_LoadoutComponent* Component, int32 Revision)
{
	ApplySkin(Component ? Component->GetResolvedSkin() : nullptr);
	RefreshAttachments();
}

void UGSTN_SkinComponent::ClearAttachments()
{
	for (USceneComponent* Attachment : Attachments)
	{
		if (Attachment)
		{
			Attachment->DestroyComponent();
		}
	}
	Attachments.Reset();
	AttachmentSlots.Reset();
}

void UGSTN_SkinComponent::CancelPendingLoad()
{
	++RefreshSerial;
	if (PendingLoad.IsValid())
	{
		PendingLoad->CancelHandle();
		PendingLoad.Reset();
	}
	PendingPieces.Reset();
}

bool UGSTN_SkinComponent::IsAttachmentLoadPending() const
{
	return PendingLoad.IsValid();
}

void UGSTN_SkinComponent::RefreshAttachments()
{
	CancelPendingLoad();
	if (GetNetMode() == NM_DedicatedServer || !GetTargetMesh() || !GetOwner())
	{
		ClearAttachments();
		return;
	}

	// The loadout's own look first, then the providers' (a backpack names the implant's slot, so it must come after it)
	const UGSTN_LoadoutComponent* Loadout = BoundLoadout.Get();
	FGSTN_AttachmentContext Context;
	Context.Pawn = GetOwner();
	Context.Resolved = Loadout && Loadout->HasLoadout() ? &Loadout->GetResolvedLoadout() : nullptr;
	TArray<FGSTN_AttachmentRequest> Requests;
	if (Context.Resolved)
	{
		const UGSTN_WeaponDefinition* Primary = Context.Resolved->Weapons.IsValidIndex(0) ? Context.Resolved->Weapons[0] : nullptr;
		FGSTN_EquipmentVisualUtils::GatherLoadoutVisuals(Context.Resolved->Kit, Primary, Requests);
	}
	FGSTN_AttachmentProviderRegistry::Gather(Context, Requests);

	TArray<FGSTN_AttachmentRequest> Pieces;
	FGSTN_EquipmentVisualUtils::Compose(Requests, Pieces);

	TArray<FSoftObjectPath> ToLoad;
	for (const FGSTN_AttachmentRequest& Piece : Pieces)
	{
		if (!Piece.Mesh.IsNull() && !Piece.Mesh.Get())
		{
			ToLoad.AddUnique(Piece.Mesh.ToSoftObjectPath());
		}
	}
	if (ToLoad.IsEmpty() || !UAssetManager::IsInitialized())
	{
		// Everything is in memory (or there is no streamer, as in a bare test world): swap now
		BuildAttachments(Pieces);
		return;
	}

	// The old look stays until every mesh is in; a newer refresh bumps the serial and this completion does nothing
	PendingPieces = MoveTemp(Pieces);
	const uint32 Serial = RefreshSerial;
	TWeakObjectPtr<UGSTN_SkinComponent> WeakThis(this);
	PendingLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(MoveTemp(ToLoad), FStreamableDelegate::CreateLambda([WeakThis, Serial]()
	{
		UGSTN_SkinComponent* This = WeakThis.Get();
		if (!This || This->RefreshSerial != Serial)
		{
			return;
		}
		TArray<FGSTN_AttachmentRequest> Ready = MoveTemp(This->PendingPieces);
		This->PendingPieces.Reset();
		This->PendingLoad.Reset();
		This->BuildAttachments(Ready);
	}));
	if (PendingLoad.IsValid() && PendingLoad->HasLoadCompleted() && PendingPieces.IsEmpty())
	{
		// The completion already ran inside the request (every path failed at once): nothing is pending any more
		PendingLoad.Reset();
	}
	if (!PendingLoad.IsValid() && RefreshSerial == Serial && !PendingPieces.IsEmpty())
	{
		// The streamer refused the request (bad paths): build what resolves now rather than keep a stale look
		TArray<FGSTN_AttachmentRequest> Ready = MoveTemp(PendingPieces);
		PendingPieces.Reset();
		BuildAttachments(Ready);
	}
}

FName UGSTN_SkinComponent::ResolveSocket(const USceneComponent& Parent, FName Socket) const
{
	if (Socket.IsNone() || Parent.DoesSocketExist(Socket))
	{
		return Socket;
	}
	const FName* Fallback = SocketFallbacks.Find(Socket);
	return Fallback && Parent.DoesSocketExist(*Fallback) ? *Fallback : Socket;
}

void UGSTN_SkinComponent::BuildAttachments(const TArray<FGSTN_AttachmentRequest>& Pieces)
{
	ClearAttachments();
	USkeletalMeshComponent* Mesh = GetTargetMesh();
	AActor* Owner = GetOwner();
	if (!Mesh || !Owner || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	for (const FGSTN_AttachmentRequest& Piece : Pieces)
	{
		// Loaded by the async request; the synchronous load only covers a path the streamer did not resolve
		UObject* Asset = Piece.Mesh.IsNull() ? nullptr : Piece.Mesh.Get();
		if (!Asset && !Piece.Mesh.IsNull())
		{
			Asset = Piece.Mesh.LoadSynchronous();
		}
		UMeshComponent* Attachment = nullptr;
		if (UStaticMesh* Static = Cast<UStaticMesh>(Asset))
		{
			UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
			Component->SetStaticMesh(Static);
			Attachment = Component;
		}
		else if (USkeletalMesh* Skeletal = Cast<USkeletalMesh>(Asset))
		{
			USkeletalMeshComponent* Component = NewObject<USkeletalMeshComponent>(Owner, NAME_None, RF_Transient);
			Component->SetSkeletalMeshAsset(Skeletal);
			Attachment = Component;
		}
		if (!Attachment)
		{
			continue;
		}

		// The piece made earlier under ParentSlot (Compose keeps the name only when one exists), else the pawn's mesh
		USceneComponent* Parent = Mesh;
		if (!Piece.ParentSlot.IsNone())
		{
			const int32 ParentIndex = AttachmentSlots.Find(Piece.ParentSlot);
			if (ParentIndex != INDEX_NONE && Attachments[ParentIndex])
			{
				Parent = Attachments[ParentIndex];
			}
		}

		// Cosmetic: never collides, never blocks the camera or a hit query
		Attachment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Attachment->SetGenerateOverlapEvents(false);
		Attachment->SetupAttachment(Parent, ResolveSocket(*Parent, Piece.Socket));
		Attachment->SetRelativeTransform(Piece.Offset);
		if (!Piece.bShownByDefault)
		{
			Attachment->ComponentTags.Add(HiddenByDefaultTag);
		}
		Attachment->RegisterComponent();

		if (Piece.HasMaterialParameters())
		{
			for (int32 Slot = 0; Slot < Attachment->GetNumMaterials(); ++Slot)
			{
				if (UMaterialInstanceDynamic* Dynamic = Attachment->CreateAndSetMaterialInstanceDynamic(Slot))
				{
					for (const TPair<FName, FLinearColor>& Pair : Piece.VectorParameters)
					{
						Dynamic->SetVectorParameterValue(Pair.Key, Pair.Value);
					}
					for (const TPair<FName, float>& Pair : Piece.ScalarParameters)
					{
						Dynamic->SetScalarParameterValue(Pair.Key, Pair.Value);
					}
				}
			}
		}

		Attachments.Add(Attachment);
		AttachmentSlots.Add(Piece.VisualSlot);
	}

	// Visibility last: a parent's propagation must not undo a child's own default
	for (int32 Index = 0; Index < Attachments.Num(); ++Index)
	{
		ApplyPieceVisibility(Index);
	}
}

void UGSTN_SkinComponent::ApplyPieceVisibility(int32 Index)
{
	USceneComponent* Attachment = Attachments.IsValidIndex(Index) ? Attachments[Index].Get() : nullptr;
	if (!Attachment)
	{
		return;
	}
	const FName Slot = AttachmentSlots[Index];
	const bool* Override = Slot.IsNone() ? nullptr : SlotShownOverrides.Find(Slot);
	const bool bDefault = !Attachment->ComponentTags.Contains(HiddenByDefaultTag);
	// Not propagated: a backpack on a hidden piece keeps its own visibility
	Attachment->SetVisibility(Override ? *Override : bDefault, false);
}

void UGSTN_SkinComponent::SetVisualSlotShown(FName Slot, bool bShown)
{
	if (Slot.IsNone())
	{
		return;
	}
	SlotShownOverrides.Add(Slot, bShown);
	for (int32 Index = 0; Index < Attachments.Num(); ++Index)
	{
		if (AttachmentSlots[Index] == Slot)
		{
			ApplyPieceVisibility(Index);
		}
	}
}

void UGSTN_SkinComponent::ClearVisualSlotShown(FName Slot)
{
	if (SlotShownOverrides.Remove(Slot) == 0)
	{
		return;
	}
	for (int32 Index = 0; Index < Attachments.Num(); ++Index)
	{
		if (AttachmentSlots[Index] == Slot)
		{
			ApplyPieceVisibility(Index);
		}
	}
}

USceneComponent* UGSTN_SkinComponent::FindVisualSlotComponent(FName Slot) const
{
	const int32 Index = Slot.IsNone() ? INDEX_NONE : AttachmentSlots.Find(Slot);
	return Index != INDEX_NONE ? Attachments[Index].Get() : nullptr;
}

void UGSTN_SkinComponent::CaptureOriginal(const USkeletalMeshComponent& Mesh)
{
	if (bCaptured)
	{
		return;
	}
	bCaptured = true;
	OriginalMesh = Mesh.GetSkeletalMeshAsset();
	OriginalAnimClass = Mesh.AnimClass;
	OriginalOverrides = Mesh.OverrideMaterials;
}

void UGSTN_SkinComponent::RestoreLook(USkeletalMeshComponent& Mesh)
{
	if (!bCaptured)
	{
		return;
	}
	if (Mesh.GetSkeletalMeshAsset() != OriginalMesh)
	{
		Mesh.SetSkeletalMeshAsset(OriginalMesh);
	}
	if (Mesh.AnimClass != OriginalAnimClass)
	{
		Mesh.SetAnimInstanceClass(OriginalAnimClass);
	}
	// A slot the skin overrode and the pawn did not goes back to null, which clears the override
	const int32 Slots = FMath::Max(Mesh.OverrideMaterials.Num(), OriginalOverrides.Num());
	for (int32 Slot = 0; Slot < Slots; ++Slot)
	{
		UMaterialInterface* Original = OriginalOverrides.IsValidIndex(Slot) ? OriginalOverrides[Slot].Get() : nullptr;
		if (Slot < Mesh.GetNumMaterials())
		{
			Mesh.SetMaterial(Slot, Original);
		}
	}
}

void UGSTN_SkinComponent::RestoreOriginal()
{
	if (USkeletalMeshComponent* Mesh = GetTargetMesh())
	{
		RestoreLook(*Mesh);
	}
	AppliedSkin.Reset();
}

bool UGSTN_SkinComponent::ApplySkin(UGSTN_SkinDefinition* Skin)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return false;
	}
	USkeletalMeshComponent* Mesh = GetTargetMesh();
	if (!Mesh)
	{
		return false;
	}

	UGSTN_SkinDefinition* Wanted = Skin && Skin->ChangesAnything() ? Skin : nullptr;
	if (Wanted == AppliedSkin.Get())
	{
		// A respawn or a repeated apply: the pawn already shows it
		return true;
	}

	CaptureOriginal(*Mesh);
	RestoreLook(*Mesh);
	AppliedSkin = Wanted;
	if (!Wanted)
	{
		return true;
	}

	// The skin's content is soft and loaded here, once. The mesh first: replacing it resets the material slots
	if (!Wanted->Mesh.IsNull())
	{
		if (USkeletalMesh* NewMesh = Wanted->Mesh.LoadSynchronous())
		{
			Mesh->SetSkeletalMeshAsset(NewMesh);
		}
	}
	if (!Wanted->AnimClass.IsNull())
	{
		if (UClass* NewAnimClass = Wanted->AnimClass.LoadSynchronous())
		{
			Mesh->SetAnimInstanceClass(NewAnimClass);
		}
	}
	for (int32 Slot = 0; Slot < Wanted->Materials.Num(); ++Slot)
	{
		if (!Wanted->Materials[Slot].IsNull() && Slot < Mesh->GetNumMaterials())
		{
			if (UMaterialInterface* NewMaterial = Wanted->Materials[Slot].LoadSynchronous())
			{
				Mesh->SetMaterial(Slot, NewMaterial);
			}
		}
	}
	return true;
}
