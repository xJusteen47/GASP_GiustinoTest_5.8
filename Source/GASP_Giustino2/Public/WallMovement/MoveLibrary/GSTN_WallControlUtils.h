// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WallMovement/GSTN_WallControlTypes.h"
#include "GSTN_WallControlUtils.generated.h"

/**
 * GSTN_WallControlUtils: the control model of the v2 wall modes as pure functions (wall-slide-v2.md 6.2-6.3). Called from
 * GenerateMove to build the proposed velocity; no world, no state, deterministic (rollback-safe).
 * Plane coordinates are (X = H horizontal along the wall, Y = V along the gravity-tangent, up positive), the stick uses the same axes.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_WallControlUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Axes of the movement plane: V = gravity-up projected on the plane (falls back to an arbitrary tangent on a ceiling/floor),
	 * H = V x N. Both unit, orthogonal to the wall normal.
	 */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static void MakePlaneAxes(const FVector& WallNormal, const FVector& GravityUp, FVector& OutHorizontal, FVector& OutVertical);

	/** World vector on the plane -> (H, V) */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static FVector2D ToPlane(const FVector& WorldVector, const FVector& Horizontal, const FVector& Vertical);

	/** (H, V) -> world vector on the plane */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static FVector FromPlane(const FVector2D& PlaneVector, const FVector& Horizontal, const FVector& Vertical);

	/**
	 * Entry: the velocity is turned onto the plane keeping its length (times EntrySpeedKeep, clamped to MaxEntrySpeed).
	 * Nothing is zeroed: the horizontal run and the fall carry in. A velocity along the normal (no tangent part) gives zero.
	 */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static FVector ProjectEntryVelocity(const FGSTN_WallSlideControlSettings& Settings, const FVector& Velocity, const FVector& WallNormal);

	/** Whether the vertical brake is engaged by this stick / tag state */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static bool ResolveBrake(const FGSTN_WallBrakeInput& BrakeInput, const FVector2D& VelocityOnPlane, const FVector2D& InputOnPlane, bool bTagActive);

	/** One tick of the slide: slow-decaying low-control horizontal axis, soft-terminal gravity vertical axis, hard braking */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static FVector2D StepSlideVelocity(const FGSTN_WallSlideControlSettings& Settings, const FVector2D& VelocityOnPlane, const FVector2D& InputOnPlane, bool bBrake, float DeltaSeconds);

	/** One tick of the climb: velocity moves toward InputOnPlane * ClimbSpeed (full 2D control), no gravity */
	UFUNCTION(BlueprintPure, Category = "Mover|Wall")
	static FVector2D StepClimbVelocity(const FGSTN_WallClimbControlSettings& Settings, const FVector2D& VelocityOnPlane, const FVector2D& InputOnPlane, float DeltaSeconds);
};
