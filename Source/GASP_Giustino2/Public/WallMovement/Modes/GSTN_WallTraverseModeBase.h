// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WallMovement/GSTN_WallEdgeTypes.h"
#include "WallMovement/Modes/GSTN_ControlledWallMovementMode.h"
#include "GSTN_WallTraverseModeBase.generated.h"

struct FGSTN_WallTraverseTestAccess;

/**
 * Shared core of the v2 wall modes (slide, later climb), on the lean pipeline of wall-slide-v2.md 2.2 and 6.1-6.3. It is the
 * answer to the "Warframe problem" of the legacy modes: a wall mode that rotates its collision with every facet, follows every
 * raw normal and spends speed in many places snags on complex geometry. Three rules:
 *
 *  1. The collision capsule NEVER rotates on a wall. Its up stays gravity-up; a capsule is symmetric around that axis, so
 *     facing the wall needs no collision change. The facing is presentation: a yaw about gravity-up, written to the sync state
 *     as the orientation (ComputeFacingQuat). Tilting for overhangs is a climb-mode opt-in, never here.
 *  2. The character moves on a PROXY PLANE: the contact sweep's normal, filtered (deadzone + rate limit, as the legacy modes).
 *     Small features are absorbed by adhesion (the capsule is kept at WallGap from the real surface), not by turning.
 *  3. One place decides speed: the control model of the subclass (ComputeWallVelocity). Geometry may only cap speed ahead of a
 *     real corner (ComputeEdgeSpeedCap, W3); transport is length preserving, a blocked sweep slides without a friction term.
 *
 * GenerateMove builds the FProposedMove from the virtual ComputeWallVelocity(StartState, Plane, Input, Dt). Mover merges layered
 * and queued moves (abilities, knockback, launches) into it as for any mode, and the tick moves by the FINAL proposed velocity.
 * Extending = overriding ComputeWallVelocity or the edge hooks, or adding a layered move; never the tick.
 *
 * SimulationTick, per tick, one pass and no substeps (the move is clamped to MaxMoveRadii capsule radii instead):
 *  1. ride the movement base, voluntary exits (CheckForWallExit of the controlled mode: wall jump, detach)
 *  2. edge hooks (ProbeAhead, ClassifyAhead, ComputeEdgeSpeedCap): no-ops until W3, so no query is spent on them yet
 *  3. ONE move sweep along the plane, facing yaw applied in the same move (a blocking hit: floor -> FloorModeName, step-over hook,
 *     inside corner, else slide)
 *  4. contact + adhesion in ONE capsule sweep (UGSTN_WallMovementUtils::AdhereAndSense); nothing found -> wrap around the convex
 *     edge, then NoWallGraceTime -> NoWallModeName
 *  5. filter the proxy plane toward the contact normal, transport the velocity onto it
 *  6. store the ridden base, write the sync state
 *
 * Properties of the legacy base that this mode does not read (documented, not hidden: a later EditCondition can grey them out):
 * Horizontal, MaxSubstepDistance, MaxSubsteps, EntryMomentum, ProbeRadiusScale, ProbeVerticalSpread, MaxProbeDeviationAngle,
 * LookaheadTime, MinLookaheadDistance, UprightSharpness. Orientation settings (RotationSharpness, RotationSmoothingDistance,
 * MaxRotationRate) shape the facing yaw only.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class GASP_GIUSTINO2_API UGSTN_WallTraverseModeBase : public UGSTN_ControlledWallMovementMode
{
	GENERATED_BODY()

	friend struct FGSTN_WallTraverseTestAccess;

public:
	UGSTN_WallTraverseModeBase(const FObjectInitializer& ObjectInitializer);

	virtual void Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState) override;

	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	/**
	 * The move of one tick is clamped to this many capsule radii, which is what replaces the substep loop: a sweep that long
	 * cannot skip a concave wall. At 60 Hz one radius per tick is about 2000 cm/s, far above the slide speeds
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0.1", ClampMax = "2"))
	float MaxMoveRadii = 1.f;

	/**
	 * Edge look-ahead, corner speed cap and step-over tuning (wall-slide-v2.md 2.3). INERT until checkpoints W3 (corners) and W4
	 * (bumps) fill the hooks below: the values are stored but nothing reads them yet
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Edges")
	FGSTN_WallEdgeSettings Edges;

	/**
	 * The velocity a character grabbing the wall starts with, from the velocity it arrives with. Base: turned onto the plane
	 * keeping its length (UGSTN_WallControlUtils::ProjectEntryVelocity with default entry settings), so the run and the fall
	 * carry in. Subclasses use their own entry settings
	 */
	virtual FVector ComputeEntryVelocity(const FVector& InVelocity, const FVector& WallNormal) const;

	/**
	 * Presentation facing: a rotation about GravityUp that looks at the wall (X = -WallNormal flattened on the gravity plane).
	 * Its up is exactly GravityUp, so applying it never changes what the capsule collides with. A wall (nearly) parallel to
	 * gravity keeps the current heading
	 */
	static FQuat ComputeFacingQuat(const FQuat& CurrentQuat, const FVector& WallNormal, const FVector& GravityUp);

protected:
	// ---------------------------------------------------------------- Control (override points)

	/**
	 * Velocity on the wall for this tick, from the start state (the prior velocity is its default sync state's), the movement
	 * plane and the stick intent mapped on it (world vector on the plane, length <= 1). Base: keeps the prior velocity
	 */
	virtual FVector ComputeWallVelocity(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent, float DeltaSeconds) const;

	/** Prior velocity of the start state projected on the plane (not length preserving: a velocity into the wall must not turn into speed along it) */
	FVector GetPriorVelocityOnPlane(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane) const;

	// ---------------------------------------------------------------- Edge hooks (wall-slide-v2.md 2.6), no-ops until W3 / W4

	/** Looks at the wall ahead of the movement (line traces in W3, counted with GSTN_WALL_QUERY_SCOPE). Base: nothing ahead (Kind None) */
	virtual FGSTN_WallAheadResult ProbeAhead(const FMovingComponentSet& MovingComps, const FGSTN_WallTickState& State, const FGSTN_WallCheckSettings& CheckSettings) const;

	/** Final kind of what the look-ahead found. Base: the kind the probe already set */
	virtual EGSTN_WallEdgeKind ClassifyAhead(const FGSTN_WallAheadResult& Ahead) const;

	/** Highest speed allowed now given the edge ahead (cm/s). Base: no cap (the largest float) */
	virtual float ComputeEdgeSpeedCap(const FGSTN_WallAheadResult& Ahead) const;

	/**
	 * A move was blocked by a bump the look-ahead classified as stepable: lift over it and redo the move. Returns true if it
	 * handled the hit (then no corner or floor logic runs). Base: false
	 */
	virtual bool TryStepOverBump(const FMovingComponentSet& MovingComps, const FHitResult& Hit, const FGSTN_WallAheadResult& Ahead, float DeltaSeconds,
		FGSTN_WallTickState& InOutState, FMovementRecord& MoveRecord) const;

	// ---------------------------------------------------------------- SimulationTick steps

	/**
	 * The world part of the tick: move, sense, adhere, filter the plane. Depends only on the moving components, the working state
	 * (velocity = the final proposed velocity) and the timestep, so it can be exercised without a Mover pawn
	 */
	void TraverseStep(const FMovingComponentSet& MovingComps, float DeltaSeconds, bool bHasDirIntent, FGSTN_WallTickState& InOutState, FMovementRecord& MoveRecord) const;

	/**
	 * Handles the blocking hit of the move sweep: floor, inside corner or plain slide. Fills OutCornerHit on an inside corner.
	 * Returns false if it left the mode (moved down into a floor)
	 */
	bool HandleMoveBlocked(const FMovingComponentSet& MovingComps, const FHitResult& Hit, const FVector& MoveDelta, float DeltaSeconds,
		const FGSTN_WallAheadResult& Ahead, const FGSTN_WallCheckSettings& CheckSettings, FGSTN_WallTickState& InOutState,
		FGSTN_WallCornerHit& OutCornerHit, FMovementRecord& MoveRecord) const;
};
