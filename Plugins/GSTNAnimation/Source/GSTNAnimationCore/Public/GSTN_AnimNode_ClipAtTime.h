// Leaf pose node: a clip at the time its anim instance chooses (IGSTN_ClipTimeSource).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "GSTN_AnimNode_ClipAtTime.generated.h"

class UAnimSequenceBase;

/**
 * Evaluates the clip of the owning anim instance (an IGSTN_ClipTimeSource, e.g. the run skid layer) at the time it gives, copied on the game
 * thread in PreUpdate. No root motion is extracted (the simulation moves the character; the clip only poses it). An instance that is not a
 * source, or that answers false, gets the reference pose.
 */
USTRUCT(BlueprintInternalUseOnly)
struct GSTNANIMATIONCORE_API FGSTN_AnimNode_ClipAtTime : public FAnimNode_Base
{
	GENERATED_BODY()

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override {}
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

	/** Pure: Time clamped into the clip's length (0 for no clip or a negative time) */
	static float ClampTime(float Time, float Length);

private:
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> Clip;

	float Time = 0.f;
};
