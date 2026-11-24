// Greybox capsule shape: the squash/stretch and lean an anim instance wants on its mesh root (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GSTN_GreyboxShapeSource.generated.h"

/** The procedural shape of the greybox capsule this frame (greybox-capsula, owner 2026-11-24 2A) */
struct GSTNANIMATIONCORE_API FGSTN_GreyboxShape
{
	/** Height scale of the whole body around the ground (< 1 squash, > 1 stretch); width gets 1/sqrt so the volume stays */
	float Squash = 1.f;

	/** Tilt of the whole body around the ground point, component space, degrees (pitch forward, roll sideways) */
	FRotator Lean = FRotator::ZeroRotator;
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
