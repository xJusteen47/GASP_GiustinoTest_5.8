// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/Modes/GSTN_SkiddingWallMovementMode.h"

#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "WallMovement/GSTN_WallMovementDebug.h"
#include "WallMovement/MoveLibrary/GSTN_WallMovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_SkiddingWallMovementMode)

void UGSTN_SkiddingWallMovementMode::SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	// Skid state, re-derived from the same start data GenerateMove used, so both agree. Read before the base tick moves the capsule
	bool bIsSkidding = false;
	if (const USceneComponent* UpdatedComponent = Params.MovingComps.UpdatedComponent.Get())
	{
		const FMoverDefaultSyncState* StartingSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
		const FGSTN_WallMovementPlane Plane = MakeStartMovementPlane(Params.StartState, UpdatedComponent->GetComponentQuat());
		const FVector WallIntent = ComputeWallIntent(Params.StartState, Plane);
		const float PriorHorizontalSpeed = StartingSyncState ? Plane.GetHorizontalSpeed(StartingSyncState->GetVelocity_WorldSpace()) : 0.f;
		bIsSkidding = IsSkidding(Params.StartState, Plane, WallIntent, PriorHorizontalSpeed);
	}

	Super::SimulationTick_Implementation(Params, OutputState);

	if (FGSTN_WallMovementSyncState* OutputWallState = OutputState.SyncState.SyncStateCollection.FindMutableDataByType<FGSTN_WallMovementSyncState>())
	{
		OutputWallState->bIsSkidding = bIsSkidding && OutputState.MovementEndState.NextModeName.IsNone();
	}
}

float UGSTN_SkiddingWallMovementMode::ComputeHorizontalSpeed(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent,
	float HorizontalSpeed, float DeltaSeconds) const
{
	if (IsSkidding(StartState, Plane, WallIntent, HorizontalSpeed))
	{
		GSTN_WALL_LOG(1, TEXT("GEN skid speed=%.1f"), HorizontalSpeed);
		return UGSTN_WallMovementUtils::MoveTowards(HorizontalSpeed, 0.f, Skid.Deceleration * DeltaSeconds);
	}
	return Super::ComputeHorizontalSpeed(StartState, Plane, WallIntent, HorizontalSpeed, DeltaSeconds);
}

bool UGSTN_SkiddingWallMovementMode::IsSkidding(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent, float HorizontalSpeed) const
{
	const FGSTN_WallMovementSyncState* WallState = StartState.SyncState.SyncStateCollection.FindDataByType<FGSTN_WallMovementSyncState>();
	const bool bWasSkidding = WallState && WallState->bIsSkidding;
	return UGSTN_WallMovementUtils::IsSkidding(bWasSkidding, WallIntent, HorizontalSpeed, Plane.HorizontalAxis, Skid);
}
