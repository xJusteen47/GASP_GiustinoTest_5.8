// Pure combat logic (no GAS, no Network Prediction): the analog walk start and the air turn of a kit's locomotion profile
// (owner playtest 2026-11-16: "a small stick tilt must walk slowly; the start should feel like Mario / Astro Bot; in the air you can turn").

#pragma once

#include "CoreMinimal.h"
#include "GSTN_AnalogWalkTypes.generated.h"

/**
 * How the move stick becomes walking speed and how the walk starts. Values only, copied out of the profile asset (UGSTN_LocomotionProfile)
 * like the run stages. Speeds stay the run stages' (stage 1 is the walk/jog the stick tilt scales); nothing here is a Project Setting.
 *
 *  - Analog: the Mover move intent's length follows the stick tilt through a curve, so the stage's top speed is reached only at full
 *    tilt and a light tilt walks slowly. The intent is written into the input command (predicted with it), never into the sim state.
 *  - Walk start: while the ground speed is low (under LowSpeedFraction of the stage top speed) the acceleration and the turning are
 *    raised, so the character answers the stick at once and turns on the spot, then settles to the stage values as it speeds up.
 *  - Air turn: the facing follows the stick in the air at AirTurnRateDegPerSec (Mover's falling mode keeps its own air control of the
 *    velocity; this is only where the character looks, which is where the next aerial points).
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_AnalogWalkTuning
{
	GENERATED_BODY()

	/** Off = the move intent and the stage values are left exactly as before this feature */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk", meta = (DisplayName = "Analog Walk Enabled"))
	bool bEnabled = true;

	/**
	 * A second dead zone on top of the player's (UGSTN_InputOptionsSubsystem, applied before this reads the stick). 0 by default since
	 * 2026-11-17: two dead zones in series made a light tilt do nothing (owner playtest 2026-11-16)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk|Stick", meta = (EditCondition = "bEnabled", ClampMin = "0", ClampMax = "0.9"))
	float InnerDeadzone = 0.f;

	/** Fraction of the top speed asked for just past the dead zone (the slowest walk) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk|Stick", meta = (EditCondition = "bEnabled", ClampMin = "0", ClampMax = "1"))
	float MinSpeedFraction = 0.f;

	/** Shape of the tilt curve: 1 = linear, above 1 = finer control of the slow walks */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk|Stick", meta = (EditCondition = "bEnabled", ClampMin = "0.25", ClampMax = "4"))
	float ResponseExponent = 1.4f;

	/** Stick length from which the full top speed is asked (lets a worn stick that never reaches 1 still run) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk|Stick", meta = (EditCondition = "bEnabled", ClampMin = "0.5", ClampMax = "1"))
	float FullTiltAt = 0.95f;

	/** Below this fraction of the stage top speed the walk-start boosts apply (fading out linearly towards it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk|Walk Start", meta = (EditCondition = "bEnabled", ClampMin = "0.01", ClampMax = "1"))
	float LowSpeedFraction = 0.5f;

	/**
	 * Acceleration multiplier at rest (1 = the stage's own acceleration). 1 by default since 2026-11-17: the start is shaped by the run
	 * stage's acceleration ramp (FGSTN_RunStage::AccelRampFrames), not by a jump in acceleration (owner: "no step")
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk|Walk Start", meta = (EditCondition = "bEnabled", ClampMin = "1", ClampMax = "10"))
	float StartAccelerationMultiplier = 1.f;

	/** Turning strength at rest (Mover's TurningStrength; the stage's own value takes over as the speed rises) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk|Walk Start", meta = (EditCondition = "bEnabled", ClampMin = "0"))
	float StartTurningStrength = 30.f;

	/**
	 * Walking mode properties that also get the stage speed / acceleration, so GASP's walk gait (picked for a light tilt) does not cap
	 * the speed at its own WalkSpeed: the tilt alone decides. Names the mode does not have are skipped
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode Bindings|Walking Mode", AdvancedDisplay)
	TArray<FName> WalkSpeedProperties = { TEXT("WalkSpeed") };

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode Bindings|Walking Mode", AdvancedDisplay)
	TArray<FName> WalkAccelerationProperties = { TEXT("WalkAcceleration") };

	/**
	 * Ground facing (owner 2026-11-16: "the character takes a long time to turn towards the stick"). GASP's walking mode smooths the
	 * facing with a spring of FacingSmoothingTime seconds (0.25 in Mover, more in GASP's tuning); these replace it: StartFacingTime at
	 * rest (a near-instant pivot), RunFacingTime from LowSpeedFraction of the top speed up. Lower = snappier. < 0 = leave the mode's own
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk|Ground Turn", meta = (EditCondition = "bEnabled", ClampMin = "-1", ClampMax = "1", ForceUnits = "s"))
	float StartFacingTime = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk|Ground Turn", meta = (EditCondition = "bEnabled", ClampMin = "-1", ClampMax = "1", ForceUnits = "s"))
	float RunFacingTime = 0.08f;

	/** The walking mode's facing smoothing property (USmoothWalkingMode) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode Bindings|Walking Mode", AdvancedDisplay)
	TArray<FName> FacingTimeProperties = { TEXT("FacingSmoothingTime") };

	/**
	 * Smoothing of the acceleration itself (USmoothWalkingMode's AccelerationSmoothingTime, 0.1 s in Mover): a lag before the walk starts
	 * answering. < 0 = leave the mode's own
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Walk|Walk Start", meta = (EditCondition = "bEnabled", ClampMin = "-1", ClampMax = "1", ForceUnits = "s"))
	float AccelerationSmoothingTime = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mode Bindings|Walking Mode", AdvancedDisplay)
	TArray<FName> AccelerationSmoothingProperties = { TEXT("AccelerationSmoothingTime") };

	/**
	 * How fast the facing follows the stick in the air, degrees per second. 0 = no turning in the air. Shown under Air but part of the
	 * analog walk: no air turn while Analog Walk Enabled is off (the combat sim reads it only through an enabled analog walk)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|Air Turn", meta = (DisplayName = "Air Turn Rate", ClampMin = "0", ForceUnits = "deg/s"))
	float AirTurnRateDegPerSec = 540.f;

	/** In the air during a move (attack timeline): turn too? Off = the move keeps its facing (its own steer rules still apply) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Air|Air Turn")
	bool bAirTurnDuringMoves = false;
};

/** The analog rules, pure: same input, same output on every machine */
struct GSTNCOMBATCORE_API FGSTN_AnalogWalkLogic
{
	/** Length of the move intent (0..1) for a stick of length StickSize (0..1) */
	static float StickToIntent(const FGSTN_AnalogWalkTuning& Tuning, float StickSize);

	/** 1 at rest, 0 from LowSpeedFraction of the top speed up: how much of the walk-start boost applies */
	static float StartBlend(const FGSTN_AnalogWalkTuning& Tuning, float GroundSpeed, float TopSpeed);

	/** The yaw after one air-turn step from CurrentYaw towards TargetYaw (degrees, shortest way) */
	static float StepAirYaw(const FGSTN_AnalogWalkTuning& Tuning, float CurrentYaw, float TargetYaw, float DeltaSeconds);

	/**
	 * Yaw speed (deg/s, signed about the local up) that turns CurrentYaw toward TargetYaw this step, capped at RateDegPerSec: the falling
	 * mode's turn rule with the profile's air turn rate (m3-air-turn 2026-11-17). GASP's BP_MovementMode_Falling recomputes the turn
	 * with a fixed 300 deg/s cap after Mover's own, so the air turn sets it again from this. 0 when Rate or DeltaSeconds is not positive
	 */
	static float AirTurnYawRate(float RateDegPerSec, float CurrentYaw, float TargetYaw, float DeltaSeconds);
};
