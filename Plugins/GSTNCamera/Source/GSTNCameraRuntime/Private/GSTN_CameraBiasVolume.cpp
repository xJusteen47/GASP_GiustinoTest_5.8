#include "GSTN_CameraBiasVolume.h"

#include "Components/BrushComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GSTN_CameraMath.h"
#include "GSTN_CameraSection.h"
#include "PhysicsEngine/BodySetup.h"

AGSTN_CameraBiasVolume::AGSTN_CameraBiasVolume()
{
	bReplicates = false;
	SetCanBeDamaged(false);
	PrimaryActorTick.bCanEverTick = false;

	Focus = CreateDefaultSubobject<USceneComponent>(TEXT("Focus"));
	Focus->SetupAttachment(RootComponent);
	Focus->SetRelativeLocation(FVector(1000.0, 0.0, 0.0));
}

float AGSTN_CameraBiasVolume::GetSpatialWeight(const FVector& Point) const
{
	const FTransform Transform = GetActorTransform();
	const UBrushComponent* BrushComp = GetBrushComponent();
	if (BrushComp && BrushComp->BrushBodySetup)
	{
		if (!EncompassesPoint(Point))
		{
			return 0.f;
		}
		// Depth from the local bounds box of the brush (exact for boxes, an approximation for other shapes)
		const FBox Local = BrushComp->CalcBounds(FTransform::Identity).GetBox();
		const FVector Scale = Transform.GetScale3D().GetAbs();
		const FVector LocalPoint = Transform.InverseTransformPositionNoScale(Point) - Local.GetCenter() * Scale;
		return FMath::Max(KINDA_SMALL_NUMBER, FGSTN_CameraMath::BoxFalloffWeight(LocalPoint, Local.GetExtent() * Scale, Spec.FalloffCm));
	}
	return FGSTN_CameraMath::BoxFalloffWeight(Transform.InverseTransformPositionNoScale(Point), FallbackExtent, Spec.FalloffCm);
}

FVector AGSTN_CameraBiasVolume::GetFocusLocation() const
{
	if (const AActor* Actor = Spec.FocusActor.Get())
	{
		return Actor->GetActorLocation();
	}
	return Focus ? Focus->GetComponentLocation() : GetActorLocation();
}

float AGSTN_CameraBiasVolume::GetBaseHeading(const FGSTN_CameraFrame& Frame) const
{
	const FVector Flat = FGSTN_CameraMath::ProjectOnUpPlane(GetActorForwardVector(), Frame.Up);
	return Flat.IsNearlyZero() ? 0.f : FGSTN_CameraMath::DirectionToHeading(Frame, Flat);
}

void AGSTN_CameraBiasVolume::BeginPlay()
{
	Super::BeginPlay();
	if (UGSTN_CameraSectionSubsystem* Subsystem = UGSTN_CameraSectionSubsystem::Get(GetWorld()))
	{
		Subsystem->RegisterBiasVolume(this);
	}
}

void AGSTN_CameraBiasVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGSTN_CameraSectionSubsystem* Subsystem = UGSTN_CameraSectionSubsystem::Get(GetWorld()))
	{
		Subsystem->UnregisterBiasVolume(this);
	}
	Super::EndPlay(EndPlayReason);
}
