// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GSTN_OverrideLayerSource.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGSTN_OverrideLayerSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * A linked anim layer that can take over the character's pose while its own feature runs (a wall mode, later a spin or a drill), see
 * the plugin README and docs/architecture/anim-layers.md. The layer decides when from its own data; the main graph only asks (UGSTN_CharacterAnimInstance),
 * so features bring their animation as a layer class chosen by data and the main graph never names them.
 *
 * Called on the game thread after the linked layers have updated, so the answer is this frame's.
 */
class GSTNANIMATIONCORE_API IGSTN_OverrideLayerSource
{
	GENERATED_BODY()

public:
	/** True while this layer wants to replace the main pose */
	virtual bool IsOverrideLayerActive() const = 0;

	/** Blend time into and out of this layer (s) */
	virtual float GetOverrideBlendTime() const { return 0.15f; }

	/** When more than one layer is active, the highest priority wins */
	virtual int32 GetOverridePriority() const { return 0; }
};
