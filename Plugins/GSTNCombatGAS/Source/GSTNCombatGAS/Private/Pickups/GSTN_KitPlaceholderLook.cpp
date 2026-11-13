// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Pickups/GSTN_KitPlaceholderLook.h"

#include "AnimationRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Cosmetics/GSTN_EquipmentVisual.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Loadout/GSTN_LoadoutComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

namespace GSTNKitPlaceholderLook
{
	static TAutoConsoleVariable<bool> CVarEnabled(TEXT("gstn.Combat.KitPlaceholderLook"), true,
		TEXT("Coloured box on the back of a player whose kit has no skin (stand-in look of the kit, presentation only)"));

	// Engine content, always present: the basic cube and the material with a Color parameter that the editor's shapes use
	static const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	static const TCHAR* MaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	// Upper back, relative to the pawn's root (the capsule centre, facing +X): 30 x 40 x 50 cm. This is the place in the body's rest pose;
	// the box then rides the back bone, so it follows the animation
	static const FVector BackOffset(-28.f, 0.f, 35.f);
	static const FVector BackScale(0.15f, 0.4f, 0.5f);

	// UE5 mannequin upper back (GASP characters and their visual overrides share the bone names)
	static const FName BackBone(TEXT("spine_05"));

	/** The skeletal mesh that draws the pawn: the one the box must follow */
	USkeletalMeshComponent* FindBody(const APawn& Pawn)
	{
		const ACharacter* Character = Cast<ACharacter>(&Pawn);
		USkeletalMeshComponent* Preferred = Character ? Character->GetMesh() : Pawn.FindComponentByClass<USkeletalMeshComponent>();
		return FGSTN_EquipmentVisualUtils::FindShownBodyMesh(Pawn, Preferred);
	}

	/**
	 * Puts Box on Body's back bone where BackOffset puts it relative to the pawn's root in the rest pose. The bone's rest transform comes
	 * from the reference skeleton, not the current pose, so the place is the same whatever the pawn is doing when the box is attached,
	 * and no bone axis convention is assumed. Without the bone (another skeleton), the box stays on the root as before
	 */
	void AttachToBack(UStaticMeshComponent& Box, const APawn& Pawn, USkeletalMeshComponent* Body)
	{
		USceneComponent* Root = Pawn.GetRootComponent();
		const USkeletalMesh* Asset = Body ? Body->GetSkeletalMeshAsset() : nullptr;
		const int32 Bone = Asset ? Asset->GetRefSkeleton().FindBoneIndex(BackBone) : INDEX_NONE;
		if (!Root || Bone == INDEX_NONE)
		{
			Box.AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
			Box.SetRelativeTransform(FTransform(FQuat::Identity, BackOffset, BackScale));
			return;
		}
		const FTransform Wanted = FTransform(FQuat::Identity, BackOffset, BackScale) * Root->GetComponentTransform();
		const FTransform BoneAtRest = FAnimationRuntime::GetComponentSpaceTransformRefPose(Asset->GetRefSkeleton(), Bone) * Body->GetComponentTransform();
		Box.AttachToComponent(Body, FAttachmentTransformRules::KeepRelativeTransform, BackBone);
		Box.SetRelativeTransform(Wanted.GetRelativeTransform(BoneAtRest));
	}
}

FLinearColor UGSTN_KitPlaceholderLookSubsystem::ColourForKit(const FPrimaryAssetId& Kit)
{
	// FCrc on the plain string, not GetTypeHash(FName): an FName's hash depends on the name table of the process
	const uint32 Hash = FCrc::StrCrc32(*Kit.PrimaryAssetName.ToString().ToLower());
	// Golden-ratio spread so kits with close names still get far apart hues
	const float Hue = FMath::Fractional(static_cast<float>(Hash % 1024u) * 0.618034f);
	return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue * 255.f), 210, 255);
}

void UGSTN_KitPlaceholderLookSubsystem::ApplyPlaceholder(UStaticMeshComponent* Mesh, const FPrimaryAssetId& Kit)
{
	if (!Mesh)
	{
		return;
	}
	if (!Kit.IsValid())
	{
		Mesh->SetVisibility(false);
		return;
	}
	if (!Mesh->GetStaticMesh())
	{
		Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, GSTNKitPlaceholderLook::CubePath));
	}
	UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Mesh->GetMaterial(0));
	if (!Material)
	{
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, GSTNKitPlaceholderLook::MaterialPath))
		{
			Material = UMaterialInstanceDynamic::Create(Base, Mesh);
			Mesh->SetMaterial(0, Material);
		}
	}
	if (Material)
	{
		Material->SetVectorParameterValue(TEXT("Color"), ColourForKit(Kit));
	}
	Mesh->SetVisibility(true);
}

bool UGSTN_KitPlaceholderLookSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_KitPlaceholderLookSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// Nobody looks at a dedicated server
	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	InWorld.GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateUObject(this, &ThisClass::Refresh), 0.25f, true);
}

void UGSTN_KitPlaceholderLookSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Timer);
	}
	Markers.Reset();
	Super::Deinitialize();
}

void UGSTN_KitPlaceholderLookSubsystem::Refresh()
{
	for (auto It = Markers.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Value().Component.IsValid())
		{
			It.RemoveCurrent();
		}
	}

	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}
	const bool bEnabled = GSTNKitPlaceholderLook::CVarEnabled.GetValueOnGameThread();
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		APawn* Pawn = PlayerState ? PlayerState->GetPawn() : nullptr;
		if (!Pawn || !Pawn->GetRootComponent())
		{
			continue;
		}
		const UGSTN_LoadoutComponent* Loadout = PlayerState->FindComponentByClass<UGSTN_LoadoutComponent>();
		if (!Loadout)
		{
			Loadout = Pawn->FindComponentByClass<UGSTN_LoadoutComponent>();
		}
		const bool bShow = bEnabled && Loadout && Loadout->HasLoadout() && !Loadout->GetResolvedSkin();
		const FPrimaryAssetId Kit = bShow ? Loadout->GetApplied().PrimaryKit : FPrimaryAssetId();

		FMarker* Marker = Markers.Find(Pawn);
		if (!Marker)
		{
			if (!Kit.IsValid())
			{
				continue;
			}
			UStaticMeshComponent* Box = NewObject<UStaticMeshComponent>(Pawn, TEXT("GSTN_KitPlaceholderLook"), RF_Transient);
			Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Box->SetGenerateOverlapEvents(false);
			Box->SetupAttachment(Pawn->GetRootComponent());
			Box->RegisterComponent();
			Marker = &Markers.Add(Pawn, FMarker{Box, FPrimaryAssetId()});
		}
		// The body that draws the pawn can change in play (GASP's visual override spawns its child actor late, or is switched)
		UStaticMeshComponent* Box = Marker->Component.Get();
		USkeletalMeshComponent* Body = GSTNKitPlaceholderLook::FindBody(*Pawn);
		if (Box && (!Marker->Body.IsValid() || Marker->Body.Get() != Body))
		{
			GSTNKitPlaceholderLook::AttachToBack(*Box, *Pawn, Body);
			Marker->Body = Body;
		}
		if (Marker->Kit != Kit)
		{
			Marker->Kit = Kit;
			ApplyPlaceholder(Marker->Component.Get(), Kit);
		}
	}
}
