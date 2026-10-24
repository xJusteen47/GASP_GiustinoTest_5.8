// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WallMovement/GSTN_WallControlTypes.h"
#include "WallMovement/Modes/GSTN_WallTraverseModeBase.h"
#include "GSTN_WallSlideV2MovementMode.generated.h"

/**
 * Wall slide v2 (wall-slide-v2.md 6): keeps the momentum it is grabbed with, has little control of its own (Mario style) and a great
 * capacity to brake. Built on UGSTN_WallTraverseModeBase, so the capsule never rotates and the character moves on a proxy plane;
 * this class only supplies the control model: ComputeWallVelocity is UGSTN_WallControlUtils::StepSlideVelocity, fed with the stick
 * on the plane (H, V) and the brake decision (FGSTN_WallBrakeInput).
 *
 * Runs next to the legacy UGSTN_WallSlideMovementMode, which is untouched: register this one as "WallSlidingV2" on the Mover
 * component and pick which one the Falling entry transition targets (Tools/GAS/setup_wall_slide_modes.py --use v2|legacy).
 */
UCLASS(Blueprintable, BlueprintType)
class GASP_GIUSTINO2_API UGSTN_WallSlideV2MovementMode : public UGSTN_WallTraverseModeBase
{
	GENERATED_BODY()

	friend struct FGSTN_WallTraverseTestAccess;

public:
	UGSTN_WallSlideV2MovementMode(const FObjectInitializer& ObjectInitializer);

	/** Momentum in, horizontal drag and control, gravity toward a terminal speed, braking. See FGSTN_WallSlideControlSettings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control")
	FGSTN_WallSlideControlSettings Control;

	/** Which input brakes the vertical slide: stick against the motion / stick up / a held Mover tag (dedicated button) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control")
	FGSTN_WallBrakeInput Brake;

	/** Arrival velocity turned onto the plane keeping its length times Control.EntrySpeedKeep, clamped to Control.MaxEntrySpeed */
	virtual FVector ComputeEntryVelocity(const FVector& InVelocity, const FVector& WallNormal) const override;

protected:
	/**
	 * StepSlideVelocity on the plane (H = horizontal along the wall, V = along the gravity tangent, up positive). Gravity along the
	 * wall comes from the Mover component when there is one, so slanted walls slide slower and any gravity direction works
	 */
	virtual FVector ComputeWallVelocity(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent, float DeltaSeconds) const override;
};
