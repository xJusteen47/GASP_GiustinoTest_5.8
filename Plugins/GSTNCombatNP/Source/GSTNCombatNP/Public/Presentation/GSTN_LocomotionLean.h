// Combat presentation (GAS-NPP fork): additive locomotion lean and the locomotion animation profile (docs/notes/additive-lean-1.md). Never read by
// the simulation.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_AnimSetTypes.h"
#include "GSTN_LocomotionLean.generated.h"

/**
 * The additive lean while moving (owner 2026-11-18, "Mario / Astro Bot" feel): the torso leans into the acceleration of the presented
 * velocity, so into a turn (the centripetal part, speed x turn rate), forward when speeding up and back when braking. It bends the spine on
 * top of GASP's motion matched pose (the legs, the root and the foot lock keep GASP's pose), through the directional additive node already in
 * the main AnimGraph. Angles are summed over the spine chain (spine_01..head).
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_LocomotionLeanSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean")
	bool bEnabled = true;

	/** Largest lean, degrees summed over the spine chain */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean", meta = (ClampMin = "0.0", ClampMax = "45.0", ForceUnits = "deg"))
	float MaxAngleDeg = 14.f;

	/** Horizontal acceleration that reaches MaxAngleDeg (cm/s^2). Lower = leans more easily */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean", meta = (ClampMin = "100.0", ForceUnits = "cm/s^2"))
	float AccelForMaxLean = 3000.f;

	/** Share of the turn (sideways) part: 1 = full */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float TurnScale = 1.f;

	/** Share of the speed up / brake (along the run) part: GASP's start and stop clips already lean, so less than the turn */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float SpeedChangeScale = 0.5f;

	/** No lean under this speed, full lean from FullLeanSpeed (cm/s): standing turns do not tilt the body */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MinSpeed = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float FullLeanSpeed = 350.f;

	/** Scale per run stage (index = stage: run, sprint 1, sprint 2...; a missing stage uses the last entry) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean")
	TArray<float> StageScale = { 1.f, 1.15f, 1.3f };

	/** Smoothing of the measured acceleration (s): the presented velocity steps at the 62.5 Hz simulation rate */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean|Feel", meta = (ClampMin = "0.0", ForceUnits = "s"))
	float AccelSmoothingSeconds = 0.06f;

	/** The lean follows its target as a spring: frequency (Hz, higher = snappier) and damping (1 = no overshoot, lower = follow-through) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean|Feel", meta = (ClampMin = "0.5", ClampMax = "12.0"))
	float SpringFrequency = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean|Feel", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float SpringDamping = 0.65f;

	/** Scale in the air (0 = the lean springs back to straight after take off) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean|States", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AirScale = 0.f;

	/** Scale during a move timeline (attacks): 0 = the attack's own pose is untouched */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean|States", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AttackScale = 0.f;

	/** Scale during a run skid or an attack slide (the skid has its own lean and clip) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lean|States", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SlideScale = 0.f;

	float GetStageScale(int32 Stage) const
	{
		return StageScale.Num() == 0 ? 1.f : StageScale[FMath::Clamp(Stage, 0, StageScale.Num() - 1)];
	}
};

/**
 * The LOCOMOTION ANIMATION PROFILE (owner 2026-11-18: locomotion animation depends on the character, body, weight and state, not only on the
 * kit). It is an anim set in slot Anim.Set.Locomotion, so it is chosen like every other anim set (UGSTN_AnimSetComponent): the character's
 * collection by default, a kit, weapon or skin collection above it, variants by cosmetic tags (Anim.Trait.Build.Heavy...). Presentation only;
 * the kit's FGSTN_LocomotionSimTuning keeps the gameplay numbers.
 *
 * Today it holds the lean. Planned sections (docs/notes/additive-lean-1.md): the GASP locomotion set (chooser table / pose search databases),
 * the skid and run start presentation values (their clips already live in the Anim.Set.Skid / Anim.Set.RunStart sets).
 */
UCLASS(BlueprintType)
class GSTNCOMBATNP_API UGSTN_LocomotionAnimSet : public UGSTN_AnimSetDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Locomotion")
	FGSTN_LocomotionLeanSettings Lean;
};

/** The lean's state between frames, game thread. Pure but for the inputs: the tests drive it */
struct GSTNCOMBATNP_API FGSTN_LocomotionLeanSolver
{
	/** Current lean: horizontal world vector, length = degrees, direction = where the upper body goes */
	FVector Lean = FVector::ZeroVector;
	FVector LeanRate = FVector::ZeroVector;
	FVector FilteredAccel = FVector::ZeroVector;
	FVector PrevVelocity = FVector::ZeroVector;
	bool bHasPrev = false;

	/**
	 * One presented frame. Velocity: presented world velocity; Forward: the facing (used while too slow for a heading); StateScale: run stage
	 * x air / attack / slide scale. Returns Lean
	 */
	FVector Step(const FGSTN_LocomotionLeanSettings& Settings, const FVector& Velocity, const FVector& Forward, float DeltaSeconds, float StateScale);

	/** The target lean of an acceleration at a speed (no smoothing): the tests check the directions on it */
	static FVector TargetLean(const FGSTN_LocomotionLeanSettings& Settings, const FVector& HorizontalVelocity, const FVector& HorizontalAccel,
		const FVector& Forward, float StateScale);

	void Reset() { *this = FGSTN_LocomotionLeanSolver(); }
};
