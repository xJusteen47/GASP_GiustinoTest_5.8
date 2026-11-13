// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Pickups/GSTN_KitPlaceholderLook.h"

#include "Components/StaticMeshComponent.h"
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

	// Upper back, relative to the pawn's root (the capsule centre, facing +X): 30 x 40 x 50 cm
	static const FVector BackOffset(-28.f, 0.f, 35.f);
	static const FVector BackScale(0.15f, 0.4f, 0.5f);
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
			Box->SetRelativeLocation(GSTNKitPlaceholderLook::BackOffset);
			Box->SetRelativeScale3D(GSTNKitPlaceholderLook::BackScale);
			Box->RegisterComponent();
			Marker = &Markers.Add(Pawn, FMarker{Box, FPrimaryAssetId()});
		}
		if (Marker->Kit != Kit)
		{
			Marker->Kit = Kit;
			ApplyPlaceholder(Marker->Component.Get(), Kit);
		}
	}
}
