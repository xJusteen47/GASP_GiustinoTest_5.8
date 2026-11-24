// Greybox capsule shape: the squash/stretch and lean an anim instance wants on its mesh root (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GSTN_GreyboxShapeSource.generated.h"

class UAnimSequenceBase;

/** The procedural shape of the greybox capsule this frame (greybox-capsula, owner 2026-11-24 2A) */
struct GSTNANIMATIONCORE_API FGSTN_GreyboxShape
{
	/** Height scale of the whole body around the ground (< 1 squash, > 1 stretch); width gets 1/sqrt so the volume stays */
	float Squash = 1.f;

	/**
	 * Ground squash from the pose (run 2): the copied pelvis height over its reference height while on the ground (1 = standing). The node
	 * scales the pelvis along the body's up axis by it, so a crouch or a landing that lowers the pelvis makes a shorter capsule that still
	 * stands on the ground, instead of sinking into it (the capsule hangs from the pelvis since run 2).
	 */
	float PelvisSquash = 1.f;

	/** Scale of the hand and foot markers, 0..1 (owner 2026-11-24: limbs hidden in locomotion and most of the time, shown in moves) */
	float LimbScale = 1.f;

	/**
	 * How much the capsule body follows the copied pose, 0..1 (owner 2026-11-24: in locomotion the capsule does not use the bones, it only
	 * sways). 0 = body in the reference pose, the whole read comes from Squash and Lean; 1 = it bends with the driver (moves).
	 */
	float PoseWeight = 1.f;

	/**
	 * Lying (owner 2026-11-24, "the capsule should turn when it is on the ground, it squashes strangely now"): 0..1 how much the rigid capsule
	 * takes the copied pelvis (rotation and height) instead of the reference one, so a body that lies down (knockdown, get-up, roll, tumble)
	 * lies down as a whole capsule instead of standing squashed to its pelvis height. The rest of the body stays rigid on the pelvis. The squash
	 * fades with it, like for a posed body.
	 */
	float LyingWeight = 0.f;

	/**
	 * Keep the capsule out of the floor (owner 2026-11-24, same note): the capsule's reference geometry in component space, cm above the feet
	 * (bottom and top of the body, radius). When FloorRadius > 0 the node lifts the pelvis so the lowest point of the posed capsule stays at or
	 * above the feet's height (a lying capsule is as thick as its radius, the copied pelvis of a lying body is lower than that). 0 = off (the
	 * instance turns it off in the air, where the feet are not the floor).
	 */
	float FloorBottom = 0.f;
	float FloorTop = 0.f;
	float FloorRadius = 0.f;

	/** Tilt of the whole body around the ground point, component space, degrees (pitch forward, roll sideways) */
	FRotator Lean = FRotator::ZeroRotator;

	/**
	 * Move override (3A, owner 2026-11-24): the capsule version of the montage the driver plays (an attack, a hit, a roll), shown at the
	 * montage's time instead of the copied pose, blended by the montage's weight. The driver keeps playing the reference montage, so
	 * hitboxes and hurtboxes do not change. Null = no override. The clip must stay referenced by the source (it is not a UPROPERTY here).
	 */
	const UAnimSequenceBase* OverrideClip = nullptr;
	float OverrideTime = 0.f;
	float OverrideWeight = 0.f;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGSTN_GreyboxShapeSource : public UInterface
{
	GENERATED_BODY()
};

/** Read by FGSTN_AnimNode_GreyboxShape on the game thread (its PreUpdate, after the instance's NativeUpdateAnimation) */
class GSTNANIMATIONCORE_API IGSTN_GreyboxShapeSource
{
	GENERATED_BODY()

public:
	virtual FGSTN_GreyboxShape GetGreyboxShape() const = 0;
};
