#include "GSTN_CameraSection.h"

#include "Components/BrushComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"
#include "GSTN_CameraMath.h"
#include "GSTN_CameraSettings.h"
#include "PhysicsEngine/BodySetup.h"

AGSTN_CameraSection::AGSTN_CameraSection()
{
	// A section is local presentation data: nothing about it replicates, and it never needs to tick
	bReplicates = false;
	SetCanBeDamaged(false);
	PrimaryActorTick.bCanEverTick = false;

	Rail = CreateDefaultSubobject<USplineComponent>(TEXT("Rail"));
	Rail->SetupAttachment(RootComponent);
	Rail->SetMobility(EComponentMobility::Static);
	Rail->bHiddenInGame = true;
	Rail->ClearSplinePoints(true);
}

bool AGSTN_CameraSection::ContainsPoint(const FVector& Point) const
{
	const UBrushComponent* BrushComp = GetBrushComponent();
	if (BrushComp && BrushComp->BrushBodySetup)
	{
		return EncompassesPoint(Point);
	}
	// No brush geometry (a section made in code): an oriented box around the actor
	const FVector Local = GetActorTransform().InverseTransformPositionNoScale(Point);
	return FMath::Abs(Local.X) <= FallbackExtent.X && FMath::Abs(Local.Y) <= FallbackExtent.Y && FMath::Abs(Local.Z) <= FallbackExtent.Z;
}

double AGSTN_CameraSection::GetVolumeSize() const
{
	const UBrushComponent* BrushComp = GetBrushComponent();
	if (BrushComp && BrushComp->BrushBodySetup)
	{
		return BrushComp->Bounds.BoxExtent.X * BrushComp->Bounds.BoxExtent.Y * BrushComp->Bounds.BoxExtent.Z;
	}
	return FallbackExtent.X * FallbackExtent.Y * FallbackExtent.Z;
}

bool AGSTN_CameraSection::GetWorldBounds(FBox& OutBox) const
{
	const UBrushComponent* BrushComp = GetBrushComponent();
	if (BrushComp && BrushComp->BrushBodySetup)
	{
		OutBox = BrushComp->Bounds.GetBox();
		return OutBox.IsValid != 0;
	}
	OutBox = FBox(ForceInit);
	const FTransform Transform = GetActorTransform();
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Local((Corner & 1) ? FallbackExtent.X : -FallbackExtent.X, (Corner & 2) ? FallbackExtent.Y : -FallbackExtent.Y, (Corner & 4) ? FallbackExtent.Z : -FallbackExtent.Z);
		OutBox += Transform.TransformPositionNoScale(Local);
	}
	return OutBox.IsValid != 0;
}

float AGSTN_CameraSection::GetBaseHeading(const FGSTN_CameraFrame& Frame) const
{
	const FVector Flat = FGSTN_CameraMath::ProjectOnUpPlane(GetActorForwardVector(), Frame.Up);
	return Flat.IsNearlyZero() ? 0.f : FGSTN_CameraMath::DirectionToHeading(Frame, Flat);
}

void AGSTN_CameraSection::GetRailPoints(TArray<FVector>& OutPoints, int32 SamplesPerSegment) const
{
	OutPoints.Reset();
	if (!Rail || Rail->GetNumberOfSplinePoints() < 2)
	{
		return;
	}
	const int32 Segments = Rail->IsClosedLoop() ? Rail->GetNumberOfSplinePoints() : Rail->GetNumberOfSplinePoints() - 1;
	const int32 Samples = FMath::Max(1, SamplesPerSegment);
	for (int32 Segment = 0; Segment < Segments; ++Segment)
	{
		for (int32 Sample = 0; Sample < Samples; ++Sample)
		{
			const float Key = Segment + static_cast<float>(Sample) / Samples;
			OutPoints.Add(Rail->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World));
		}
	}
	const float EndKey = static_cast<float>(Segments);
	OutPoints.Add(Rail->GetLocationAtSplineInputKey(EndKey, ESplineCoordinateSpace::World));
}

void AGSTN_CameraSection::BeginPlay()
{
	Super::BeginPlay();
	if (UGSTN_CameraSectionSubsystem* Subsystem = UGSTN_CameraSectionSubsystem::Get(GetWorld()))
	{
		Subsystem->RegisterSection(this);
	}
}

void AGSTN_CameraSection::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGSTN_CameraSectionSubsystem* Subsystem = UGSTN_CameraSectionSubsystem::Get(GetWorld()))
	{
		Subsystem->UnregisterSection(this);
	}
	Super::EndPlay(EndPlayReason);
}

bool UGSTN_CameraSectionSubsystem::ShouldCreateFor(ENetMode NetMode, bool bGameWorld, bool bEnabled)
{
	return bEnabled && bGameWorld && NetMode != NM_DedicatedServer;
}

UGSTN_CameraSectionSubsystem* UGSTN_CameraSectionSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UGSTN_CameraSectionSubsystem>() : nullptr;
}

bool UGSTN_CameraSectionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && World && ShouldCreateFor(World->GetNetMode(), World->IsGameWorld(), UGSTN_CameraSettings::Get().bEnabled);
}

void UGSTN_CameraSectionSubsystem::RegisterSection(AGSTN_CameraSection* Section)
{
	if (Section)
	{
		Sections.AddUnique(Section);
	}
}

void UGSTN_CameraSectionSubsystem::UnregisterSection(AGSTN_CameraSection* Section)
{
	Sections.Remove(Section);
}

void UGSTN_CameraSectionSubsystem::GatherContaining(const FVector& Point, TArray<AGSTN_CameraSection*>& OutSections) const
{
	OutSections.Reset();
	for (const TWeakObjectPtr<AGSTN_CameraSection>& Weak : Sections)
	{
		AGSTN_CameraSection* Section = Weak.Get();
		if (Section && Section->ContainsPoint(Point))
		{
			OutSections.Add(Section);
		}
	}
}
