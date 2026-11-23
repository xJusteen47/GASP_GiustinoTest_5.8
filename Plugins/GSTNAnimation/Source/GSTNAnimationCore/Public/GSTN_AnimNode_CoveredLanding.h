// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "GSTN_AnimNode_CoveredLanding.generated.h"

/**
 * Covered landing (FGSTN_CoveredLandingLogic): after a touchdown made under a montage, holds the montage's last full pose over the base
 * layer's own landing reaction, then blends back to the live pose. It reads UGSTN_CharacterAnimInstance (CoveredLandingCapture /
 * CoveredLandingAlpha), so it needs no pin; at weight 0 the pose passes through untouched. Place it in the main AnimGraph after the montage
 * slot (Tools/GAS/setup_covered_landing_node.py puts it right after the blend-out foot lock). Console: a.GSTN.CoveredLanding 0 turns it off
 */
USTRUCT(BlueprintInternalUseOnly)
struct GSTNANIMATIONCORE_API FGSTN_AnimNode_CoveredLanding : public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Links")
	FPoseLink Source;

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

private:
	bool bCapture = false;
	float HoldAlpha = 0.f;
	/** The held pose, local space, for the bones of the LOD it was taken at (dropped when they change) */
	TArray<FTransform> Held;
};
