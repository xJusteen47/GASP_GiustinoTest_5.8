// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GSTN_PoseDriverSource.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGSTN_PoseDriverSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * A component on the character whose gameplay clip owns the pose for a while the way a montage does, but through the main graph (a Blend
 * Stack state such as Run Stop or Skid), so UGSTN_CharacterAnimInstance cannot see it in MontageInstances. When the source stops driving,
 * the anim instance treats it like a montage blend-out: the blend-out foot lock holds the feet where the clip left them, waits for the hand-off
 * blend to the next pose (OutHandOffSeconds) and then steps each foot to where that pose wants it.
 *
 * Why (anim-setup-1, owner 2026-11-19 "no per-clip hacks"): every clip that ends at rest (run stop, skid release, landings) ends in a stance
 * that differs a few cm from the idle the chooser picks next; holding the feet through the hand-off fixes that for any clip and any anim set,
 * instead of re-posing each clip to the idle of the day. Called on the game thread only.
 */
class GSTNANIMATIONCORE_API IGSTN_PoseDriverSource
{
	GENERATED_BODY()

public:
	/** True while this source's clip owns the pose. OutHandOffSeconds: how long the main graph blends from that clip to the next pose */
	virtual bool IsDrivingPose(float& OutHandOffSeconds) const = 0;
};
