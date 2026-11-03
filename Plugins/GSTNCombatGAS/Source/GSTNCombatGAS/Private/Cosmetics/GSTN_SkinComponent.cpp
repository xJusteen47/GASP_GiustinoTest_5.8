// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Cosmetics/GSTN_SkinComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Kits/GSTN_SkinDefinition.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

UGSTN_SkinComponent::UGSTN_SkinComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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
	}
}

void UGSTN_SkinComponent::HandleLoadoutApplied(UGSTN_LoadoutComponent* Component, int32 Revision)
{
	ApplySkin(Component ? Component->GetResolvedSkin() : nullptr);
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
