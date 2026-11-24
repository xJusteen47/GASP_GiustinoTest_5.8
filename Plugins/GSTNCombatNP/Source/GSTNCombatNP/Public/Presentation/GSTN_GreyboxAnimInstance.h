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

	/** Hand and foot markers (owner 2026-11-24: hidden in locomotion and most of the time): shown while the driver plays a montage (a
	 *  move, a hit reaction, a roll) and/or in locomotion */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Limbs")
	bool bShowLimbsInMoves = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Limbs")
	bool bShowLimbsInLocomotion = false;

	/** How fast the markers grow / shrink, 1/s */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Limbs", meta = (ClampMin = "1"))
	float LimbBlendSpeed = 15.f;
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

	/** Pure: the limb marker scale target for a montage weight on the driver (0 = locomotion only) */
	static float LimbScaleTarget(const FGSTN_GreyboxShapeSettings& Settings, float MontageWeight);

	/** Pure: the lean target (pitch forward, roll right, degrees) for a horizontal acceleration in the actor's local frame */
	static FRotator LeanTarget(const FGSTN_GreyboxShapeSettings& Settings, const FVector& LocalAcceleration);

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	/** The move override of this frame from the driver's active montage */
	void UpdateMoveOverride();

	/** Weight of the driver's active montage this frame (UpdateMoveOverride writes it) */
	float DriverMontageWeight = 0.f;

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
	/** Driver pelvis reference height, cached per driver mesh asset */
	TWeakObjectPtr<const USkeletalMesh> PelvisReferenceMesh;
	float PelvisReferenceHeight = 0.f;
};
