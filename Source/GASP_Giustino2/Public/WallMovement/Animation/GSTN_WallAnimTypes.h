// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "GSTN_WallAnimTypes.generated.h"

class UBaseMovementMode;

/**
 * Presentation data of the wall modes for the animation graph (wall-anim.md). Nothing here is simulated: it is derived on the game
 * thread, every frame, from the Mover presentation state (the smoothed sync state on simulated proxies), so it never takes part in
 * prediction or rollback and may keep its own timers. The movement modes do not know about it.
 */

/** Where the character is in the life of one wall attachment. Drives the outer state machine of the wall anim layer */
UENUM(BlueprintType)
enum class EGSTN_WallAnimPhase : uint8
{
	/** Not on a wall and no exit playing: the wall layer is not shown */
	None,
	/** Just grabbed a wall (from a mode that is not a wall mode) for Settings.EnterDuration */
	Enter,
	/** On the wall: the loop states (EGSTN_WallSlideAnimState) play */
	Active,
	/** Just left the wall for Settings.ExitDuration; the mode that took over is FGSTN_WallAnimState::ExitModeName */
	Exit
};

/** Which loop plays while the phase is Active (and Enter, for an enter clip that wants to pick its variant) */
UENUM(BlueprintType)
enum class EGSTN_WallSlideAnimState : uint8
{
	/** Little or no motion along the wall horizontally: the "stationary" slide (still, or sliding straight down) */
	Idle,
	/** Moving to the character's left along the wall (the character faces the wall) */
	SlideLeft,
	/** Moving to the character's right along the wall */
	SlideRight,
	/** Fast on the wall (above Settings.SkidEnterSpeed): the braking skid. Wins over left and right */
	Skid
};

/** Thresholds and durations of the wall anim state. Speeds are on the wall plane (cm/s); every "exit" speed is lower than its "enter" for hysteresis */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallAnimSettings
{
	GENERATED_BODY()

	/**
	 * Active movement modes of this class (or a child) count as "on a wall". Default: the v2 family (slide and climb), so an in-place switch between
	 * slide and climb is not an exit and a new entry. Set UGSTN_ControlledWallMovementMode to include the legacy modes too
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation")
	TSubclassOf<UBaseMovementMode> WallModeClass;

	/** Length of the Enter phase (s). Match the enter clip; 0 = no enter phase, straight to Active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation|Phase", meta = (ClampMin = "0", ForceUnits = "s"))
	float EnterDuration = 0.2f;

	/** Length of the Exit phase (s). Match the exit clip; 0 = no exit phase, straight to None */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation|Phase", meta = (ClampMin = "0", ForceUnits = "s"))
	float ExitDuration = 0.2f;

	/** Speed on the wall plane at which the skid starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation|Skid", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SkidEnterSpeed = 750.f;

	/** Speed on the wall plane below which the skid ends (keep it under SkidEnterSpeed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation|Skid", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float SkidExitSpeed = 650.f;

	/** The climb mode never skids by default (it has no momentum slide) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation|Skid")
	bool bSkidWhileClimbing = false;

	/** Horizontal speed along the wall at which SlideLeft / SlideRight start */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation|Lateral", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float LateralEnterSpeed = 120.f;

	/** Horizontal speed along the wall below which the slide goes back to Idle (keep it under LateralEnterSpeed) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation|Lateral", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float LateralExitSpeed = 80.f;

	/** Horizontal speed that maps to LateralAlpha = +-1 (blend spaces) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation|Blend", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float LateralReferenceSpeed = 350.f;

	/** Speed along the gravity tangent that maps to VerticalAlpha = +-1 (blend spaces) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation|Blend", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float VerticalReferenceSpeed = 600.f;

	FGSTN_WallAnimSettings();
};

/** What the anim graph reads (property access, thread safe: it is a plain copy made on the game thread before the graph runs) */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallAnimState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Wall|Animation")
	EGSTN_WallAnimPhase Phase = EGSTN_WallAnimPhase::None;

	/** Time spent in the current Phase (s) */
	UPROPERTY(BlueprintReadOnly, Category = "Wall|Animation")
	float PhaseTime = 0.f;

	/** Loop state. Kept as it was during Exit, so the exit clip can pick its variant */
	UPROPERTY(BlueprintReadOnly, Category = "Wall|Animation")
	EGSTN_WallSlideAnimState SlideState = EGSTN_WallSlideAnimState::Idle;

	/** Time spent in the current SlideState (s), reset by every change. Useful for a short blend-in of each loop */
	UPROPERTY(BlueprintReadOnly, Category = "Wall|Animation")
	float SlideStateTime = 0.f;

	/** True while the wall layer should be shown (Phase is not None): the bool of the main graph's blend */
	UPROPERTY(BlueprintReadOnly, Category = "Wall|Animation")
	bool bIsActive = false;

	/** The current wall mode is a climb mode (UGSTN_WallClimbV2MovementMode or a child). Kept during Exit */
	UPROPERTY(BlueprintReadOnly, Category = "Wall|Animation")
	bool bIsClimbing = false;

	/** Signed horizontal speed along the wall (cm/s), positive = to the character's right */
	UPROPERTY(BlueprintReadOnly, Category = "Wall|Animation")
	float LateralSpeed = 0.f;

	/** Signed speed along the gravity tangent of the wall (cm/s), positive = up */
	UPROPERTY(BlueprintReadOnly, Category = "Wall|Animation")
	float VerticalSpeed = 0.f;

	/** Length of the velocity on the wall plane (cm/s) */
	UPROPERTY(BlueprintReadOnly, Category = "Wall|Animation")
	float PlaneSpeed = 0.f;

	/** LateralSpeed / LateralReferenceSpeed, clamped to [-1, 1] */
	UPROPERTY(BlueprintReadOnly, Category = "Wall|Animation")
	float LateralAlpha = 0.f;

	/** VerticalSpeed / VerticalReferenceSpeed, clamped to [-1, 1] */
	UPROPERTY(BlueprintReadOnly, Category = "Wall|Animation")
	float VerticalAlpha = 0.f;

	/** Mode that took over when the wall was left (set on entering Exit): lets the exit pick floor, air or jump variants */
	UPROPERTY(BlueprintReadOnly, Category = "Wall|Animation")
	FName ExitModeName = NAME_None;
};

/** One frame of input to the wall anim state, gathered from the Mover component (or built by a test) */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallAnimInputs
{
	GENERATED_BODY()

	/** The active mode is a wall mode (FGSTN_WallAnimSettings::WallModeClass) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation")
	bool bOnWall = false;

	/** The active mode is a climb mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation")
	bool bIsClimbing = false;

	/** Name of the active mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation")
	FName ModeName = NAME_None;

	/** Presentation velocity (world, cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation")
	FVector Velocity = FVector::ZeroVector;

	/** Filtered wall normal of the wall sync state (the movement plane). Zero = unknown: the classification falls back to FallbackFacing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation")
	FVector WallNormal = FVector::ZeroVector;

	/** The character's forward, used as -WallNormal when the normal is unknown */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation")
	FVector FallbackFacing = FVector::ForwardVector;

	/** Gravity-up of the Mover component (never assume world up: gravity can point anywhere) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Animation")
	FVector GravityUp = FVector::UpVector;
};
