// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GSTN_AdditiveReactionSource.generated.h"

class UAnimSequenceBase;

/**
 * One directional hit additive the anim node draws this frame (hit-react-directional, owner option B 2026-11-16): an additive clip on the
 * torso, or, while no clip exists for that direction, a PLACEHOLDER procedural bend of the spine away from the hit. Presentation only.
 */
struct GSTNANIMATIONCORE_API FGSTN_AdditiveReactionLayer
{
	/** Additive clip (local space or mesh-space rotation offset); null = the placeholder bend */
	const UAnimSequenceBase* Additive = nullptr;

	/** Seconds into the clip / into the placeholder curve */
	float Time = 0.f;

	/** 0..1: the hit intensity times the fade */
	float Weight = 0.f;

	/** The way the body is pushed, world space, horizontal, unit (the cardinal direction the side was selected as) */
	FVector PushDirection = FVector::ZeroVector;

	/** Placeholder only: peak bend (degrees, summed over the spine chain at weight 1), rise time and total length (s) */
	float PlaceholderAngleDeg = 0.f;
	float PlaceholderPeakSeconds = 0.07f;
	float PlaceholderLengthSeconds = 0.4f;

	bool IsActive() const { return Weight > KINDA_SMALL_NUMBER && (Additive != nullptr || PlaceholderAngleDeg > 0.f); }
};

/** What a source wants drawn now: the newest hit, and the one it replaces while that fades out */
struct GSTNANIMATIONCORE_API FGSTN_AdditiveReactionRequest
{
	static constexpr int32 MaxLayers = 2;
	FGSTN_AdditiveReactionLayer Layers[MaxLayers];
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UGSTN_AdditiveReactionSource : public UInterface
{
	GENERATED_BODY()
};

/**
 * A component on the character that decides the directional hit additives (UGSTN_ReactionAnimComponent in GSTNCombatGAS). The anim node
 * FGSTN_AnimNode_DirectionalHitAdditive finds it on the owning actor and asks it once a frame on the game thread, so the anim graph never
 * names the combat code. Called on the game thread only.
 */
class GSTNANIMATIONCORE_API IGSTN_AdditiveReactionSource
{
	GENERATED_BODY()

public:
	virtual void GetAdditiveReaction(FGSTN_AdditiveReactionRequest& Out) const = 0;
};
