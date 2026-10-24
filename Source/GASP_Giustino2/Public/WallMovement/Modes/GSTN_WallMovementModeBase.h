// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_MovementModeBase.h"
#include "WallMovement/GSTN_WallMovementTypes.h"
#include "GSTN_WallMovementModeBase.generated.h"

struct FCharacterDefaultInputs;
struct FMoverDefaultSyncState;
struct FMovementRecord;
struct FMovingComponentSet;

/**
 * Essential wall movement: accelerated movement on arbitrary surfaces (walls, overhangs, ceilings), wrapping around
 * convex and concave corners. Works with any gravity direction (the capsule keeps upright relative to it) and on
 * moving/rotating walls (dynamic movement bases).
 *
 * Abstract: it only holds the movement core. Features are added by the subclasses through the override points below,
 * the same way USmoothWalkingMode extends USimpleWalkingMode:
 *  - UGSTN_ControlledWallMovementMode: stick mapping on the wall (MapMoveIntentToWall), detach and wall jump (CheckForWallExit)
 *  - UGSTN_SkiddingWallMovementMode:   skid against the horizontal movement (ComputeHorizontalSpeed)
 *  - UGSTN_SimpleWallMovementMode:     climbing, the stick drives the vertical axis too (GetVerticalMovement, ComputeTargetUp)
 *  - UGSTN_WallSlideMovementMode:      gravity drives the vertical axis (ComputeVerticalSpeed)
 * The math and the queries live in UGSTN_WallMovementUtils and UGSTN_WallQueryUtils, the data in GSTN_WallMovementTypes.h.
 *
 * Three separate quantities, each with its own settings category:
 *  - Raw probe normal   ("Surface Detection"): what the probes measure this substep. Noisy, can jump at edges
 *  - Filtered normal    ("Normal Filter"): the wall normal kept in the sync state. It is the MOVEMENT PLANE: velocity,
 *                       input mapping, adhesion and probe directions all use it
 *  - Capsule rotation   ("Orientation"): purely visual/collision pose. It CHASES the filtered normal with its own smoothing,
 *                       but never feeds back into the movement plane
 *  Position against the wall is a fourth, independent concern ("Adhesion").
 *
 * GenerateMove: intent on the wall (ComputeWallIntent) -> velocity in the plane, per axis (GenerateWallMove).
 *
 * SimulationTick: ride the movement base, face the wall, voluntary exits (CheckForWallExit), then per substep (MoveAlongWall):
 *  1. Sweep the capsule along the wall tangent (slide on blocking hits; a blocking wall the capsule fits on becomes a
 *     concave-corner candidate; moving down into a floor switches to FloorModeName)
 *  2. Probe the wall with several sphere sweeps (center/top/bottom + lookahead) and blend their normals (spatial filter).
 *     Hits on edges leading onto non-wall faces (roofs, floors) are discarded
 *  3. If nothing is found, run a wrap probe around the convex edge we just passed. No wall for NoWallGraceTime -> NoWallModeName
 *  4. Filter the normal: soft deadzone (surface noise), rate limit (popping samples). Frozen while at rest
 *  5. Build the target capsule orientation by parallel-transporting the current frame onto the filtered normal, plus upright bias
 *  6. Smooth toward it (time + distance based) and apply the rotation around the wall contact point, validated by an overlap test
 *  7. Adhere to the wall at a fixed gap (instant push-out, rate-limited pull-in, real penetration resolution)
 *  8. Parallel-transport the velocity onto the new plane (speed is preserved around corners)
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class GASP_GIUSTINO2_API UGSTN_WallMovementModeBase : public UGSTN_MovementModeBase
{
	GENERATED_BODY()

public:
	UGSTN_WallMovementModeBase(const FObjectInitializer& ObjectInitializer);

	virtual void OnRegistered(const FName ModeName, const FMoverSimContext& SimContext) override;
	virtual void Activate(const FMoverEventContext& Context, FName PrevModeName, const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, FMoverSyncState* OutSyncState, FMoverAuxStateContext* OutAuxState) override;

	virtual void GenerateMove_Implementation(const FMoverSimContext& SimContext, const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const override;
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	// ---------------------------------------------------------------- Movement
	// Velocity inside the movement plane (the filtered wall normal). None of these touch the normal or the capsule rotation.

	/** Sideways along the wall (perpendicular to gravity) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement")
	FGSTN_WallHorizontalMovement Horizontal;

	/** Moves longer than this are split in substeps, so corners are never skipped at speed. 0 = half the capsule radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MaxSubstepDistance = 0.f;

	/** Upper bound on substeps per tick (cost cap). If hit, substeps get longer than MaxSubstepDistance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxSubsteps = 4;

	// ---------------------------------------------------------------- Entry / Exit

	/** What happens to the incoming velocity when the mode starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Entry and Exit")
	FGSTN_WallEntryMomentum EntryMomentum;

	/** Mode to switch to after losing the wall for longer than NoWallGraceTime. None = stay in this mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Entry and Exit")
	FName NoWallModeName;

	/** How long the wall can be missing (e.g. while wrapping an edge) before leaving the mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Entry and Exit", meta = (ClampMin = "0", ForceUnits = "s"))
	float NoWallGraceTime = 0.15f;

	/** Mode to switch to when moving down into a floor (a surface below MinSurfaceAngle). None = stay in this mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Entry and Exit")
	FName FloorModeName;

	// ---------------------------------------------------------------- Movement Bases

	/** Ride moving/rotating walls (dynamic movement bases) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement Bases")
	bool bUseMovementBases = true;

	/** [Rotation] How the capsule follows a rotating wall */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement Bases", meta = (EditCondition = "bUseMovementBases"))
	EGSTN_WallBaseRotation BaseRotation = EGSTN_WallBaseRotation::FollowExceptRoll;

	/** When leaving the wall, add the wall's own velocity at the contact point (momentum from a moving platform) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement Bases", meta = (EditCondition = "bUseMovementBases"))
	bool bAddBaseVelocityOnExit = true;

	// ---------------------------------------------------------------- Surface Detection
	// What the probes accept and where they look. Output: the RAW PROBE NORMAL (and contact point) of this substep.
	// These shape which wall the character is on and how corners are sensed; they do not smooth anything by themselves.

	/**
	 * [Raw normal] Minimum angle between a surface normal and gravity-up for it to count as a wall. Below this it's a floor:
	 * probes ignore it, and moving down into it switches to FloorModeName
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MinSurfaceAngle = 40.f;

	/** [Raw normal] Maximum angle between a surface normal and gravity-up. 90 = vertical walls only, 180 = ceilings allowed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxSurfaceAngle = 180.f;

	/**
	 * [Raw normal] How far past WallGap the probes search for the wall. Too short and small bumps/recesses lose the wall
	 * (-> NoWallModeName); too long and the probes start seeing nearby geometry that isn't the current wall
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ForceUnits = "cm"))
	float WallProbeDistance = 25.f;

	/**
	 * [Raw normal] Probe sphere radius as a fraction of the capsule radius. A sphere returns a "rounded" normal on edges, so
	 * bigger = smoother transitions and small crevices ignored, but it also grazes more neighbouring geometry (props, corners)
	 * and that can pull the normal sideways. 0.6 is a good default
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0.1", ClampMax = "1"))
	float ProbeRadiusScale = 0.6f;

	/** [Raw normal] Where the top/bottom probes sit along the capsule cylinder (0 = all at the center, 1 = hemisphere centers) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ClampMax = "1"))
	float ProbeVerticalSpread = 0.8f;

	/**
	 * [Raw normal] Probe hits deviating more than this from the current filtered normal are discarded. Weights fade to zero
	 * toward this angle, so samples never pop in or out
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxProbeDeviationAngle = 70.f;

	/** [Raw normal] Max angle of an inside (concave) corner that can be transitioned onto. The capsule must also fit on the new wall */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxConcaveAngle = 120.f;

	/** [Raw normal] Max angle of an outside (convex) corner that can be wrapped around */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxConvexAngle = 120.f;

	/**
	 * [Raw normal] Lookahead along the movement direction, in seconds of travel. An inside corner ahead is added to the raw
	 * normal with a weight that grows as it gets closer, so the movement plane (and then the capsule) starts turning BEFORE
	 * touching it. This anticipates WHERE the normal goes; how smoothly the capsule follows is set in Orientation
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ForceUnits = "s"))
	float LookaheadTime = 0.2f;

	/** [Raw normal] Minimum lookahead distance, so corners are anticipated even when moving slowly */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Detection", meta = (ClampMin = "0", ForceUnits = "cm"))
	float MinLookaheadDistance = 15.f;

	// ---------------------------------------------------------------- Normal Filter
	// Raw probe normal -> FILTERED NORMAL (sync state). This is the movement plane: velocity, input mapping, adhesion
	// and next substep's probe directions use it. The capsule rotation then chases it (see Orientation).

	/**
	 * [Filtered normal] Soft deadzone: raw changes smaller than this are progressively ignored (surface noise, bumpy walls),
	 * bigger ones pass through untouched
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Normal Filter", meta = (ClampMin = "0", ClampMax = "30", ForceUnits = "deg"))
	float NormalDeadzoneAngle = 4.f;

	/**
	 * [Filtered normal] Max angular speed of the movement plane. Stops a probe sample appearing/disappearing from snapping it
	 * in a single step. Keep it well above how fast corners are taken (90 deg in ~0.2 s = 450 deg/s). 0 = unlimited
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Normal Filter", meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float MaxNormalChangeRate = 540.f;

	// ---------------------------------------------------------------- Adhesion
	// Distance from the wall along the filtered normal. Position only: no effect on the normal or the rotation.

	/** [Position] Desired gap between the capsule surface and the wall. Also part of the probes' reach */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Adhesion", meta = (ClampMin = "0", ForceUnits = "cm"))
	float WallGap = 2.f;

	/** [Position] Max pull-in speed toward the wall (bumpy walls don't pop the capsule in). Push-out is always instant */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Adhesion", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxAdhesionSpeed = 400.f;

	// ---------------------------------------------------------------- Orientation
	// Capsule rotation chasing the filtered normal. Rotation only: none of these change the movement plane, so they can be
	// tuned for looks/animation without affecting where the character goes.

	/** [Rotation] Time-based smoothing toward the target orientation (1/s). Higher = snappier. Acts even when standing still */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "0"))
	float RotationSharpness = 10.f;

	/**
	 * [Rotation] Distance-based smoothing: distance travelled to close ~63% of the remaining angle. Keeps corner rotation
	 * consistent at any speed. The faster of time/distance smoothing wins
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "1", ForceUnits = "cm"))
	float RotationSmoothingDistance = 20.f;

	/** [Rotation] Hard cap on the capsule angular speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "0", ForceUnits = "deg/s"))
	float MaxRotationRate = 720.f;

	/**
	 * [Rotation] How fast the capsule up re-aligns with gravity-up on walls (roll around the wall normal, 1/s). Works with any
	 * gravity direction. Fades out to 0 on ceilings
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "0"))
	float UprightSharpness = 6.f;

	// ---------------------------------------------------------------- Debug

	/** Draws the capsule (green = on wall, red = no wall), filtered normal (cyan), capsule up (blue), velocity (yellow), probes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Debug")
	bool bDrawDebug = false;

protected:
	// ---------------------------------------------------------------- GenerateMove (override points)

	/**
	 * Maps a world-space move intent (length <= 1, already non zero) onto the wall plane.
	 * Base: plain projection on the plane, so the intent keeps its world direction
	 */
	virtual FVector MapMoveIntentToWall(const FCharacterDefaultInputs& Inputs, const FVector& MoveIntent, const FGSTN_WallMovementPlane& Plane) const;

	/**
	 * Velocity in the wall plane for this tick, from the prior one. Each axis has its own model and speed limits.
	 * Base: ComputeHorizontalSpeed and ComputeVerticalSpeed, then the terminal speeds
	 */
	virtual FVector GenerateWallMove(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent,
		const FVector& PriorVelocity, float DeltaSeconds) const;

	/** Horizontal (sideways along the wall) speed. Base: the CMC-style model with the Horizontal settings */
	virtual float ComputeHorizontalSpeed(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent,
		float HorizontalSpeed, float DeltaSeconds) const;

	/** Vertical (along gravity projected on the wall, positive = up) speed. Base: the CMC-style model with GetVerticalMovement */
	virtual float ComputeVerticalSpeed(const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent, float VerticalSpeed, float DeltaSeconds) const;

	/**
	 * Vertical velocity model: input targets MaxUpSpeed/MaxDownSpeed (both 0 = no vertical input, the axis only brakes) and
	 * the terminal speeds clamp it. Base: no vertical input, no braking, no limits
	 */
	virtual FGSTN_WallVerticalMovement GetVerticalMovement() const;

	// ---------------------------------------------------------------- SimulationTick (override points)

	/**
	 * Leaving the wall on purpose (jump, detach...), checked before moving. Call InOutState.LeaveTo to leave: the tick then
	 * skips the movement. Base: none
	 */
	virtual void CheckForWallExit(const FSimulationTickParams& Params, FGSTN_WallTickState& InOutState) const;

	/**
	 * Whether the pawn is held in place from outside (hitstop): a locked tick skips CheckForWallExit (so no wall jump, no detach, no
	 * pull-away) and does not advance TimeWithoutWall / PullAwayTime, so the mode is still the same one when the lock ends. Read from
	 * the start sync state like every simulation input. Base: Mover carries the tag Combat.Mover.Frozen (a freeze layered move of the
	 * combat plugin); the tag is looked up by name so this module does not depend on the plugin. Unlocked ticks behave as before
	 */
	virtual bool IsMovementLocked(const FMoverTickStartData& StartState) const;

	/**
	 * Capsule up the orientation chases on the wall: parallel transport of the current frame plus the upright bias.
	 * The target orientation faces the wall (X = -WallNormal) with this up
	 */
	virtual FVector ComputeTargetUp(const FQuat& CurrentQuat, const FVector& WallNormal, const FVector& GravityUp, const FVector& Velocity, float DeltaSeconds) const;

	// ---------------------------------------------------------------- GenerateMove steps

	/** The plane the move starting from StartState happens in (filtered wall normal, sync state orientation) */
	FGSTN_WallMovementPlane MakeStartMovementPlane(const FMoverTickStartData& StartState, const FQuat& FallbackOrientation) const;

	/** Character input -> wall-tangent intent (length <= 1). Missing input and debug overrides are handled here, the mapping in MapMoveIntentToWall */
	FVector ComputeWallIntent(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane) const;

	// ---------------------------------------------------------------- SimulationTick steps

	/** Working state of the tick, from the start sync state and the proposed move */
	FGSTN_WallTickState MakeTickState(const FSimulationTickParams& Params, const FQuat& StartQuat) const;

	/** Moves/rotates the capsule, the wall normal and the velocity with the dynamic wall it is riding */
	void ApplyMovementBase(const FSimulationTickParams& Params, FGSTN_WallTickState& InOutState, FMovementRecord& MoveRecord) const;

	/** Substepped move along the wall: translate, sense, filter the normal, orient, adhere, transport the velocity */
	void MoveAlongWall(const FSimulationTickParams& Params, FGSTN_WallTickState& InOutState, FMovementRecord& MoveRecord) const;

	/**
	 * Sweeps the capsule along the wall by SubDelta, sliding on blocking hits. Fills OutCornerHit on an inside corner.
	 * Returns false if it left the mode (moved down into a floor)
	 */
	bool MoveSubstep(const FMovingComponentSet& MovingComps, const FVector& SubDelta, float SubDeltaSeconds, float RemainingSeconds,
		const FGSTN_WallCheckSettings& CheckSettings, FGSTN_WallTickState& InOutState, FGSTN_WallCornerHit& OutCornerHit, FMovementRecord& MoveRecord) const;

	/** Probes the wall around the capsule, then around the convex edge just passed */
	bool SenseWall(const FMovingComponentSet& MovingComps, const FVector& SubDelta, const FGSTN_WallCornerHit& CornerHit,
		const FGSTN_WallCheckSettings& CheckSettings, const FGSTN_WallTickState& State, FGSTN_WallProbeResult& OutProbe) const;

	/** Takes a found wall: filters the movement plane toward the probe normal (frozen at rest) and refreshes the contact */
	void AttachToWall(const FGSTN_WallProbeResult& Probe, bool bAtRest, float DeltaSeconds, FGSTN_WallTickState& InOutState) const;

	/** Turns the capsule toward the target orientation, smoothed, around the wall contact point */
	void OrientToWall(const FMovingComponentSet& MovingComps, float DistanceTravelled, float DeltaSeconds, FGSTN_WallTickState& InOutState, FMovementRecord& MoveRecord) const;

	/** Stores the dynamic wall to ride next tick (rollback blackboard, GSTNWallBlackboard::BaseInfo), or adds its velocity and forgets it when leaving */
	void UpdateMovementBase(const FSimulationTickParams& Params, FGSTN_WallTickState& InOutState) const;

	/** Writes the tick result into the default and wall sync states, and the mode switch if any */
	void CaptureFinalState(const FSimulationTickParams& Params, const FQuat& StartQuat, const FGSTN_WallTickState& State, bool bLeftOnPurpose, FMoverTickEndData& OutputState) const;

	// ---------------------------------------------------------------- Helpers

	/** Target capsule orientation: facing the wall, with ComputeTargetUp as up */
	FQuat ComputeTargetOrientation(const FQuat& CurrentQuat, const FVector& WallNormal, const FVector& GravityUp, const FVector& Velocity, float DeltaSeconds) const;

	/** Query parameters from the Surface Detection and Adhesion settings */
	FGSTN_WallCheckSettings GetWallCheckSettings() const;

	/** Wall normal of the sync state, or the capsule's back if there is none yet */
	static FVector GetWallNormal(const FGSTN_WallMovementSyncState* WallState, const FQuat& FallbackOrientation);

#if ENABLE_DRAW_DEBUG
	void DrawWallDebug(const FMovingComponentSet& MovingComps, const FGSTN_WallTickState& State) const;
#endif

	FName RegisteredModeName = NAME_None;
};
