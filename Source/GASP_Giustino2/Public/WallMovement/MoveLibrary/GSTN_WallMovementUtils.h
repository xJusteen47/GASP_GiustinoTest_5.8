// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WallMovement/GSTN_WallMovementTypes.h"
#include "GSTN_WallMovementUtils.generated.h"

struct FHitResult;
struct FMovementRecord;
struct FMovingComponentSet;

/**
 * GSTN_WallMovementUtils: a collection of stateless static functions for wall movement: the movement plane, the
 * per-axis velocity model, the wall normal filter, the capsule orientation and the moves that keep the capsule on the
 * wall. The wall counterpart of UGroundMovementUtils. Queries that don't move anything are in UGSTN_WallQueryUtils.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_WallMovementUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ---------------------------------------------------------------- Movement plane

	/** Tangent basis of the wall plane, aligned with the capsule's heading. Matches UE's basis convention (Y = Z ^ X) with X = -WallNormal */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static void ComputeWallTangentBasis(const FQuat& Orientation, const FVector& WallNormal, FVector& OutTangentUp, FVector& OutTangentRight);

	/** Horizontal/vertical axes of the wall plane: gravity-relative on walls, capsule-relative on ceilings */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static FGSTN_WallMovementPlane MakeMovementPlane(const FQuat& Orientation, const FVector& WallNormal, const FVector& GravityUp);

	// ---------------------------------------------------------------- Velocity model

	/**
	 * One axis of the CharacterMovementComponent-style velocity model. With input (TargetSpeed != 0) friction turns the
	 * velocity toward the input direction, then acceleration brings it to the target speed (braking back to it when
	 * above). Without input: proportional + constant braking toward zero, then StopSpeed snaps tiny residuals to zero
	 */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static float ComputeAxisSpeed(float Speed, float TargetSpeed, float Acceleration, float Friction, float BrakingFriction,
		float BrakingDeceleration, float StopSpeed, float DeltaSeconds);

	/** ComputeAxisSpeed on the horizontal axis. Input is the intent along the axis (-1..1) */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static float ComputeHorizontalSpeed(float Speed, float Input, const FGSTN_WallHorizontalMovement& Settings, float DeltaSeconds);

	/** ComputeAxisSpeed on the vertical axis. Input is the intent along the axis (-1..1): positive targets MaxUpSpeed, negative MaxDownSpeed */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static float ComputeVerticalSpeed(float Speed, float Input, const FGSTN_WallVerticalMovement& Settings, float DeltaSeconds);

	/** Applies the horizontal terminal speed (absolute limit whatever the source) */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static float ClampHorizontalSpeed(float Speed, const FGSTN_WallHorizontalMovement& Settings);

	/** Applies the vertical terminal speeds (absolute limits whatever the source) */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static float ClampVerticalSpeed(float Speed, const FGSTN_WallVerticalMovement& Settings);

	/** Whether a move is a skid (hard brake against the horizontal movement), with hysteresis on an ongoing skid */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static bool IsSkidding(bool bWasSkidding, const FVector& Intent, float HorizontalSpeed, const FVector& HorizontalAxis, const FGSTN_WallSkid& Settings);

	/** Incoming velocity split on the starting wall's axes, each part scaled and clamped by EntryMomentum */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static FVector ComputeEntryVelocity(const FVector& InVelocity, const FGSTN_WallMovementPlane& Plane, const FGSTN_WallEntryMomentum& EntryMomentum);

	/** Launch velocity of a wall jump: away from the wall, up along gravity, plus part of the horizontal wall velocity */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static FVector ComputeWallJumpVelocity(const FVector& Velocity, const FGSTN_WallMovementPlane& Plane, const FGSTN_WallJump& WallJump);

	/** Whether the move input, flattened on the gravity plane, points away from the wall by at least MinInputDot */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static bool IsPullingAwayFromWall(const FVector& MoveInput, const FVector& WallNormal, const FVector& GravityUp, float MinInputDot);

	/** Parallel-transports a velocity from one wall plane onto the next: same speed, direction carried around the corner */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static FVector TransportVelocity(const FVector& Velocity, const FVector& FromNormal, const FVector& ToNormal);

	// ---------------------------------------------------------------- Normal filter and orientation

	/**
	 * Moves the filtered wall normal toward a raw probe normal. Soft deadzone: changes below DeadzoneAngle (deg) are
	 * progressively ignored (surface noise), bigger ones pass through untouched. Then rate limit (MaxChangeRate, deg/s,
	 * 0 = unlimited): a probe sample appearing or vanishing must not snap the movement plane in a single step
	 */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static FVector FilterWallNormal(const FVector& CurrentNormal, const FVector& ProbeNormal, float DeadzoneAngle, float MaxChangeRate, float DeltaSeconds);

	/**
	 * Capsule up after parallel transport: the minimal rotation that brings the current facing (-Forward) onto the wall
	 * (-Normal). It changes only what the geometry forces and keeps the heading on the wall, so there are no flips at
	 * ceilings or corners. Returned in the wall plane
	 */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static FVector ComputeTransportedUp(const FQuat& CurrentQuat, const FVector& WallNormal, const FVector& GravityUp);

	/**
	 * Upright bias: rolls Up (around the wall normal) toward gravity-up projected on the wall. Its strength scales with
	 * how vertical the wall is, so it fades smoothly to zero on ceilings, where "up" is undefined
	 */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static FVector ApplyUprightBias(const FVector& Up, const FVector& WallNormal, const FVector& GravityUp, float UprightSharpness, float DeltaSeconds);

	/**
	 * Smooths CurrentQuat toward TargetQuat by time (Sharpness, 1/s) AND distance travelled (SmoothingDistance, cm):
	 * the faster of the two wins, so corner rotation is consistent at any speed. Then caps the angular speed (deg/s)
	 */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static FQuat SmoothOrientation(const FQuat& CurrentQuat, const FQuat& TargetQuat, float Sharpness, float SmoothingDistance,
		float MaxRotationRate, float DistanceTravelled, float DeltaSeconds);

	/** Angular velocity (deg/s) of the rotation from FromQuat to ToQuat over DeltaSeconds */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static FVector ComputeAngularVelocityDegrees(const FQuat& FromQuat, const FQuat& ToQuat, float DeltaSeconds);

	// ---------------------------------------------------------------- Moves

	/**
	 * Facing away from the wall (e.g. grabbing it with the back): turns the heading toward the wall around the capsule's
	 * own axis and center. The capsule is symmetric around it, so this never changes its collision. Returns true if it turned
	 */
	static bool TryMoveToFaceWall(const FMovingComponentSet& MovingComps, const FVector& WallNormal, FMovementRecord& MoveRecord);

	/**
	 * Rotates the capsule around Pivot (the wall contact) instead of its center, so it rolls over edges rather than
	 * swinging its ends into the geometry. Backs off the rotation if the new pose would overlap. Returns the orientation applied
	 */
	static FQuat TryMoveToRotateAroundPivot(const FMovingComponentSet& MovingComps, const FQuat& FromQuat, const FQuat& ToQuat, const FVector& Pivot, FMovementRecord& MoveRecord);

	/**
	 * Keeps the capsule at WallGap from the wall: sweeps the real capsule toward it (bridging small crevices), pushes out
	 * instantly, pulls in no faster than MaxAdhesionSpeed, and resolves real penetrations along their own direction
	 */
	static void TryMoveToAdhereToWall(const FMovingComponentSet& MovingComps, const FQuat& Orientation, const FVector& WallNormal, float WallGap,
		float WallProbeDistance, float MaxAdhesionSpeed, float DeltaSeconds, FMovementRecord& MoveRecord);

	/**
	 * Contact and adhesion in ONE capsule sweep (wall-slide-v2.md 2.2 step 8): the sweep toward -WallNormal over
	 * WallGap + WallProbeDistance is both the wall sensor (OutContact: rounded normal, contact point, component) and the
	 * adhesion probe. The gap error then moves the capsule along the normal: pulling in is UNSWEPT (at most Hit.Distance - WallGap,
	 * which the sweep already proved free) and rate-limited to MaxAdhesionSpeed, pushing out is swept and instant.
	 *
	 * Returns true when a hit that passes IsHitOnValidWall (angle, surface filter, edge faces) was found; OutContact is then valid
	 * (as measured BEFORE the correction). Returns false with OutContact.bStartPenetrating set when the capsule was already
	 * overlapping something (resolved along the penetration, no sensing this tick), and false for nothing found / not a wall.
	 * Every query is counted for gstn.Wall.CountQueries.
	 */
	static bool AdhereAndSense(const FMovingComponentSet& MovingComps, const FQuat& Orientation, const FVector& WallNormal, const FVector& GravityUp,
		const FGSTN_WallCheckSettings& Settings, float MaxAdhesionSpeed, float DeltaSeconds, FMovementRecord& MoveRecord, FHitResult& OutContact);

	/**
	 * Moves/rotates the capsule, the wall normal and the velocity with the dynamic wall described by BaseInfo, according to
	 * BaseRotation. Returns true if the base moved
	 */
	static bool TryMoveWithWallBase(const FMovingComponentSet& MovingComps, const FGSTN_WallBaseInfo& BaseInfo, EGSTN_WallBaseRotation BaseRotation,
		FVector& InOutWallNormal, FVector& InOutVelocity, FMovementRecord& MoveRecord);

	/** Builds the info to ride Base next tick. False if it isn't a dynamic movement base */
	static bool MakeWallBaseInfo(UPrimitiveComponent* Base, FName BoneName, const FVector& CharacterLocation, FGSTN_WallBaseInfo& OutBaseInfo);

	// ---------------------------------------------------------------- Math helpers

	/** Constant-rate move of a scalar toward a target, never overshooting */
	static float MoveTowards(float Value, float Target, float MaxStep);

	/** Rotates A toward B around Axis (both assumed perpendicular to Axis) by Alpha of their signed angle */
	static FVector RotateTowardsInPlane(const FVector& A, const FVector& B, const FVector& Axis, float Alpha);

	/** Projects V onto the plane, keeping its original length */
	static FVector ProjectOnPlaneKeepLength(const FVector& V, const FVector& PlaneNormal);

	/** Alpha for exponential smoothing that is independent of the step size */
	static float ExpAlpha(float Sharpness, float Step);
};
