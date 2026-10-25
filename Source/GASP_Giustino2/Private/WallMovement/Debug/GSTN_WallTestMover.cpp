// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/Debug/GSTN_WallTestMover.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
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

void AGSTN_WallTestMover::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FVector Location;
	FRotator Rotation;
	ComputePose(GetWorld()->GetTimeSeconds(), BaseLocation, BaseRotation, BaseRotation.RotateVector(TravelAxis), TranslationSpeed, TravelDistance, YawRate, YawRange, Location, Rotation);
	// No sweep, no teleport: a kinematic body moved this way gets a velocity from the physics step
	SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::None);
}
