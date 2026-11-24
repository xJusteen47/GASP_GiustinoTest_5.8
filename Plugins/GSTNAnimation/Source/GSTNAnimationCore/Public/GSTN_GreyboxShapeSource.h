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
