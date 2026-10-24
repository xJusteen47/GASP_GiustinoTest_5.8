// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WallMovement/GSTN_WallEdgeTypes.h"
#include "WallMovement/GSTN_WallMovementTypes.h"
#include "GSTN_WallEdgeUtils.generated.h"

struct FMovingComponentSet;

/**
 * GSTN_WallEdgeUtils: stateless maths of the edge look-ahead (wall-slide-v2.md 2.2 steps 4-6) and the two line-trace probes that
 * feed it (ProbeAhead, ProbeBumpAt). All angles are between surface normals and all heights are measured along the wall normal,
 * so none of it depends on the gravity direction. The probes only trace; they never move anything.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_WallEdgeUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Angle between two unit normals (deg) */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static float AngleBetweenNormalsDeg(const FVector& A, const FVector& B);

	/** max(MinLookahead, Speed * LookaheadTime) */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static float LookaheadDistance(const FGSTN_WallEdgeSettings& Settings, float Speed);

	/**
	 * Classifies what is ahead.
	 * bHitAhead: the lifted trace toward the wall hit a surface; Offset is its distance relative to the wall plane (negative =
	 * bump) and AngleDeg its angle to the wall normal. Without a hit the wall is gone ahead (convex); bFarFaceFound/AngleDeg
	 * then describe the far face found by the second trace (none: the wall simply ends, which is a convex edge).
	 * MaxEdgeAngleDeg is the largest turn the mode accepts (the base's MaxConcaveAngle / MaxConvexAngle).
	 * A face within the smooth angle and within step height is a Slope only if its offset is what such a slope makes over
	 * AheadDistance (the distance of the sample past the point where the plane was measured); a flush-parallel face with more
	 * offset than that is a step (Bump). AheadDistance < 0 = unknown: the old rule, every smooth face is a Slope.
	 * Past step height a face that recedes (Offset > 0) and turns away is Convex, one that rises toward the character is Concave.
	 */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static EGSTN_WallEdgeKind ClassifyAhead(const FGSTN_WallEdgeSettings& Settings, float MaxEdgeAngleDeg, bool bHitAhead, float Offset, float AngleDeg, bool bFarFaceFound, float AheadDistance = -1.f);

	/**
	 * Speed the character may have when it reaches the edge: TerminalSpeed scaled by the edge angle (1 at MaxSmoothAngle down to
	 * the class MinScale at 90 deg). Returns a very large value (no cap) for None/Bump/Slope/Blocked or a disabled class.
	 */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static float EdgeSpeedCap(const FGSTN_WallEdgeSettings& Settings, EGSTN_WallEdgeKind Kind, float AngleDeg, float TerminalSpeed);

	/** Highest speed allowed at Distance from the edge: sqrt(CornerSpeed^2 + 2 * CornerBraking * Distance). Monotonic, never below CornerSpeed */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static float AllowedSpeedAtDistance(const FGSTN_WallEdgeSettings& Settings, float CornerSpeed, float Distance);

	/** Height to lift to clear a bump found at Offset (0 for a recess), clamped to MaxStepHeight; the caller adds StepMargin */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static float BumpHeightOf(const FGSTN_WallEdgeSettings& Settings, float Offset);

	/**
	 * How far to lift the capsule along the wall normal to clear a bump of BumpHeight (measured from the wall plane) when it is seated
	 * WallGap above that plane: the part of the bump above the capsule's surface plus StepMargin. Never more than MaxStepHeight + StepMargin
	 */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static float StepLiftFor(const FGSTN_WallEdgeSettings& Settings, float BumpHeight, float WallGap);

	/** Half extent of an upright-or-tilted capsule along a unit direction: (HalfHeight - Radius) * |Dir . CapsuleUp| + Radius */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static float CapsuleSupportAlong(float Radius, float HalfHeight, const FQuat& CapsuleQuat, const FVector& Dir);

	/**
	 * The look-ahead (wall-slide-v2.md 2.2 step 4, counted with GSTN_WALL_QUERY_SCOPE): where the capsule would be if it were seated on
	 * the wall plane at its WallGap, the plane is read along the movement (Velocity projected on it) at LookaheadDistance past the
	 * capsule's leading edge, and what is found is classified. One line trace on a flat wall:
	 *  1. lifted StepHeight + Margin off the plane at the end point, down toward the wall: a hit on a valid wall gives the Offset
	 *     (negative = bump) and the Angle; a miss, or a hit on a surface the wall filter refuses, means the wall ends
	 *  2. only when 1 starts inside geometry (something taller than the step is there: an inside corner): a ray along the
	 *     movement from the lifted leading edge gives the face, its angle and the distance to it
	 *  3. only when the wall ends: a ray back toward the capsule, just behind the plane, gives the far face of an outside edge
	 * Every accepted hit goes through IsHitOnValidWall (T5 surface filter); pawns are ignored when Edges.bIgnorePawns.
	 * Distance is measured from the capsule's leading edge. Kind None and no query when the velocity along the plane is ~0.
	 */
	static FGSTN_WallAheadResult ProbeAhead(const FMovingComponentSet& MovingComps, const FGSTN_WallEdgeSettings& Edges, const FGSTN_WallCheckSettings& Check,
		const FVector& WallNormal, const FVector& GravityUp, const FVector& Velocity);

	/**
	 * Sizes the feature the capsule was just blocked by (one line trace): steps Inset along Dir into the hit point, lifts off the capsule's
	 * wall plane and traces toward the wall. Kind Bump = a low feature (Offset = minus its height) that the capsule can step over;
	 * anything else (taller than the step, ramp, the trace starts inside geometry) is not a bump.
	 */
	static FGSTN_WallAheadResult ProbeBumpAt(const FMovingComponentSet& MovingComps, const FGSTN_WallEdgeSettings& Edges, const FGSTN_WallCheckSettings& Check,
		const FVector& WallNormal, const FVector& GravityUp, const FVector& HitPoint, const FVector& Dir);
};
