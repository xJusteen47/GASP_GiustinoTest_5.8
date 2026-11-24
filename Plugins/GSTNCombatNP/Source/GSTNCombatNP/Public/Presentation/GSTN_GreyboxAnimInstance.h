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
class UGSTN_RunPresentationComponent;
struct FAnimMontageInstance;
struct FAnimSegment;

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
	float LandSquashPer1000 = 0.6f;

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

	/**
	 * Skid (owner 2026-11-24, Toad / Luigi reference): it starts as the run stop (the backward plant), the capsule keeps facing the old run
	 * while the run presentation's skid lasts, then turns to the new facing on this spring and starts running (frantic start again).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Skid")
	bool bSkidHoldsFacing = true;

	/** Turn after the skid: spring frequency (Hz) and damping (< 1 overshoots a little, cartoon) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Skid", meta = (ClampMin = "0.5", ClampMax = "20", ForceUnits = "Hz"))
	float SkidTurnFrequency = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Skid", meta = (ClampMin = "0.05", ClampMax = "2"))
	float SkidTurnDamping = 0.6f;

	/**
	 * Jump squat (owner 2026-11-24 14:35; the sim's 3 squat frames before the take-off, FGSTN_JumpState::SquatFramesLeft): the capsule
	 * squashes to this fast, then the take-off releases the spring into the air stretch
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Jump Squat", meta = (ClampMin = "0.3", ClampMax = "1"))
	float JumpSquatSquash = 0.72f;

	/** How fast the squat squash is reached, 1/s (the squat lasts ~50 ms) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Jump Squat", meta = (ClampMin = "1"))
	float JumpSquatBlendSpeed = 45.f;

	/**
	 * Wall movement (owner 2026-11-24 14:35): Mover modes that are a wall slide or climb. On a wall the capsule leans its top away from the
	 * wall (the bottom presses it), judders with the slide's friction, leans into a sideways push like on the ground, and leans back
	 * against the slide while it brakes (owner 15:26: "it should simulate the friction on the wall", not a sway)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Wall")
	TArray<FName> WallModeNames = {TEXT("WallSliding"), TEXT("WallSlidingV2"), TEXT("WallClimbingV2")};

	/** Lean of the top away from the wall while on it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Wall", meta = (ClampMin = "0", ClampMax = "45", ForceUnits = "deg"))
	float WallLeanDeg = 10.f;

	/**
	 * Friction judder (stick-slip): the body catches on the wall and slips, a sawtooth along the slide (slow build, quick release), small and
	 * fast, never a side-to-side sway. Tip at WallJudderSpeed, in degrees, plus a squash pulse; the frequency rises with the speed
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Wall", meta = (ClampMin = "0", ClampMax = "10", ForceUnits = "deg"))
	float WallJudderDeg = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Wall", meta = (ClampMin = "0", ClampMax = "0.2"))
	float WallJudderSquash = 0.025f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Wall", meta = (ClampMin = "1", ForceUnits = "cm/s"))
	float WallJudderSpeed = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Wall", meta = (ClampMin = "1", ClampMax = "40", ForceUnits = "Hz"))
	float WallJudderMinHz = 9.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Wall", meta = (ClampMin = "1", ClampMax = "40", ForceUnits = "Hz"))
	float WallJudderMaxHz = 20.f;

	/** Braking on the wall grips harder: the judder grows by this share at full brake (WallBrakeFullDecel) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Wall", meta = (ClampMin = "0", ClampMax = "3"))
	float WallJudderBrakeBoost = 1.f;

	/**
	 * Wall brake (the wall slide skid): a held lean back against the slide for as long as it decelerates, as if a foot dug in toward the
	 * motion, degrees per 1000 cm/s² of deceleration up to WallBrakeMaxLeanDeg. Held, not kicked: the lean springs are damped to
	 * WallSpringDamping on a wall so it does not bounce
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Wall", meta = (ClampMin = "0", ClampMax = "30"))
	float WallBrakeLeanPer1000 = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Wall", meta = (ClampMin = "0", ClampMax = "45", ForceUnits = "deg"))
	float WallBrakeMaxLeanDeg = 18.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Wall", meta = (ClampMin = "1", ForceUnits = "cm/s2"))
	float WallBrakeFullDecel = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Wall", meta = (ClampMin = "0.05", ClampMax = "2"))
	float WallSpringDamping = 1.f;

	/** How far the wall is looked for around the capsule (presentation traces, only while in a wall mode) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Wall", meta = (ClampMin = "10", ForceUnits = "cm"))
	float WallProbeDistance = 90.f;

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

	/**
	 * Lying (owner 2026-11-24: "the capsule should turn when it is on the ground, it squashes strangely now"). The rigid capsule takes the
	 * driver's pelvis once that pelvis tips past LyingStartDeg from upright (knockdown, get-ups, rolls, a tumble), fully at LyingFullDeg, so it
	 * lies down and rolls as a whole body instead of standing squashed to the lying pelvis's height. Off = the old upright capsule
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Lying")
	bool bLyingFollowsPelvis = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Lying", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "deg"))
	float LyingStartDeg = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Lying", meta = (ClampMin = "0", ClampMax = "90", ForceUnits = "deg"))
	float LyingFullDeg = 65.f;

	/** How fast the capsule goes from upright to lying and back, 1/s (high: a roll must not lag behind the driver) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Lying", meta = (ClampMin = "1"))
	float LyingBlendSpeed = 18.f;

	/**
	 * Keep the posed capsule out of the floor on the ground (a lying capsule is as thick as its radius, a lying mannequin's pelvis is lower).
	 * The capsule's reference geometry, cm above the feet: the setup script's GREYBOX_SHAPE (radius, bottom, top)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Lying")
	bool bKeepAboveFloor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Lying", meta = (ClampMin = "1", ForceUnits = "cm"))
	float CapsuleRadius = 34.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Lying", meta = (ClampMin = "0", ForceUnits = "cm"))
	float CapsuleBottom = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Lying", meta = (ClampMin = "1", ForceUnits = "cm"))
	float CapsuleTop = 156.f;

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

	/** Sway amplitude during the frantic start (replaces the speed's amplitude, 1 = the full run's; owner 2026-11-24 14:32: smaller) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Run Sway", meta = (ClampMin = "0", ClampMax = "3"))
	float FranticAmplitude = 0.45f;

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
	 * Same, keyed by the sequence the montage plays at its current time (the segment under it, first slot), for montages that are not
	 * assets (a sequence played as a dynamic montage has no montage to key) and for montages built from several sequences (run 3: the
	 * dropkick's startup, air loop and land each get their own clip, asked before the montage's entry). The time is the segment's position
	 * in that sequence, scaled to the capsule clip's length.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Greybox|Moves")
	TMap<TSoftObjectPtr<UAnimSequenceBase>, TSoftObjectPtr<UAnimSequenceBase>> SequenceMoveOverrides;

	virtual FGSTN_GreyboxShape GetGreyboxShape() const override { return Shape; }

	/** Pure: the squash target (1 = rest) for a vertical speed in the air and a forward acceleration on the ground */
	static float SquashTarget(const FGSTN_GreyboxShapeSettings& Settings, bool bOnGround, float VelocityZ, float ForwardAcceleration);

	/** Pure: the pelvis squash (1 = standing) for a pelvis height over its reference height; 1 in the air */
	static float PelvisSquashTarget(const FGSTN_GreyboxShapeSettings& Settings, bool bOnGround, float PelvisHeight, float ReferenceHeight);

	/** Pure: the lying target 0..1 for how upright the driver's pelvis is (UpZ = the Z of its body-up axis: 1 upright, 0 flat, < 0 upside down) */
	static float LyingTarget(const FGSTN_GreyboxShapeSettings& Settings, float UpZ);

	/** Pure: the limb marker scale target for the weight of an attack's capsule clip (0 = locomotion only) */
	static float LimbScaleTarget(const FGSTN_GreyboxShapeSettings& Settings, float AttackWeight);

	/** Pure: 0..1 how frantic the run start still is, TimeSinceStart seconds after leaving rest (negative = not started) */
	static float FranticAlpha(const FGSTN_GreyboxShapeSettings& Settings, float TimeSinceStart);

	/** Pure: sway cycles per second for a ground speed and a frantic alpha */
	static float SwayCadenceHz(const FGSTN_GreyboxShapeSettings& Settings, float Speed2D, float Frantic);

	/** Pure: the sway amplitude goal for a ground speed and a frantic alpha (the frantic start blends to FranticAmplitude, not max) */
	static float SwayAmplitudeTarget(const FGSTN_GreyboxShapeSettings& Settings, float Speed2D, float Frantic);

	/** Pure: the sway at a cycle phase (radians) and amplitude: X squash delta, Y roll, Z twist yaw (degrees); OutPitch the nod */
	static FVector RunSway(const FGSTN_GreyboxShapeSettings& Settings, float Phase, float Amplitude, float& OutPitch);

	/**
	 * Pure: the plant kick of a hard brake as a lean (pitch, roll, degrees in the body frame), from the braking motion's direction in that
	 * frame (owner 2026-11-24 14:35: the body tips back against the motion as if the leg on the side it moves toward went in front of it to
	 * stop faster): braking a forward run tips the top back, braking a run to the right tips it left
	 */
	static FRotator PlantLean(const FVector& LocalMoveDirection, float KickDeg);

	/** Pure: stick-slip sawtooth at a cycle fraction 0..1: rises slowly while the body catches, drops fast when it slips; -0.5..0.5 */
	static float StickSlip(float CycleFraction);

	/** Pure: does this frame start a run stop (braking hard from a run, not already kicked)? */
	static bool StartsRunStop(const FGSTN_GreyboxShapeSettings& Settings, bool bOnGround, float Speed2D, float ForwardAcceleration, bool bAlreadyKicked);

	/** Pure: the lean target (pitch forward, roll right, degrees) for a horizontal acceleration in the actor's local frame; forward and
	 *  sideways (curves) have their own rates and caps */
	static FRotator LeanTarget(const FGSTN_GreyboxShapeSettings& Settings, const FVector& LocalAcceleration);

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	/** Adds a plant kick (PlantLean of the world move direction in the body frame) to the lean springs */
	void KickPlant(const FQuat& BodyQuat, const FVector& WorldMoveDirection, float KickDeg);

	/** On a wall mode: finds the wall's outward normal around the owner (presentation traces); zero when none */
	FVector FindWallNormal(const AActor& Owner, const FQuat& BodyQuat) const;

	/** The move override of this frame from the driver's active montage */
	void UpdateMoveOverride();

	/** Shape's override from MoveOverrides (the montage's own clip at the montage's time); false without an entry */
	bool ApplyMontageOverride(UAnimMontage& Montage, const FAnimMontageInstance& Instance, float Position);

	/** Shape's override from SequenceMoveOverrides (the clip of the sequence Segment plays, at its time in it); false without an entry */
	bool ApplySequenceOverride(const UAnimMontage& Montage, const FAnimMontageInstance& Instance, const FAnimSegment& Segment, float Position);


	/**
	 * The driver's pelvis height above the component (its feet) now, and in its reference pose, and the Z of its body-up axis now (the axis
	 * that is up in the reference pose, carried by the current rotation: 1 upright, 0 lying); false without a driver or a pelvis
	 */
	bool GetDriverPelvisHeight(float& OutHeight, float& OutReference, float& OutUpZ);

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
	/** Wall: the outward normal found this frame (zero off a wall), the judder's cycle fraction and this cycle's random size */
	FVector WallNormal = FVector::ZeroVector;
	float WallJudderCycle = 0.f;
	float WallJudderScale = 1.f;
	/** Run sway cycle phase (radians) and its smoothed amplitude */
	float SwayPhase = 0.f;
	float SwayAmplitude = 0.f;
	/** Seconds since the body last left rest on the ground; < 0 while at rest or in the air */
	float TimeSinceStart = -1.f;
	/** The current run stop already kicked (re-armed once still or speeding up) */
	bool bRunStopKicked = false;
	/** Skid: the run presentation (created at BeginPlay), the world yaw held during the skid, and the capsule's yaw offset from the actor */
	TWeakObjectPtr<UGSTN_RunPresentationComponent> RunPresentation;
	bool bWasSkidding = false;
	float SkidHeldYaw = 0.f;
	FGSTN_GreyboxSpring TurnSpring;
	/** Driver pelvis reference height, cached per driver mesh asset */
	TWeakObjectPtr<const USkeletalMesh> PelvisReferenceMesh;
	float PelvisReferenceHeight = 0.f;
	/** The body's up in the driver pelvis's own frame (reference pose), cached with the height */
	FVector PelvisLocalUp = FVector::UpVector;
	/** 0..1 how much the capsule lies with the driver's pelvis (eased) */
	float LyingAlpha = 0.f;
};
