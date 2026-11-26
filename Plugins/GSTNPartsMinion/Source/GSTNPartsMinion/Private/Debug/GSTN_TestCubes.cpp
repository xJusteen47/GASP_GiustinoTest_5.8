// Test cubes: fixed targets of different hardness for playtesting penetration (owner request 2026-11-25).

#include "Debug/GSTN_TestCubes.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Data/GSTN_MinionDefinition.h"
#include "Data/GSTN_PartDefinition.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Data/GSTN_PartsMinionExtension.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GSTN_PartsMinionSubsystem.h"
#include "GSTN_PartsSettings.h"
#include "GSTNPartsMinionModule.h"
#include "HAL/IConsoleManager.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Pipeline/GSTN_PartHitOutcome.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Spawn/GSTN_MinionSpawner.h"
#include "Timeline/GSTN_Pushbox.h"
#include "UObject/ConstructorHelpers.h"

namespace GSTNTestCubes
{
	/** The cube's only hurtbox and the part that names it */
	const FName HurtboxName(TEXT("Cube"));
	const FName PartName(TEXT("Cube"));

	/** Enough that a session of attacks never kills a cube (the store's health is a float; the wire only carries a 5 bit fraction) */
	constexpr float CubeHealth = 1000000.f;

	constexpr int32 MaxHardness = 15;

	/** Green (soft) to red (hard) */
	FLinearColor ColorOfHardness(int32 Hardness)
	{
		const float Alpha = FMath::Clamp(static_cast<float>(Hardness) / MaxHardness, 0.f, 1.f);
		return FLinearColor::LerpUsingHSV(FLinearColor(0.1f, 0.8f, 0.15f), FLinearColor(0.85f, 0.1f, 0.1f), Alpha);
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Subsystem

bool UGSTN_TestCubesSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_TestCubesSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	UGSTN_PartsMinionSubsystem* Bridge = Collection.InitializeDependency<UGSTN_PartsMinionSubsystem>();
	Super::Initialize(Collection);
	if (Bridge)
	{
		OutcomeHandle = Bridge->OnPartOutcomeApplied.AddUObject(this, &UGSTN_TestCubesSubsystem::HandlePartOutcome);
	}
}

void UGSTN_TestCubesSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (UGSTN_PartsMinionSubsystem* Bridge = World->GetSubsystem<UGSTN_PartsMinionSubsystem>())
		{
			Bridge->OnPartOutcomeApplied.Remove(OutcomeHandle);
		}
	}
	OutcomeHandle.Reset();
	Cubes.Reset();
	Super::Deinitialize();
}

UGSTN_MinionDefinition* UGSTN_TestCubesSubsystem::GetOrCreateDefinition(int32 Hardness)
{
	if (TObjectPtr<UGSTN_MinionDefinition>* Found = Definitions.Find(Hardness))
	{
		return *Found;
	}
	UGSTN_PartsMinionSubsystem* Bridge = GetWorld() ? GetWorld()->GetSubsystem<UGSTN_PartsMinionSubsystem>() : nullptr;
	if (!Bridge)
	{
		return nullptr;
	}

	const double Half = AGSTN_TestCubePuppet::CubeSize * 0.5;
	if (!Hurtboxes)
	{
		// One box the size of the cube, from the feet up. The set's own cap keeps any rules that skip the part step from moving the cube
		Hurtboxes = NewObject<UGSTN_HurtboxSetDefinition>(this, NAME_None, RF_Transient);
		FGSTN_Hurtbox& Box = Hurtboxes->Hurtboxes.AddDefaulted_GetRef();
		Box.Name = GSTNTestCubes::HurtboxName;
		Box.Shape.Type = EGSTN_ShapeType::Box;
		Box.Shape.Extent = FVector(Half);
		Box.Shape.Local = FTransform(FVector(0.0, 0.0, Half));
		Hurtboxes->MaxReaction = EGSTN_HitReaction::StaggerInPlace;
	}
	if (!Table)
	{
		// The table the game would use for a set without its own: the settings' default, else the built-in bands. Copied, so the label
		// names the band by the same index the part step reports
		Table = NewObject<UGSTN_PenetrationTable>(this, NAME_None, RF_Transient);
		const UGSTN_PenetrationTable* Settings = UGSTN_PartsSettings::Get().DefaultPenetrationTable.LoadSynchronous();
		Table->Bands = Settings && !Settings->Bands.IsEmpty() ? Settings->Bands : UGSTN_PenetrationTable::MakeDefaultBands();
	}

	// One unbreakable part over the whole cube, every reaction capped to a flinch in place with no knockback
	FGSTN_PartReaction Fixed;
	Fixed.MaxReaction = EGSTN_PartReactionTier::StaggerInPlace;
	Fixed.KnockbackScale = 0.f;

	UGSTN_PartSetDefinition* Set = NewObject<UGSTN_PartSetDefinition>(this, NAME_None, RF_Transient);
	FGSTN_PartDefinition& Part = Set->Parts.AddDefaulted_GetRef();
	Part.Name = GSTNTestCubes::PartName;
	Part.Hurtboxes = { GSTNTestCubes::HurtboxName };
	Part.Hardness = Hardness;
	Part.PartCap = 0.f;
	// The whole hit (after the band) reaches the health pool, so the label's damage is the damage the band let through
	Part.MainTransfer = 1.f;
	Part.Reaction = Fixed;
	Set->BodyReaction = Fixed;
	Set->Table = Table.Get();
	Set->bForMinions = true;
	Set->RebakeWeightCurve();
	PartSets.Add(Set);

	const FString Name = FString::Printf(TEXT("GSTN_TestCube_H%02d"), Hardness);
	UGSTN_MinionDefinition* Definition = NewObject<UGSTN_MinionDefinition>(this, FName(*Name), RF_Transient);
	Definition->Stats.MaxHealth = GSTNTestCubes::CubeHealth;
	Definition->Stats.Weight = 1000.f;
	Definition->Stats.WalkSpeed = 0.f;
	Definition->Stats.RunSpeed = 0.f;
	Definition->Stats.Acceleration = 0.f;
	Definition->Stats.TurnRateDegPerSec = 0.f;
	Definition->Stats.CapsuleRadius = static_cast<float>(Half);
	Definition->Stats.CapsuleHalfHeight = static_cast<float>(Half);
	// A ghost to fighters: never pushed out of place by a body walking into it
	Definition->BodyCollision.Profile.Mode = EGSTN_BodyCollisionMode::None;
	Definition->HurtboxSet = Hurtboxes.Get();
	Definition->PuppetClass = TSoftClassPtr<AActor>(AGSTN_TestCubePuppet::StaticClass());

	// Before the type's first spawn: its wire format (the parts ExtByte) is decided then
	FGSTN_PartsMinionExtension Extension;
	Extension.PartSet = Set;
	Bridge->RegisterDefinition(Definition, Extension);

	Definitions.Add(Hardness, Definition);
	return Definition;
}

int32 UGSTN_TestCubesSubsystem::SpawnCubes(const TArray<int32>& Hardnesses, double Distance, double Spacing)
{
	UWorld* World = GetWorld();
	if (!UGSTN_MinionCombatSubsystem::GetActive(World))
	{
		UE_LOG(LogGSTNPartsMinion, Warning, TEXT("gstn.TestCubes.Spawn: run it on the authority (standalone, listen host or the PIE server)"));
		return 0;
	}
	const APlayerController* Controller = World->GetFirstPlayerController();
	const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
	if (!Pawn)
	{
		UE_LOG(LogGSTNPartsMinion, Warning, TEXT("gstn.TestCubes.Spawn: no player pawn"));
		return 0;
	}
	ClearCubes();

	const FVector Up = Pawn->GetActorUpVector();
	const FVector Forward = FVector::VectorPlaneProject(Pawn->GetActorForwardVector(), Up).GetSafeNormal();
	const FVector Right = FVector::CrossProduct(Up, Forward);
	const FVector Center = Pawn->GetActorLocation() + Forward * Distance;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GSTN_TestCubesFloor), false, Pawn);

	int32 Queued = 0;
	for (int32 Index = 0; Index < Hardnesses.Num(); ++Index)
	{
		const int32 Hardness = FMath::Clamp(Hardnesses[Index], 0, GSTNTestCubes::MaxHardness);
		UGSTN_MinionDefinition* Definition = GetOrCreateDefinition(Hardness);
		if (!Definition)
		{
			continue;
		}
		FVector Location = Center + Right * ((Index - (Hardnesses.Num() - 1) * 0.5) * Spacing);
		// Feet on the floor under the point (1 cm above it, the movement backend's skin); left where it is when there is no floor
		FHitResult Floor;
		if (World->LineTraceSingleByChannel(Floor, Location + Up * 300.0, Location - Up * 1000.0, ECC_WorldStatic, Query))
		{
			Location = Floor.ImpactPoint + Up * 1.0;
		}
		const FRotator Facing = FRotator(0.0, (-Forward).Rotation().Yaw, 0.0);
		FGSTN_MinionSpawnParams Params;
		Params.bPlaySpawnTimeline = false;
		const FGSTN_MinionHandle Handle = UGSTN_MinionSpawner::Spawn(World, Definition, FTransform(Facing, Location), Params);
		if (Handle.IsNull())
		{
			continue;
		}
		FGSTN_TestCubeRecord& Record = Cubes.AddDefaulted_GetRef();
		Record.Handle = Handle;
		Record.Definition = Definition;
		Record.Hardness = Hardness;
		++Queued;
	}
	UE_LOG(LogGSTNPartsMinion, Log, TEXT("TestCubes: %d cubes queued %.0f cm in front of %s"), Queued, Distance, *Pawn->GetName());
	return Queued;
}

void UGSTN_TestCubesSubsystem::ClearCubes()
{
	for (const FGSTN_TestCubeRecord& Record : Cubes)
	{
		UGSTN_MinionSpawner::Despawn(GetWorld(), Record.Handle);
	}
	Cubes.Reset();
}

void UGSTN_TestCubesSubsystem::Report() const
{
	if (Cubes.IsEmpty())
	{
		UE_LOG(LogGSTNPartsMinion, Log, TEXT("TestCubes: none in %s"), *GetNameSafe(GetWorld()));
	}
	for (const FGSTN_TestCubeRecord& Record : Cubes)
	{
		UE_LOG(LogGSTNPartsMinion, Log, TEXT("TestCubes hardness=%d hits=%d ricochets=%d damage=%.1f last='%s'"),
			Record.Hardness, Record.Hits, Record.Ricochets, Record.TotalDamage, *Record.LastHit);
	}
}

const FGSTN_TestCubeRecord* UGSTN_TestCubesSubsystem::FindByDefinition(const UGSTN_MinionDefinition* Definition) const
{
	return Definition ? Cubes.FindByPredicate([Definition](const FGSTN_TestCubeRecord& Record) { return Record.Definition.Get() == Definition; }) : nullptr;
}

FString UGSTN_TestCubesSubsystem::MakeLabel(const FGSTN_TestCubeRecord& Record)
{
	FString Text = FString::Printf(TEXT("Durezza %d"), Record.Hardness);
	if (Record.Hits > 0)
	{
		Text += FString::Printf(TEXT("\n%s\ncolpi %d, rimbalzi %d, danno %.1f"), *Record.LastHit, Record.Hits, Record.Ricochets, Record.TotalDamage);
	}
	return Text;
}

FString UGSTN_TestCubesSubsystem::DescribeBand(int32 BandIndex) const
{
	if (!Table || !Table->Bands.IsValidIndex(BandIndex))
	{
		return TEXT("corpo");
	}
	const FGSTN_PenetrationBand& Band = Table->Bands[BandIndex];
	if (Band.bRicochet)
	{
		return TEXT("RIMBALZO");
	}
	return FString::Printf(TEXT("pen - durezza >= %d: %d%%"), Band.MinDelta, FMath::RoundToInt(Band.MainScale * 100.f));
}

void UGSTN_TestCubesSubsystem::HandlePartOutcome(const FGSTN_MinionHandle& Handle, const FGSTN_PartHitOutcome& Outcome, int32 Frame)
{
	FGSTN_TestCubeRecord* Record = Cubes.FindByPredicate([&Handle](const FGSTN_TestCubeRecord& Each) { return Each.Handle == Handle; });
	if (!Record)
	{
		return;
	}
	const bool bRicochet = Outcome.HasFlag(GSTNPartOutcomeFlags::Ricochet);
	++Record->Hits;
	Record->Ricochets += bRicochet ? 1 : 0;
	Record->TotalDamage += Outcome.MainDelta;
	Record->LastHit = bRicochet ? DescribeBand(Outcome.BandIndex)
		: FString::Printf(TEXT("%s, danno %.1f"), *DescribeBand(Outcome.BandIndex), Outcome.MainDelta);
	UE_LOG(LogGSTNPartsMinion, Log, TEXT("TestCubes hit frame=%d hardness=%d band=%d ricochet=%d damage=%.2f"),
		Frame, Record->Hardness, Outcome.BandIndex, bRicochet ? 1 : 0, Outcome.MainDelta);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Puppet

AGSTN_TestCubePuppet::AGSTN_TestCubePuppet()
{
	// The base puppet ticks nothing; the cube refreshes its colour and label, a few calls per frame for a handful of cubes
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Cube = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cube"));
	Cube->SetupAttachment(GetRootComponent());
	Cube->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Cube->SetGenerateOverlapEvents(false);
	Cube->SetCanEverAffectNavigation(false);
	// The engine cube is 1 m with its pivot at the centre; the puppet's origin is the minion's feet
	Cube->SetRelativeLocation(FVector(0.0, 0.0, CubeSize * 0.5));
	Cube->SetRelativeScale3D(FVector(CubeSize / 100.0));
	if (CubeMesh.Succeeded())
	{
		Cube->SetStaticMesh(CubeMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Cube->SetMaterial(0, ShapeMaterial.Object);
	}

	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	Label->SetupAttachment(GetRootComponent());
	Label->SetRelativeLocation(FVector(0.0, 0.0, CubeSize + 40.0));
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextBottom);
	Label->SetWorldSize(18.f);
	Label->SetTextRenderColor(FColor::White);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AGSTN_TestCubePuppet::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!IsInUse())
	{
		return;
	}
	UWorld* World = GetWorld();
	const UGSTN_TestCubesSubsystem* Cubes = World ? World->GetSubsystem<UGSTN_TestCubesSubsystem>() : nullptr;
	const FGSTN_TestCubeRecord* Record = Cubes ? Cubes->FindByDefinition(GetDefinition()) : nullptr;

	if (ShownDefinition.Get() != GetDefinition())
	{
		ShownDefinition = GetDefinition();
		BaseColor = Record ? GSTNTestCubes::ColorOfHardness(Record->Hardness) : FLinearColor::Gray;
		if (!CubeMaterial)
		{
			CubeMaterial = Cube->CreateDynamicMaterialInstance(0);
		}
		ShownFlash = -1.f;
	}

	// The minion presentation's hit flash (0..1), shown as a blend to white
	const float FlashNow = GetFlash();
	if (CubeMaterial && FlashNow != ShownFlash)
	{
		ShownFlash = FlashNow;
		CubeMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::LerpUsingHSV(BaseColor, FLinearColor::White, FMath::Clamp(FlashNow, 0.f, 1.f)));
	}

	const FString Text = Record ? UGSTN_TestCubesSubsystem::MakeLabel(*Record) : FString(TEXT("?"));
	if (Text != ShownText)
	{
		ShownText = Text;
		Label->SetText(FText::FromString(Text));
	}

	// The label turns to the local camera (yaw only), so it reads from wherever the player stands
	const APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
	if (Controller && Controller->PlayerCameraManager)
	{
		const FVector ToCamera = Controller->PlayerCameraManager->GetCameraLocation() - Label->GetComponentLocation();
		Label->SetWorldRotation(FRotator(0.0, ToCamera.Rotation().Yaw, 0.0));
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Console commands (test tooling, not in Shipping)

#if !UE_BUILD_SHIPPING
namespace GSTNTestCubes
{
	static FAutoConsoleCommandWithWorldAndArgs SpawnCommand(
		TEXT("gstn.TestCubes.Spawn"),
		TEXT("gstn.TestCubes.Spawn [Hardness=0,1,3,6,10,15] [Distance=350] [Spacing=200]: on the authority, fixed cubes to attack, one per hardness ")
		TEXT("(0-15, comma separated), in a row in front of the player. Each cube is labelled with its hardness and the band of the last hit ")
		TEXT("(full, reduced, ricochet). Replaces the cubes already there."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGSTN_TestCubesSubsystem* Cubes = World ? World->GetSubsystem<UGSTN_TestCubesSubsystem>() : nullptr;
			if (!Cubes)
			{
				UE_LOG(LogGSTNPartsMinion, Warning, TEXT("gstn.TestCubes.Spawn: no game world"));
				return;
			}
			TArray<FString> Items;
			(Args.IsValidIndex(0) ? Args[0] : FString(TEXT("0,1,3,6,10,15"))).ParseIntoArray(Items, TEXT(","));
			TArray<int32> Hardnesses;
			for (const FString& Item : Items)
			{
				Hardnesses.Add(FCString::Atoi(*Item.TrimStartAndEnd()));
			}
			const double Distance = Args.IsValidIndex(1) ? FCString::Atod(*Args[1]) : 350.0;
			const double Spacing = Args.IsValidIndex(2) ? FCString::Atod(*Args[2]) : 200.0;
			Cubes->SpawnCubes(Hardnesses, Distance, Spacing);
		}));

	static FAutoConsoleCommandWithWorld ClearCommand(
		TEXT("gstn.TestCubes.Clear"),
		TEXT("Removes the test cubes of this world."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UGSTN_TestCubesSubsystem* Cubes = World ? World->GetSubsystem<UGSTN_TestCubesSubsystem>() : nullptr)
			{
				Cubes->ClearCubes();
			}
		}));

	static FAutoConsoleCommandWithWorld ReportCommand(
		TEXT("gstn.TestCubes.Report"),
		TEXT("Logs one 'TestCubes hardness=.. hits=.. ricochets=.. damage=..' line per test cube."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const UGSTN_TestCubesSubsystem* Cubes = World ? World->GetSubsystem<UGSTN_TestCubesSubsystem>() : nullptr)
			{
				Cubes->Report();
			}
		}));
}
#endif
