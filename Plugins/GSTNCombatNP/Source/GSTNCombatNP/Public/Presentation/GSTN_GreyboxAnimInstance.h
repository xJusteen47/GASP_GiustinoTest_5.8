// Combat presentation (GAS-NPP fork): the greybox capsule's anim instance (greybox-capsula, owner 2026-11-24 2A). Never read by the simulation.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GSTN_GreyboxShapeSource.h"
#include "GSTN_GreyboxAnimInstance.generated.h"

class UMoverComponent;

/** How the capsule squashes, stretches and leans (class defaults of ABP_GSTN_GreyboxCapsule; the owner tunes them there) */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_GreyboxShapeSettings
{
	GENERATED_BODY()

	/** Stretch in the air per 1000 cm/s of vertical speed (up or down) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Squash", meta = (ClampMin = "0", ClampMax = "1"))
	float AirStretchPer1000 = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Squash", meta = (ClampMin = "1", ClampMax = "2"))
	float MaxStretch = 1.2f;

	/** Landing squash per 1000 cm/s of fall speed at touchdown (a kick into the spring, it bounces back) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Squash", meta = (ClampMin = "0", ClampMax = "1"))
	float LandSquashPer1000 = 0.25f;

	/** Squash when the ground speed jumps (a run start, a burst): per 1000 cm/s² of forward acceleration */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Squash", meta = (ClampMin = "0", ClampMax = "1"))
	float AccelSquashPer1000 = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Squash", meta = (ClampMin = "0.3", ClampMax = "1"))
	float MinSquash = 0.7f;

	/** Lean into the horizontal acceleration, degrees per 1000 cm/s² (forward on a start, back on a brake, sideways in a turn) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Lean", meta = (ClampMin = "0", ClampMax = "20"))
	float LeanDegPer1000 = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Lean", meta = (ClampMin = "0", ClampMax = "45", ForceUnits = "deg"))
	float MaxLeanDeg = 12.f;

	/** Spring of squash and lean: frequency (Hz) and damping ratio (< 1 bounces, 1 settles without overshoot) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Spring", meta = (ClampMin = "0.5", ClampMax = "20", ForceUnits = "Hz"))
	float SpringFrequency = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Greybox|Spring", meta = (ClampMin = "0.05", ClampMax = "2"))
	float SpringDamping = 0.45f;
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
 */
UCLASS()
class GSTNCOMBATNP_API UGSTN_GreyboxAnimInstance : public UAnimInstance, public IGSTN_GreyboxShapeSource
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Greybox")
	FGSTN_GreyboxShapeSettings ShapeSettings;

	virtual FGSTN_GreyboxShape GetGreyboxShape() const override { return Shape; }

	/** Pure: the squash target (1 = rest) for a vertical speed in the air and a forward acceleration on the ground */
	static float SquashTarget(const FGSTN_GreyboxShapeSettings& Settings, bool bOnGround, float VelocityZ, float ForwardAcceleration);

	/** Pure: the lean target (pitch forward, roll right, degrees) for a horizontal acceleration in the actor's local frame */
	static FRotator LeanTarget(const FGSTN_GreyboxShapeSettings& Settings, const FVector& LocalAcceleration);

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	TWeakObjectPtr<UMoverComponent> Mover;
	FGSTN_GreyboxShape Shape;
	FGSTN_GreyboxSpring SquashSpring;
	FGSTN_GreyboxSpring PitchSpring;
	FGSTN_GreyboxSpring RollSpring;
	FVector PreviousVelocity = FVector::ZeroVector;
	bool bHasPrevious = false;
	bool bWasOnGround = true;
	float LastAirVelocityZ = 0.f;
};
