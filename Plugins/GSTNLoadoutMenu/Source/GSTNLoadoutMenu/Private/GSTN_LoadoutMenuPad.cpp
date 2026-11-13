// Loadout debug pad.

#include "GSTN_LoadoutMenuPad.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GSTN_LoadoutMenuRules.h"
#include "GSTN_LoadoutMenuSubsystem.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "GSTNLoadoutMenu"

AGSTN_LoadoutMenuPad::AGSTN_LoadoutMenuPad()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	bReplicates = false;
	Label = LOCTEXT("PadLabel", "LOADOUT DEBUG");

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Plate = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Plate"));
	Plate->SetupAttachment(GetRootComponent());
	// The engine's basic cylinder, flattened: a look only, nothing stands or collides on it (the pad reads distances)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Plate->SetStaticMesh(Cylinder.Object);
	}
	Plate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Plate->SetGenerateOverlapEvents(false);
	Plate->SetCanEverAffectNavigation(false);

	Text = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	Text->SetupAttachment(GetRootComponent());
	Text->SetHorizontalAlignment(EHTA_Center);
	Text->SetVerticalAlignment(EVRTA_TextCenter);
	Text->SetTextRenderColor(FColor(255, 200, 40));
}

void AGSTN_LoadoutMenuPad::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// The basic cylinder is 100 cm wide and tall, centred: scale it to the radius, 10 cm thick, its top 5 cm above the floor; the label floats
	// above, facing the actor's forward
	Plate->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, 0.1f));
	Plate->SetRelativeLocation(FVector::ZeroVector);
	Text->SetText(Label);
	Text->SetWorldSize(40.f);
	Text->SetRelativeLocation(FVector(0.f, 0.f, 180.f));
}

void AGSTN_LoadoutMenuPad::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	if (!World || !GEngine)
	{
		return;
	}
	const FVector Centre = GetActorLocation();
	TSet<TWeakObjectPtr<const ULocalPlayer>> Now;
	for (ULocalPlayer* Player : GEngine->GetGamePlayers(World))
	{
		const APlayerController* Controller = Player ? Player->GetPlayerController(World) : nullptr;
		const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		if (!Pawn || !Pawn->IsLocallyControlled())
		{
			continue;
		}
		// The pawn's feet: the bottom of its bounds (the capsule), so the height test does not depend on the capsule's size
		FVector Origin, Extent;
		Pawn->GetActorBounds(true, Origin, Extent);
		const FVector Feet(Origin.X, Origin.Y, Origin.Z - Extent.Z);
		if (FVector::Dist2D(Feet, Centre) > Radius || Feet.Z - Centre.Z > MaxHeight || Feet.Z < Centre.Z - MaxHeight)
		{
			continue;
		}
		Now.Add(Player);
		if (Standing.Contains(Player))
		{
			continue;
		}
		UGSTN_LoadoutMenuSubsystem* Menu = Player->GetSubsystem<UGSTN_LoadoutMenuSubsystem>();
		if (Menu && !Menu->OpenMenu(bDebug))
		{
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 4.f, FColor::Orange, FString::Printf(TEXT("Loadout: %s"), *Menu->GetOpenProblem()));
		}
	}
	Standing = MoveTemp(Now);
}

bool UGSTN_LoadoutMenuPadSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_LoadoutMenuPadSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	const FString LevelPackage = UWorld::RemovePIEPrefix(InWorld.GetOutermost()->GetName());
	for (const FGSTN_LoadoutMenuPadPlacement& Placement : UGSTN_LoadoutMenuRules::Get().Pads)
	{
		if (Placement.Level.ToSoftObjectPath().GetLongPackageName() != LevelPackage)
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Params.ObjectFlags |= RF_Transient;
		Params.bDeferConstruction = true;
		const FTransform Where(FRotator(0.f, Placement.Yaw, 0.f), Placement.Location);
		if (AGSTN_LoadoutMenuPad* Pad = InWorld.SpawnActor<AGSTN_LoadoutMenuPad>(AGSTN_LoadoutMenuPad::StaticClass(), Where, Params))
		{
			Pad->bDebug = Placement.bDebug;
			Pad->FinishSpawning(Where);
		}
	}
}

#undef LOCTEXT_NAMESPACE
