#include "Animation/GSTN_GreyboxSubsystem.h"

#include "Animation/AnimInstance.h"
#include "Animation/GSTN_LocoBaseSubsystem.h"
#include "AnimationRuntime.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerState.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Pickups/GSTN_KitPlaceholderLook.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNGreybox, Log, All);

namespace GSTNGreybox
{
	static TAutoConsoleVariable<int32> CVarGreybox(
		TEXT("gstn.Greybox"), 0,
		TEXT("Greybox capsule in place of the character mesh (judge the movement feel without final animations). 0 = off, 1 = capsule ")
		TEXT("with a nose that copies the hidden driver's pose and squashes / leans. Hitboxes unchanged. Pair with gstn.Loco.Base 2."),
		ECVF_Default);

	static TAutoConsoleVariable<FString> CVarSwordKits(
		TEXT("gstn.Greybox.SwordKits"), TEXT("Overlord"),
		TEXT("Kits whose asset name contains one of these comma-separated parts carry the greatsword on the greybox capsule's back."),
		ECVF_Default);

	static const FName CapsuleComponentName(TEXT("GSTN_GreyboxCapsule"));
	static const FName PelvisBone(TEXT("pelvis"));
}

bool UGSTN_GreyboxSubsystem::IsEnabled()
{
	return GSTNGreybox::CVarGreybox.GetValueOnGameThread() != 0;
}

bool UGSTN_GreyboxSubsystem::KitCarriesSword(const FString& KitName, const FString& SwordKits)
{
	TArray<FString> Parts;
	SwordKits.ParseIntoArray(Parts, TEXT(","), true);
	for (FString& Part : Parts)
	{
		Part.TrimStartAndEndInline();
		if (!Part.IsEmpty() && KitName.Contains(Part, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return false;
}

bool UGSTN_GreyboxSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && World->IsGameWorld() && Super::ShouldCreateSubsystem(Outer);
}

void UGSTN_GreyboxSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	SpawnHandle = InWorld.AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateUObject(this, &UGSTN_GreyboxSubsystem::HandleActorSpawned));
	for (TActorIterator<APawn> It(&InWorld); It; ++It)
	{
		HandleActorSpawned(*It);
	}
}

void UGSTN_GreyboxSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(SpawnHandle);
	}
	Super::Deinitialize();
}

void UGSTN_GreyboxSubsystem::HandleActorSpawned(AActor* Actor)
{
	if (!Cast<APawn>(Actor))
	{
		return;
	}
	TInlineComponentArray<USkeletalMeshComponent*> ActorMeshes(Actor);
	for (USkeletalMeshComponent* Mesh : ActorMeshes)
	{
		if (Mesh->GetFName() != GSTNGreybox::CapsuleComponentName)
		{
			Meshes.AddUnique(Mesh);
		}
	}
}

bool UGSTN_GreyboxSubsystem::IsDriverMesh(const USkeletalMeshComponent* Mesh) const
{
	// The GASP character mesh runs one of the locomotion bases; ragdolls, NPC props and skin parts (leader pose followers) do not
	const UClass* AnimClass = Mesh->AnimClass.Get(); // GetAnimClass() is non-const in 5.9; the property is public
	if (!AnimClass)
	{
		return false;
	}
	if (UGSTN_LocoBaseSubsystem::IsCurrentBaseClass(AnimClass))
	{
		return true;
	}
	const FString Path = AnimClass->GetPathName();
	return Path == UGSTN_LocoBaseSubsystem::StockBaseClassPath || Path == UGSTN_LocoBaseSubsystem::SimpleBaseClassPath;
}

bool UGSTN_GreyboxSubsystem::LoadAssets()
{
	if ((!CapsuleMesh.IsValid() || !CapsuleAnimClass.IsValid()) && !bLoadFailed)
	{
		CapsuleMesh = LoadObject<USkeletalMesh>(nullptr, CapsuleMeshPath);
		CapsuleAnimClass = LoadClass<UAnimInstance>(nullptr, CapsuleAnimClassPath);
		bLoadFailed = !CapsuleMesh.IsValid() || !CapsuleAnimClass.IsValid();
		// Optional: without them the capsule simply shows no kit look (older asset sets)
		CollarMesh = LoadObject<UStaticMesh>(nullptr, CollarMeshPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
		SwordMesh = LoadObject<UStaticMesh>(nullptr, SwordMeshPath, nullptr, LOAD_Quiet | LOAD_NoWarn);
		if (bLoadFailed)
		{
			UE_LOG(LogGSTNGreybox, Warning, TEXT("gstn.Greybox: cannot load %s or %s (run Tools/GAS/greybox/setup_greybox_assets.py)"),
				CapsuleMeshPath, CapsuleAnimClassPath);
		}
	}
	return !bLoadFailed;
}

void UGSTN_GreyboxSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const bool bEnabled = IsEnabled();
	if (!bEnabled && Drivers.IsEmpty())
	{
		return;
	}
	for (int32 Index = Meshes.Num() - 1; Index >= 0; --Index)
	{
		USkeletalMeshComponent* Mesh = Meshes[Index].Get();
		if (!Mesh)
		{
			Drivers.Remove(Meshes[Index]);
			Meshes.RemoveAtSwap(Index);
			continue;
		}
		FDriverState* State = Drivers.Find(Mesh);
		if (bEnabled && !State && IsDriverMesh(Mesh))
		{
			Enable(Mesh);
		}
		else if (!bEnabled && State)
		{
			Disable(Mesh, *State);
			Drivers.Remove(Mesh);
		}
		else if (bEnabled && State)
		{
			UpdateKitLook(*Mesh, *State);
			// Something else (a skin swap, a loadout) may show the driver or attach new visuals again: keep them hidden
			if (Mesh->bHiddenInGame == false)
			{
				Mesh->SetHiddenInGame(true, false);
			}
			for (USceneComponent* Child : Mesh->GetAttachChildren())
			{
				if (Child && Child != State->Capsule.Get() && Cast<UPrimitiveComponent>(Child) && !Child->bHiddenInGame)
				{
					State->HiddenChildren.Emplace(Child, false);
					Child->SetHiddenInGame(true, true);
				}
			}
		}
	}
}

void UGSTN_GreyboxSubsystem::Enable(USkeletalMeshComponent* Driver)
{
	if (!LoadAssets())
	{
		return;
	}
	AActor* Owner = Driver->GetOwner();
	USkeletalMeshComponent* Capsule = NewObject<USkeletalMeshComponent>(Owner, GSTNGreybox::CapsuleComponentName, RF_Transient);
	Capsule->SetSkeletalMesh(CapsuleMesh.Get());
	Capsule->SetAnimInstanceClass(CapsuleAnimClass.Get());
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Capsule->SetGenerateOverlapEvents(false);
	Capsule->SetupAttachment(Driver);
	Capsule->RegisterComponent();
	// Tick after the driver so Copy Pose From Mesh reads this frame's pose
	Capsule->AddTickPrerequisiteComponent(Driver);

	FDriverState State;
	State.Capsule = Capsule;
	State.Collar = AddAccessory(*Capsule, CollarMesh.Get(), TEXT("GSTN_GreyboxCollar"));
	State.Sword = AddAccessory(*Capsule, SwordMesh.Get(), TEXT("GSTN_GreyboxSword"));
	State.TickOption = static_cast<uint8>(Driver->VisibilityBasedAnimTickOption);
	State.bDriverWasHidden = Driver->bHiddenInGame;
	for (USceneComponent* Child : Driver->GetAttachChildren())
	{
		if (Child && Child != Capsule && Cast<UPrimitiveComponent>(Child))
		{
			State.HiddenChildren.Emplace(Child, Child->bHiddenInGame != 0); // bitfield: no reference binds to it
			Child->SetHiddenInGame(true, true);
		}
	}
	// Skin parts that follow the driver through leader pose may hang anywhere on the actor: hide them too
	TInlineComponentArray<USkinnedMeshComponent*> Skinned(Owner);
	for (USkinnedMeshComponent* Part : Skinned)
	{
		if (Part && Part != Capsule && Part != Driver && Part->LeaderPoseComponent.Get() == Driver && !State.HiddenChildren.ContainsByPredicate(
			[Part](const TPair<TWeakObjectPtr<USceneComponent>, bool>& Entry) { return Entry.Key.Get() == Part; }))
		{
			State.HiddenChildren.Emplace(Part, Part->bHiddenInGame != 0);
			Part->SetHiddenInGame(true, false);
		}
	}
	// Hidden but still animating and refreshing bones: the capsule copies them, the hitboxes read them
	Driver->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Driver->SetHiddenInGame(true, false);
	Drivers.Add(Driver, MoveTemp(State));
	UE_LOG(LogGSTNGreybox, Log, TEXT("gstn.Greybox=1: capsule on %s"), *GetNameSafe(Owner));
}

void UGSTN_GreyboxSubsystem::Disable(USkeletalMeshComponent* Driver, FDriverState& State)
{
	for (UStaticMeshComponent* Accessory : {State.Collar.Get(), State.Sword.Get()})
	{
		if (Accessory)
		{
			Accessory->DestroyComponent();
		}
	}
	if (USkeletalMeshComponent* Capsule = State.Capsule.Get())
	{
		Capsule->DestroyComponent();
	}
	for (const TPair<TWeakObjectPtr<USceneComponent>, bool>& Child : State.HiddenChildren)
	{
		if (USceneComponent* Component = Child.Key.Get())
		{
			Component->SetHiddenInGame(Child.Value, true);
		}
	}
	Driver->VisibilityBasedAnimTickOption = static_cast<EVisibilityBasedAnimTickOption>(State.TickOption);
	Driver->SetHiddenInGame(State.bDriverWasHidden, false);
	UE_LOG(LogGSTNGreybox, Log, TEXT("gstn.Greybox=0: %s restored"), *GetNameSafe(Driver->GetOwner()));
}

UStaticMeshComponent* UGSTN_GreyboxSubsystem::AddAccessory(USkeletalMeshComponent& Capsule, UStaticMesh* Mesh, FName Name)
{
	const USkeletalMesh* CapsuleAsset = Capsule.GetSkeletalMeshAsset();
	const int32 Pelvis = CapsuleAsset ? CapsuleAsset->GetRefSkeleton().FindBoneIndex(GSTNGreybox::PelvisBone) : INDEX_NONE;
	if (!Mesh || Pelvis == INDEX_NONE)
	{
		return nullptr;
	}
	UStaticMeshComponent* Accessory = NewObject<UStaticMeshComponent>(Capsule.GetOwner(), Name, RF_Transient);
	Accessory->SetStaticMesh(Mesh);
	Accessory->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Accessory->SetGenerateOverlapEvents(false);
	Accessory->SetHiddenInGame(true);
	// Modelled in the capsule's component space: undo the pelvis's reference transform so it sits where it was modelled and then rides
	// the pelvis (the capsule body is rigid on it)
	Accessory->SetupAttachment(&Capsule, GSTNGreybox::PelvisBone);
	Accessory->SetRelativeTransform(FAnimationRuntime::GetComponentSpaceTransformRefPose(CapsuleAsset->GetRefSkeleton(), Pelvis).Inverse());
	Accessory->RegisterComponent();
	return Accessory;
}

void UGSTN_GreyboxSubsystem::UpdateKitLook(USkeletalMeshComponent& Driver, FDriverState& State)
{
	const APawn* Pawn = Cast<APawn>(Driver.GetOwner());
	const APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
	const UGSTN_LoadoutComponent* Loadout = PlayerState ? PlayerState->FindComponentByClass<UGSTN_LoadoutComponent>() : nullptr;
	if (!Loadout && Pawn)
	{
		Loadout = Pawn->FindComponentByClass<UGSTN_LoadoutComponent>();
	}
	const FPrimaryAssetId Kit = Loadout && Loadout->HasLoadout() ? Loadout->GetApplied().PrimaryKit : FPrimaryAssetId();
	const bool bHasKit = Kit.IsValid();
	if (State.ShownKit == Kit && State.bKitShown == bHasKit)
	{
		return;
	}
	State.ShownKit = Kit;
	State.bKitShown = bHasKit;
	if (UStaticMeshComponent* Collar = State.Collar.Get())
	{
		Collar->SetHiddenInGame(!bHasKit);
		if (bHasKit)
		{
			// Same colour as the kit's box on the ground and on a back: the collar is that box's greybox look
			if (UMaterialInstanceDynamic* Material = Collar->CreateAndSetMaterialInstanceDynamic(0))
			{
				Material->SetVectorParameterValue(TEXT("Color"), UGSTN_KitPlaceholderLookSubsystem::ColourForKit(Kit));
			}
		}
	}
	if (UStaticMeshComponent* Sword = State.Sword.Get())
	{
		Sword->SetHiddenInGame(!bHasKit || !KitCarriesSword(Kit.PrimaryAssetName.ToString(), GSTNGreybox::CVarSwordKits.GetValueOnGameThread()));
	}
	UE_LOG(LogGSTNGreybox, Log, TEXT("gstn.Greybox: %s kit look %s"), *GetNameSafe(Pawn), *Kit.ToString());
}

TStatId UGSTN_GreyboxSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTN_GreyboxSubsystem, STATGROUP_Tickables);
}
