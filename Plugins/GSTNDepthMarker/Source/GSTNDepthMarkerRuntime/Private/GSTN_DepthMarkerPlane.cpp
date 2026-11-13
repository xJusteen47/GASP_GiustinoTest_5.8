#include "GSTN_DepthMarkerPlane.h"

#include "GSTN_DepthMarkerMath.h"
#include "GSTN_DepthMarkerSubsystem.h"

UGSTN_DepthMarkerPlaneComponent::UGSTN_DepthMarkerPlaneComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	SetHiddenInGame(true);
	InitBoxExtent(FVector(200.0, 200.0, 50.0));
	ShapeColor = FColor(90, 210, 255);
}

void UGSTN_DepthMarkerPlaneComponent::GetProjectionPlane(FVector& OutPoint, FVector& OutNormal) const
{
	OutNormal = GetUpVector();
	OutPoint = GetComponentLocation() + OutNormal * (GetScaledBoxExtent().Z + PlaneOffsetCm);
}

bool UGSTN_DepthMarkerPlaneComponent::ContainsPoint(const FVector& WorldPoint) const
{
	return FGSTN_DepthMarkerMath::PointInOrientedBox(WorldPoint, FTransform(GetComponentQuat(), GetComponentLocation()), GetScaledBoxExtent());
}

FGSTN_DepthPlaneCandidate UGSTN_DepthMarkerPlaneComponent::MakeCandidate() const
{
	FGSTN_DepthPlaneCandidate Candidate;
	GetProjectionPlane(Candidate.Point, Candidate.Normal);
	// Distinct from the keys of hit components (which are small positive ids): a plane is never mistaken for a probed surface
	Candidate.SurfaceKey = -static_cast<int64>(GetUniqueID()) - 1;
	Candidate.ComponentTransform = GetComponentTransform();
	Candidate.bExplicit = true;
	Candidate.Priority = Priority;
	return Candidate;
}

void UGSTN_DepthMarkerPlaneComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UGSTN_DepthMarkerSubsystem* Subsystem = UGSTN_DepthMarkerSubsystem::Get(GetWorld()))
	{
		Subsystem->RegisterPlane(this);
	}
}

void UGSTN_DepthMarkerPlaneComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGSTN_DepthMarkerSubsystem* Subsystem = UGSTN_DepthMarkerSubsystem::Get(GetWorld()))
	{
		Subsystem->UnregisterPlane(this);
	}
	Super::EndPlay(EndPlayReason);
}

AGSTN_DepthMarkerPlaneVolume::AGSTN_DepthMarkerPlaneVolume()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	Plane = CreateDefaultSubobject<UGSTN_DepthMarkerPlaneComponent>(TEXT("Plane"));
	SetRootComponent(Plane);
}
