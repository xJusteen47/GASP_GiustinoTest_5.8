// Combat presentation (GAS-NPP fork): the greybox capsule's anim instance (greybox-capsula, owner 2026-11-24 2A). Never read by the simulation.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GSTN_GreyboxShapeSource.h"
#include "GSTN_GreyboxAnimInstance.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class UMoverComponent;
class USkeletalMesh;

/** How the capsule squashes, stretches and leans (class defaults of ABP_GSTN_GreyboxCapsule; the owner tunes them there) */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_GreyboxShapeSettings
{
	GENERATED_BODY()

	/** Stretch in the air per 1000 cm/s of vertical speed (up or down) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Squash", meta = (ClampMin = "0", ClampMax = "1"))
	float AirStretchPer1000 = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Squash", meta = (ClampMin = "1", ClampMax = "2"))
	float MaxStretch = 1.3f;

	/** Landing squash per 1000 cm/s of fall speed at touchdown (a kick into the spring, it bounces back) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Squash", meta = (ClampMin = "0", ClampMax = "1"))
	float LandSquashPer1000 = 0.45f;

	/** Squash when the ground speed jumps (a run start, a burst): per 1000 cm/s² of forward acceleration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Squash", meta = (ClampMin = "0", ClampMax = "1"))
	float AccelSquashPer1000 = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Squash", meta = (ClampMin = "0.3", ClampMax = "1"))
	float MinSquash = 0.6f;

	/** Lean into the horizontal acceleration, degrees per 1000 cm/s² (forward on a start, back on a brake, sideways in a turn) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Lean", meta = (ClampMin = "0", ClampMax = "20"))
	float LeanDegPer1000 = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Lean", meta = (ClampMin = "0", ClampMax = "45", ForceUnits = "deg"))
	float MaxLeanDeg = 18.f;

	/** Additive lean into curves (owner 2026-11-24): sideways lean per 1000 cm/s² of sideways (centripetal) acceleration, own cap */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Lean", meta = (ClampMin = "0", ClampMax = "30"))
	float TurnLeanDegPer1000 = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Lean", meta = (ClampMin = "0", ClampMax = "60", ForceUnits = "deg"))
	float MaxTurnLeanDeg = 25.f;

	/**
	 * Run stop plant (owner 2026-11-24, Mario 3D World Toad): when a run starts braking hard, the body kicks backward for a moment, as if a
	 * foot went forward, then the spring brings it back. Peak of the kick, degrees; 0 turns it off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Run Stop", meta = (ClampMin = "0", ClampMax = "60", ForceUnits = "deg"))
	float RunStopKickDeg = 22.f;

	/** The braking that starts a run stop: forward deceleration at least this... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Run Stop", meta = (ClampMin = "0", ForceUnits = "cm/s2"))
	float RunStopDecel = 1200.f;

	/** ...while moving at least this fast */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Run Stop", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float RunStopMinSpeed = 250.f;

	/** Spring of squash and lean: frequency (Hz) and damping ratio (< 1 bounces, 1 settles without overshoot) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Spring", meta = (ClampMin = "0.5", ClampMax = "20", ForceUnits = "Hz"))
	float SpringFrequency = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Spring", meta = (ClampMin = "0.05", ClampMax = "2"))
	float SpringDamping = 0.45f;

	/**
	 * Ground squash from the pose (run 2): on the ground the capsule is squashed by the driver's pelvis height over its reference height,
	 * so a crouch or a landing crouch reads as a shorter body, not a curled one. Lowest allowed ratio; 1 turns it off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Squash", meta = (ClampMin = "0.2", ClampMax = "1"))
	float MinPelvisSquash = 0.35f;

	/** How fast the pelvis squash follows the pose when it switches on or off (take off, touchdown), 1/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Squash", meta = (ClampMin = "1"))
	float PelvisSquashBlendSpeed = 20.f;

	/** Hand and foot markers (owner 2026-11-24: hidden in locomotion and most of the time): shown while an attack's capsule clip plays
	 *  (MoveOverrides / SequenceMoveOverrides) and/or in locomotion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Limbs")
	bool bShowLimbsInMoves = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Limbs")
	bool bShowLimbsInLocomotion = false;

	/** How fast the markers grow / shrink, 1/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Limbs", meta = (ClampMin = "1"))
	float LimbBlendSpeed = 15.f;

	//~ Run sway (owner 2026-11-24: "animate the run without the capsule using the bones, as expressive as possible only by swaying it";
	//~ "for the run start, speed the run up"). The capsule body stays rigid outside attacks; this is the whole locomotion read.

	/** Distance of one full stride cycle (two steps) at run speed: the sway cadence follows the ground speed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Run Sway", meta = (ClampMin = "20", ForceUnits = "cm"))
	float StrideLength = 170.f;

	/** Speed at which the sway reaches full amplitude (the run) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Run Sway", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float FullSwaySpeed = 500.f;

	/** Bob: squash on each footfall, twice per cycle (fraction of the height) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Run Sway", meta = (ClampMin = "0", ClampMax = "0.5"))
	float BobSquash = 0.07f;

	/** Side rock toward the stepping foot, once per cycle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Run Sway", meta = (ClampMin = "0", ClampMax = "45", ForceUnits = "deg"))
	float RockRollDeg = 7.f;

	/** Forward nod on each footfall, twice per cycle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Run Sway", meta = (ClampMin = "0", ClampMax = "45", ForceUnits = "deg"))
	float NodPitchDeg = 4.f;

	/** Body twist with the stride (the nose wiggles), once per cycle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Run Sway", meta = (ClampMin = "0", ClampMax = "45", ForceUnits = "deg"))
	float TwistYawDeg = 8.f;

	/** Frantic run start (run-start-frantic: Toad timing ~0.6 s): after leaving rest, the sway runs at this fixed cadence, not the speed's */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Run Sway", meta = (ClampMin = "0", ClampMax = "2", ForceUnits = "s"))
	float FranticSeconds = 0.6f;

	/** Cycles per second during the frantic start (a run is ~3 at 500 cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Run Sway", meta = (ClampMin = "0.5", ClampMax = "15", ForceUnits = "Hz"))
	float FranticCadenceHz = 6.f;

	/** Sway amplitude multiplier during the frantic start */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Run Sway", meta = (ClampMin = "0", ClampMax = "3"))
	float FranticAmplitude = 1.4f;

	/** Fade from the frantic cadence to the speed's cadence after FranticSeconds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Run Sway", meta = (ClampMin = "0.01", ClampMax = "1", ForceUnits = "s"))
	float FranticFadeSeconds = 0.15f;
};

/** Pure: a damped spring step (semi-implicit Euler, sub-stepped for long frames); the tests call it */
struct GSTNCOMBATNP_API FGSTN_GreyboxSpring
{
	float Value = 0.f;
	float Velocity = 0.f;

	void Step(float Target, float DeltaSeconds, float FrequencyHz, float Damping);
};

/**
 * Parent of ABP_GSTN_GreyboxCapsule, the anim Blueprint of the greybox capsule mesh (gstn.Greybox 1). Its graph copies the pose of the
 * invisible driver mesh it is attached to (Copy Pose From Mesh, bones matched by name), then GSTN Greybox Shape squashes and leans the whole
 * capsule around the feet with the values computed here from the owner's Mover velocity. So the capsule follows every animation the driver
 * plays (GASP, the simple graph, moves) and adds the cartoon read of speed changes on top. Hitboxes and hurtboxes read the driver only.
 *
 * Run 2 (2026-11-24): the capsule hangs from the pelvis (it can tilt and lie flat), and on the ground it is squashed by the driver's pelvis
 * height over its reference (PelvisSquash), so a crouch reads as a shorter body that still stands on the ground.
 */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GreyboxAnimInstance : public UAnimInstance, public IGSTN_GreyboxShapeSource
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Greybox")
	FGSTN_GreyboxShapeSettings ShapeSettings;

	/**
	 * Capsule versions of moves (3A): reference montage the driver plays (attack, hit reaction, roll) -> capsule clip shown at the montage's
	 * time, blended by its weight. The clip should last as long as the montage (the setup script checks it). Hitboxes keep reading the
	 * reference montage on the driver.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Greybox|Moves")
	TMap<TSoftObjectPtr<UAnimMontage>, TSoftObjectPtr<UAnimSequenceBase>> MoveOverrides;

	/**
	 * Same, keyed by the clip the montage plays (its first slot segment), for montages that are not assets: a move that plays a sequence
	 * as a dynamic montage (the Playtest jab, AnimMontage_0) has no montage to key. Asked when MoveOverrides has no entry; the time is
	 * the segment's position in that clip.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Greybox|Moves")
	TMap<TSoftObjectPtr<UAnimSequenceBase>, TSoftObjectPtr<UAnimSequenceBase>> SequenceMoveOverrides;

	virtual FGSTN_GreyboxShape GetGreyboxShape() const override { return Shape; }

	/** Pure: the squash target (1 = rest) for a vertical speed in the air and a forward acceleration on the ground */
	static float SquashTarget(const FGSTN_GreyboxShapeSettings& Settings, bool bOnGround, float VelocityZ, float ForwardAcceleration);

	/** Pure: the pelvis squash (1 = standing) for a pelvis height over its reference height; 1 in the air */
	static float PelvisSquashTarget(const FGSTN_GreyboxShapeSettings& Settings, bool bOnGround, float PelvisHeight, float ReferenceHeight);

	/** Pure: the limb marker scale target for the weight of an attack's capsule clip (0 = locomotion only) */
	static float LimbScaleTarget(const FGSTN_GreyboxShapeSettings& Settings, float AttackWeight);

	/** Pure: 0..1 how frantic the run start still is, TimeSinceStart seconds after leaving rest (negative = not started) */
	static float FranticAlpha(const FGSTN_GreyboxShapeSettings& Settings, float TimeSinceStart);

	/** Pure: sway cycles per second for a ground speed and a frantic alpha */
	static float SwayCadenceHz(const FGSTN_GreyboxShapeSettings& Settings, float Speed2D, float Frantic);

	/** Pure: the sway at a cycle phase (radians) and amplitude: X squash delta, Y roll, Z twist yaw (degrees); OutPitch the nod */
	static FVector RunSway(const FGSTN_GreyboxShapeSettings& Settings, float Phase, float Amplitude, float& OutPitch);

	/** Pure: does this frame start a run stop (braking hard from a run, not already kicked)? */
	static bool StartsRunStop(const FGSTN_GreyboxShapeSettings& Settings, bool bOnGround, float Speed2D, float ForwardAcceleration, bool bAlreadyKicked);

	/** Pure: the lean target (pitch forward, roll right, degrees) for a horizontal acceleration in the actor's local frame; forward and
	 *  sideways (curves) have their own rates and caps */
	static FRotator LeanTarget(const FGSTN_GreyboxShapeSettings& Settings, const FVector& LocalAcceleration);

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	/** The move override of this frame from the driver's active montage */
	void UpdateMoveOverride();


	/** The driver's pelvis height above the component (its feet) now, and in its reference pose; false without a driver or a pelvis */
	bool GetDriverPelvisHeight(float& OutHeight, float& OutReference);

	/** Loaded override clips, by reference montage asset (kept referenced: the anim node holds raw pointers) */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UAnimMontage>, TObjectPtr<UAnimSequenceBase>> LoadedOverrides;

	/** Loaded override clips, by the played clip (dynamic montages are new objects each time, so they are never keys) */
	UPROPERTY(Transient)
	TMap<TObjectPtr<UAnimSequenceBase>, TObjectPtr<UAnimSequenceBase>> LoadedSequenceOverrides;

	TWeakObjectPtr<UMoverComponent> Mover;
	FGSTN_GreyboxShape Shape;
	FGSTN_GreyboxSpring SquashSpring;
	FGSTN_GreyboxSpring PitchSpring;
	FGSTN_GreyboxSpring RollSpring;
	FVector PreviousVelocity = FVector::ZeroVector;
	bool bHasPrevious = false;
	bool bWasOnGround = true;
	float LastAirVelocityZ = 0.f;
	/** Run sway cycle phase (radians) and its smoothed amplitude */
	float SwayPhase = 0.f;
	float SwayAmplitude = 0.f;
	/** Seconds since the body last left rest on the ground; < 0 while at rest or in the air */
	float TimeSinceStart = -1.f;
	/** The current run stop already kicked (re-armed once still or speeding up) */
	bool bRunStopKicked = false;
	/** Driver pelvis reference height, cached per driver mesh asset */
	TWeakObjectPtr<const USkeletalMesh> PelvisReferenceMesh;
	float PelvisReferenceHeight = 0.f;
};
