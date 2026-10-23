// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WallMovement/GSTN_WallMovementTypes.h"
#include "GSTN_WallQueryUtils.generated.h"

class UPrimitiveComponent;
class UWorld;
struct FCollisionQueryParams;
struct FCollisionResponseParams;
struct FHitResult;
struct FMovingComponentSet;

/**
 * GSTN_WallQueryUtils: a collection of stateless static functions for wall-related queries (surface validation,
 * multi-probe wall detection, corner checks). The wall counterpart of UFloorQueryUtils.
 * None of these move the component: see UGSTN_WallMovementUtils for that.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_WallQueryUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Whether a surface with this normal counts as a wall: its angle to gravity-up is within [MinSurfaceAngle, MaxSurfaceAngle] (deg) */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static bool IsValidWallNormal(const FVector& Normal, const FVector& GravityUp, float MinSurfaceAngle, float MaxSurfaceAngle);

	/**
	 * Whether the surface of a hit is accepted by the filter (traversal-surfaces.md §5.2): the hit's behaviours, from
	 * UGSTN_SurfaceUtils::Resolve, must match RequiredBehaviours. An empty query passes everything without resolving (the old
	 * behaviour); a surface nothing marked passes if bUnknownSurfacesPass. Stateless: resolves once per call, caches nothing.
	 */
	static bool PassesSurfaceFilter(const FHitResult& Hit, const FGSTN_WallSurfaceFilter& Filter);

	/**
	 * Whether a sweep hit lies on climbable geometry. On edges a sphere/capsule sweep returns a "rounded" normal blended between
	 * the two faces: that is only trusted if both faces are walls, so the lip of a floor or roof can't tilt the capsule toward it.
	 * The surface filter of Settings is evaluated right after the angle test of the hit's own normals.
	 */
	static bool IsHitOnValidWall(const UWorld* World, const FHitResult& Hit, const FVector& GravityUp, const FGSTN_WallCheckSettings& Settings,
		ECollisionChannel Channel, const FCollisionQueryParams& QueryParams, const FCollisionResponseParams& ResponseParams);

	/**
	 * Whether the capsule fits on a new wall met at an inside corner: turned to face it and placed at WallGap from it,
	 * it must not overlap anything. Rejects faces narrower than the capsule (small steps, slanted niches), which would
	 * otherwise be accepted as walls and leave the capsule wedged half-rotated between the two surfaces.
	 */
	static bool CanFitOnWall(const FMovingComponentSet& MovingComps, const FQuat& CurrentQuat, const FVector& NewNormal,
		const FVector& ContactPoint, float Radius, float HalfHeight, float WallGap);

	/**
	 * Whether a blocking hit of a move along the wall is an inside (concave) corner the character can move onto: another
	 * climbable surface, turned more than MinCornerAngle (deg) from the current wall and no more than MaxConcaveAngle,
	 * that the capsule fits on.
	 */
	static bool IsConcaveCornerHit(const FMovingComponentSet& MovingComps, const FHitResult& Hit, const FVector& MoveDelta, const FQuat& Orientation,
		const FVector& WallNormal, const FVector& GravityUp, float MinCornerAngle, const FGSTN_WallCheckSettings& Settings);

	/** Whether a blocking hit of a move along the wall is a floor: moving down into a surface flatter than a wall */
	static bool IsFloorHit(const FHitResult& Hit, const FVector& MoveDelta, const FVector& GravityUp, float MinSurfaceAngle);

	/**
	 * Multi-probe wall query around the capsule. Blends, by weight:
	 *  - sphere sweeps toward the wall from the capsule center/top/bottom (spatial filter)
	 *  - a lookahead sweep along the movement, so inside corners ahead are anticipated
	 *  - the inside corner the move itself ran into, if any
	 * Returns true if the blended normal is a valid wall.
	 */
	static bool FindWall(const FMovingComponentSet& MovingComps, const FQuat& Orientation, const FVector& WallNormal, const FVector& Velocity,
		const FGSTN_WallCornerHit& CornerHit, const FVector& GravityUp, const FGSTN_WallCheckSettings& Settings, FGSTN_WallProbeResult& OutResult);

	/** Looks for the face around an outside (convex) edge the capsule just moved past: dives behind the old wall plane, then sweeps back */
	static bool FindWallAroundConvexEdge(const FMovingComponentSet& MovingComps, const FVector& WallNormal, const FVector& MoveDelta,
		const FVector& GravityUp, const FGSTN_WallCheckSettings& Settings, FGSTN_WallProbeResult& OutResult);

	/** Radius and half height of the updated primitive's collision shape (box extents are approximated) */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static void GetCapsuleSize(const UPrimitiveComponent* Primitive, float& OutRadius, float& OutHalfHeight);
};
