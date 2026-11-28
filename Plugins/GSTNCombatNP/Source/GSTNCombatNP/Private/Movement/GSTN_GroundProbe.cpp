// Combat simulation (GAS-NPP fork).

#include "Movement/GSTN_GroundProbe.h"
#include "Sim/GSTN_SimQuery.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GSTN_SurfaceTypes.h"
#include "GSTN_SurfaceUtils.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

bool FGSTN_GroundProbe::HasFloorAhead(const FMoverTickStartData& StartState, const UMoverComponent& MoverComp, const FVector& Velocity, double StepMs)
{
	const FMoverDefaultSyncState* Sync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(MoverComp.GetUpdatedComponent());
	if (!Sync || !Primitive || !Primitive->GetWorld())
	{
		return true;
	}
	// The probe of the push back: the next step's feet position must have floor under it
	const FVector Up = MoverComp.GetUpDirection();
	const double HalfHeight = Primitive->Bounds.BoxExtent.GetMax();
	const FVector Next = Sync->GetLocation_WorldSpace() + Velocity * (StepMs * 0.001);
	const FVector Start = Next - Up * (HalfHeight - 10.0);
	const FVector End = Next - Up * (HalfHeight + LedgeProbeDepth);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTNGroundProbeLedge), false, Primitive->GetOwner());
	return FGSTN_SimQuery::LineTraceTestByChannel(Primitive->GetWorld(), Start, End, ECC_Visibility, Params);
}

bool FGSTN_GroundProbe::ResolveFloorSurface(const FMoverTickStartData& StartState, FGSTN_SurfaceView& OutView)
{
	const FMoverDefaultSyncState* Sync = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const UPrimitiveComponent* Base = Sync ? Sync->GetMovementBase() : nullptr;
	if (!Base)
	{
		return false;
	}
	// Static configuration of the collider (traversal-surfaces.md §2.2): the same answer on every machine
	OutView = UGSTN_SurfaceUtils::ResolveComponent(Base, Sync->GetMovementBaseBoneName());
	return true;
}
