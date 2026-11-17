// A layer anim instance that decides which clip to show and at which time (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GSTN_ClipTimeSource.generated.h"

class UAnimSequenceBase;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGSTN_ClipTimeSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * Read by FGSTN_AnimNode_ClipAtTime on the game thread (its PreUpdate, after the instance's NativeUpdateAnimation): the clip and the time to
 * evaluate it at this frame. Lets a feature layer drive a clip from its own state (the run skid's phase and distance to the stop) without
 * Blueprint bindings: the layer graph is one node, the timing is C++ and data.
 */
class GSTNANIMATIONCORE_API IGSTN_ClipTimeSource
{
	GENERATED_BODY()

public:
	/** False = nothing to show (the node outputs the reference pose; the layer is not active then anyway) */
	virtual bool GetClipAtTime(UAnimSequenceBase*& OutClip, float& OutTime) const = 0;
};
