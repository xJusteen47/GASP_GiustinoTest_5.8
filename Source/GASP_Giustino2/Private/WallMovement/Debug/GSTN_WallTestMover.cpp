// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/Debug/GSTN_WallTestMover.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "MoverComponent.h"
#include "UObject/ConstructorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallTestMover)

AGSTN_WallTestMover::AGSTN_WallTestMover()
{
	PrimaryActorTick.bCanEverTick = true;
	// Before the physics step, so the step sees the new pose and derives the kinematic velocity from it
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Mesh->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}
}

double AGSTN_WallTestMover::StepTimeOffset(double Current, double Target, double DeltaSeconds, double MaxSlewPerSecond, double SnapThreshold)
{
	const double Error = Target - Current;
	if (FMath::Abs(Error) > SnapThreshold)
	{
		return Target;
	}
	const double MaxStep = FMath::Max(MaxSlewPerSecond, 0.0) * FMath::Max(DeltaSeconds, 0.0);
	return Current + FMath::Clamp(Error, -MaxStep, MaxStep);
}

double AGSTN_WallTestMover::SimPoseTimeSeconds(double BaseSimTimeMs, double StepMs)
{
	return (BaseSimTimeMs + StepMs) * 0.001;
}

double AGSTN_WallTestMover::GetPoseTimeSeconds(float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	const double LocalTime = World->GetTimeSeconds();
	const AGameStateBase* GameState = World->GetGameState();
	// Without a game state (a plain test world, a map that has none yet) the local clock is all there is
	const double Target = GameState ? GameState->GetServerWorldTimeSeconds() - LocalTime : 0.0;
	TimeOffset = bHasTimeOffset ? StepTimeOffset(TimeOffset, Target, DeltaSeconds, MaxClockSlew, ClockSnapThreshold) : Target;
	bHasTimeOffset = GameState != nullptr;
	return LocalTime + TimeOffset;
}

double AGSTN_WallTestMover::TriangleWave(double TimeSeconds, double Slope, double Amplitude)
{
	if (Slope <= 0.0 || Amplitude <= 0.0)
	{
		return 0.0;
	}
	// One period is 4 * Amplitude / Slope: up to +A, down to -A, back to 0
	const double Period = 4.0 * Amplitude / Slope;
	const double Phase = FMath::Fmod(TimeSeconds, Period) / Period;
	const double P = Phase < 0.0 ? Phase + 1.0 : Phase;
	if (P < 0.25)
	{
		return P * 4.0 * Amplitude;
	}
	if (P < 0.75)
	{
		return (0.5 - P) * 4.0 * Amplitude;
	}
	return (P - 1.0) * 4.0 * Amplitude;
}

void AGSTN_WallTestMover::ComputePose(double TimeSeconds, const FVector& InBaseLocation, const FRotator& InBaseRotation, const FVector& TravelAxisWorld, float InTranslationSpeed,
	float InTravelDistance, float InYawRateDeg, float InYawRangeDeg, FVector& OutLocation, FRotator& OutRotation)
{
	OutLocation = InBaseLocation + TravelAxisWorld.GetSafeNormal() * TriangleWave(TimeSeconds, InTranslationSpeed, InTravelDistance);
	OutRotation = InBaseRotation;
	OutRotation.Yaw += TriangleWave(TimeSeconds, InYawRateDeg, InYawRangeDeg);
}

void AGSTN_WallTestMover::BeginPlay()
{
	Super::BeginPlay();
	BaseLocation = GetActorLocation();
	BaseRotation = GetActorRotation();
}

void AGSTN_WallTestMover::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TWeakObjectPtr<UMoverComponent>& Mover : BoundMovers)
	{
		if (UMoverComponent* Component = Mover.Get())
		{
			Component->OnPreSimulationTick.RemoveDynamic(this, &AGSTN_WallTestMover::OnMoverPreSimulationTick);
		}
	}
	BoundMovers.Reset();
	Super::EndPlay(EndPlayReason);
}

void AGSTN_WallTestMover::BindToMovers()
{
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		UMoverComponent* Component = It->FindComponentByClass<UMoverComponent>();
		if (Component && !BoundMovers.Contains(Component))
		{
			Component->OnPreSimulationTick.AddUniqueDynamic(this, &AGSTN_WallTestMover::OnMoverPreSimulationTick);
			BoundMovers.Add(Component);
		}
	}
}

void AGSTN_WallTestMover::ApplyPoseAt(double TimeSeconds, ETeleportType Teleport)
{
	FVector Location;
	FRotator Rotation;
	ComputePose(TimeSeconds, BaseLocation, BaseRotation, BaseRotation.RotateVector(TravelAxis), TranslationSpeed, TravelDistance, YawRate, YawRange, Location, Rotation);
	// No sweep: a kinematic body moved this way gets a velocity from the physics step (None), or is placed at once (TeleportPhysics)
	SetActorLocationAndRotation(Location, Rotation, false, nullptr, Teleport);
}

void AGSTN_WallTestMover::OnMoverPreSimulationTick(const FMoverTimeStep& TimeStep, const FMoverInputCmdContext& InputCmd)
{
	if (TimeStep.BaseSimTimeMs < 0.0)
	{
		return;
	}
	LastSimDrivenTimeSeconds = GetWorld()->GetTimeSeconds();
	ApplyPoseAt(SimPoseTimeSeconds(TimeStep.BaseSimTimeMs, TimeStep.StepMs), ETeleportType::TeleportPhysics);
}

void AGSTN_WallTestMover::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const double Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextBindScanSeconds)
	{
		BindToMovers();
		NextBindScanSeconds = Now + 0.5;
	}

	// A Mover simulation that ticked a moment ago owns the pose; the clock is for when none does
	constexpr double SimOwnsPoseSeconds = 0.25;
	if (Now - LastSimDrivenTimeSeconds <= SimOwnsPoseSeconds)
	{
		return;
	}
	ApplyPoseAt(GetPoseTimeSeconds(DeltaSeconds), ETeleportType::None);
}
