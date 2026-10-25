// Friend of the v2 wall modes and of the entry transition for the headless tests (GSTN_WallV2*Tests.cpp): one definition, because the
// classes befriend this name and a second definition in another test file would clash in a unity build.

#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "WallMovement/Modes/GSTN_WallClimbV2MovementMode.h"
#include "WallMovement/Modes/GSTN_WallSlideV2MovementMode.h"
#include "WallMovement/Transitions/GSTN_WallEntryTransition.h"

struct FGSTN_WallTraverseTestAccess
{
	static FVector ComputeWallVelocity(const UGSTN_WallSlideV2MovementMode& Mode, const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane,
		const FVector& WallIntent, float DeltaSeconds)
	{
		return Mode.ComputeWallVelocity(StartState, Plane, WallIntent, DeltaSeconds);
	}

	static FVector ComputeWallVelocity(const UGSTN_WallClimbV2MovementMode& Mode, const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane,
		const FVector& WallIntent, float DeltaSeconds)
	{
		return Mode.ComputeWallVelocity(StartState, Plane, WallIntent, DeltaSeconds);
	}

	/** Pretends the mode was registered on a Mover component under this name (the vote does nothing without one) */
	static void SetRegisteredName(UGSTN_WallTraverseModeBase& Mode, FName ModeName)
	{
		Mode.RegisteredModeName = ModeName;
	}

	static void Vote(const UGSTN_WallTraverseModeBase& Mode, const FMoverSyncState& SyncState, FGSTN_WallTickState& State)
	{
		Mode.UpdateSurfaceVote(SyncState, State);
	}

	static void Step(const UGSTN_WallTraverseModeBase& Mode, const FMovingComponentSet& MovingComps, float DeltaSeconds, bool bHasDirIntent,
		FGSTN_WallTickState& State, FMovementRecord& MoveRecord)
	{
		Mode.TraverseStep(MovingComps, DeltaSeconds, bHasDirIntent, State, MoveRecord);
	}

	static FGSTN_WallAheadResult ProbeAhead(const UGSTN_WallTraverseModeBase& Mode, const FMovingComponentSet& MovingComps, const FGSTN_WallTickState& State)
	{
		return Mode.ProbeAhead(MovingComps, State, Mode.GetWallCheckSettings());
	}

	static FGSTN_WallCheckSettings GetWallCheckSettings(const UGSTN_WallTraverseModeBase& Mode)
	{
		return Mode.GetWallCheckSettings();
	}

	static FVector MapMoveIntentToWall(const UGSTN_WallSlideV2MovementMode& Mode, const FCharacterDefaultInputs& Inputs, const FVector& MoveIntent, const FGSTN_WallMovementPlane& Plane)
	{
		return Mode.MapMoveIntentToWall(Inputs, MoveIntent, Plane);
	}

	static bool IsMovementLocked(const UGSTN_WallTraverseModeBase& Mode, const FMoverTickStartData& StartState)
	{
		return Mode.IsMovementLocked(StartState);
	}

	/** The base riding and the base bookkeeping of the tick, called as SimulationTick calls them (GSTN_WallV2NetTests.cpp) */
	static void ApplyBase(const UGSTN_WallTraverseModeBase& Mode, const FSimulationTickParams& Params, FGSTN_WallTickState& State, FMovementRecord& MoveRecord)
	{
		Mode.ApplyMovementBase(Params, State, MoveRecord);
	}

	static void UpdateBase(const UGSTN_WallTraverseModeBase& Mode, const FSimulationTickParams& Params, FGSTN_WallTickState& State)
	{
		Mode.UpdateMovementBase(Params, State);
	}

	static float ComputeEdgeSpeedCap(const UGSTN_WallTraverseModeBase& Mode, const FGSTN_WallAheadResult& Ahead)
	{
		return Mode.ComputeEdgeSpeedCap(Ahead);
	}
};

/** Friend of the entry transition (one definition: the class befriends this name) */
struct FGSTN_WallEntryTestAccess
{
	static bool SweepForWall(const UGSTN_WallEntryTransition& Transition, const FSimulationTickParams& Params, const FVector& Direction, const FVector& GravityUp, FHitResult& OutHit)
	{
		return Transition.SweepForWall(Params, Direction, GravityUp, OutHit);
	}
};

#endif // WITH_DEV_AUTOMATION_TESTS
