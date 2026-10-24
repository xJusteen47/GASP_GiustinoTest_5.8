// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WallMovement/GSTN_WallEdgeTypes.h"
#include "GSTN_WallEdgeUtils.generated.h"

/**
 * GSTN_WallEdgeUtils: stateless, world-free maths of the edge look-ahead (wall-slide-v2.md 2.2 steps 5-6). All angles are
 * between surface normals, so none of it depends on the gravity direction.
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
	 */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static EGSTN_WallEdgeKind ClassifyAhead(const FGSTN_WallEdgeSettings& Settings, float MaxEdgeAngleDeg, bool bHitAhead, float Offset, float AngleDeg, bool bFarFaceFound);

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
};
