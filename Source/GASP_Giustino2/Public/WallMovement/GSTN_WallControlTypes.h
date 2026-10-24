// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_WallControlTypes.generated.h"

/**
 * Control model of the wall slide (wall-slide-v2.md 6.2): momentum in, little control, strong braking.
 * Velocity lives on the movement plane as (H, V): H along the horizontal axis of the wall, V along the gravity-tangent
 * (up positive). Everything here is a rate or a speed, so the model does not depend on which way gravity points.
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallSlideControlSettings
{
	GENERATED_BODY()

	/** Fraction of the incoming speed kept when grabbing the wall (1 = the whole run and fall) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Entry", meta = (ClampMin = "0", ClampMax = "1"))
	float EntrySpeedKeep = 1.f;

	/** Upper bound of the speed after entry (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Entry", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float MaxEntrySpeed = 1500.f;

	/** Exponential decay rate of the horizontal momentum (1/s): low, momentum lasts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Horizontal", meta = (ClampMin = "0"))
	float HorizontalDrag = 0.4f;

	/** Acceleration of stick input along the current horizontal motion (cm/s^2): low, Mario style */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Horizontal", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float HorizontalControlAccel = 300.f;

	/** Input never accelerates the horizontal speed beyond this (cm/s); entry momentum above it is only decayed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Horizontal", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float HorizontalControlMaxSpeed = 350.f;

	/** Deceleration when the stick is against the horizontal motion (cm/s^2): high */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Horizontal", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float HorizontalBrakeDecel = 2400.f;

	/** Gravity along the wall in cm/s^2 before GravityScale (fed from the world gravity by the caller) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Vertical", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float GravityAcceleration = 980.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Vertical", meta = (ClampMin = "0"))
	float GravityScale = 1.f;

	/** Downward speed the slide approaches softly (cm/s). Faster entry speed decays toward it, never abruptly */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Vertical", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float TerminalDownSpeed = 600.f;

	/** Extra terminal speed with the stick fully down: "let go faster" (cm/s, 0 = off) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Vertical", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float DownBoost = 0.f;

	/** Deceleration of the vertical brake (cm/s^2): high */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Vertical", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float VerticalBrakeDecel = 2600.f;

	/**
	 * The vertical brake stops decelerating at this downward speed and holds it, no gravity gain (cm/s). Above zero on purpose: the slide
	 * never freezes on the wall (a slower or upward speed under the brake is brought to this one), only a climb mode holds still
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Vertical", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float BrakeMinSpeed = 80.f;

	/**
	 * Option: pushing the stick into the wall scales the terminal down speed by this (1 = off, the default: the stick component into the wall
	 * is simply ignored). It only slows the approach to a lower terminal speed, it never brakes
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Vertical", meta = (ClampMin = "0", ClampMax = "1"))
	float HoldTowardWallTerminalScale = 1.f;

	/** Stick magnitude below this counts as no input */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control", meta = (ClampMin = "0", ClampMax = "1"))
	float InputDeadzone = 0.2f;
};

/**
 * Which input brakes the vertical slide; data so the input layer and the ability decide (wall-slide-v2.md 6.2).
 * Resolved by UGSTN_WallControlUtils::ResolveBrake into the bBrake of StepSlideVelocity.
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallBrakeInput
{
	GENERATED_BODY()

	/** Stick pushed against the current vertical motion brakes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Brake")
	bool bStickAgainstMotion = true;

	/** Stick up brakes regardless of the motion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Brake")
	bool bStickUp = false;

	/** Stick magnitude needed on the vertical axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Brake", meta = (ClampMin = "0", ClampMax = "1"))
	float StickThreshold = 0.3f;

	/** Optional: a held input tag (dedicated brake button) brakes; evaluated by the caller and passed as bTagActive */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Brake")
	FGameplayTag BrakeInputTag;
};

/** Control model of the wall climb: full 2D control on the plane, no gravity slide (wall-slide-v2.md 6.3) */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallClimbControlSettings
{
	GENERATED_BODY()

	/** Speed at full stick (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Climb", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float ClimbSpeed = 250.f;

	/** Acceleration toward the stick target (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Climb", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float ClimbAccel = 1500.f;

	/** Deceleration when releasing the stick or asking for less speed than the current (cm/s^2) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Climb", meta = (ClampMin = "0", ForceUnits = "cm/s^2"))
	float ClimbBrake = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control|Climb", meta = (ClampMin = "0", ClampMax = "1"))
	float InputDeadzone = 0.1f;
};
