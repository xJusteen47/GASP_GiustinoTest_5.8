#include "GSTN_DepthProjectionVolume.h"

#include "GSTN_DepthMarkerMath.h"
#include "GSTN_DepthMarkerSubsystem.h"

UGSTN_DepthProjectionComponent::UGSTN_DepthProjectionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetHiddenInGame(true);
	InitBoxExtent(FVector(300.0, 300.0, 200.0));
	ShapeColor = FColor(200, 120, 255);
	Projection.Mode = EGSTN_DepthProjectionMode::World;
}

bool UGSTN_DepthProjectionComponent::ContainsPoint(const FVector& WorldPoint) const
{
	return FGSTN_DepthMarkerMath::PointInOrientedBox(WorldPoint, FTransform(GetComponentQuat(), GetComponentLocation()), GetScaledBoxExtent());
}

FGSTN_DepthProjection UGSTN_DepthProjectionComponent::GetWorldProjection() const
{
	FGSTN_DepthProjection Out = Projection;
	if (bDirectionInBoxSpace && Out.Mode == EGSTN_DepthProjectionMode::World)
	{
		Out.Direction = GetComponentQuat().RotateVector(Out.Direction);
	}
	return Out;
}

void UGSTN_DepthProjectionComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UGSTN_DepthMarkerSubsystem* Subsystem = UGSTN_DepthMarkerSubsystem::Get(GetWorld()))
	{
		Subsystem->RegisterProjectionBox(this);
	}
}

void UGSTN_DepthProjectionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGSTN_DepthMarkerSubsystem* Subsystem = UGSTN_DepthMarkerSubsystem::Get(GetWorld()))
	{
		Subsystem->UnregisterProjectionBox(this);
	}
	Super::EndPlay(EndPlayReason);
}

AGSTN_DepthProjectionVolume::AGSTN_DepthProjectionVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	Box = CreateDefaultSubobject<UGSTN_DepthProjectionComponent>(TEXT("Box"));
	SetRootComponent(Box);
}
