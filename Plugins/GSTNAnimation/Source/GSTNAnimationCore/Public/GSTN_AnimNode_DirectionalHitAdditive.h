// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "GSTN_AdditiveReactionSource.h"
#include "GSTN_AnimNode_DirectionalHitAdditive.generated.h"

/** One bone of the placeholder bend and its share of the bend angle */
USTRUCT()
struct GSTNANIMATIONCORE_API FGSTN_HitBendBone
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference Bone;

	/** Share of the layer's PlaceholderAngleDeg this bone turns by (the shares of a chain should sum to about 1) */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (ClampMin = "0.0"))
	float Share = 0.2f;
};

/**
 * Directional hit additives on the torso (hit-react-directional, owner option B 2026-11-16): on top of whatever the graph shows (the ground
 * or air base reaction montage, or an attack), adds the additive clip of the side the hit came from with the hit's intensity as weight.
 * The decisions (side, intensity, timing) come from an IGSTN_AdditiveReactionSource component on the owning actor, read once a frame on the
 * game thread (PreUpdate), so this node needs no pin and the graph never names the combat code.
 *
 * Clips are masked to BranchRoot and its children (the legs keep the base pose and the foot lock). A layer with no clip draws a PLACEHOLDER
 * procedural bend of the spine chain away from the hit: it is there so the system can be tested end to end, it is not a pose.
 *
 * With no source or no active layer the node only evaluates its input: GASP's pose passes through untouched by construction.
 * Place it in the main AnimGraph after the montage slot. Console: a.GSTN.HitAdditive 0 = pass through.
 */
USTRUCT(BlueprintInternalUseOnly)
struct GSTNANIMATIONCORE_API FGSTN_AnimNode_DirectionalHitAdditive : public FAnimNode_Base
{
	GENERATED_BODY()

	FGSTN_AnimNode_DirectionalHitAdditive();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Links")
	FPoseLink Source;

	/** Clips only change this bone and its children */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference BranchRoot;

	/** The placeholder bend chain, parents first */
	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FGSTN_HitBendBone> BendChain;

	// FAnimNode_Base
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

	/** The placeholder curve: 0 -> 1 over Peak (ease out), back to 0 at Length (ease in-out). Pure, for the tests */
	static float PlaceholderCurve(float Time, float PeakSeconds, float LengthSeconds);

private:
	void ApplyClip(FPoseContext& Output, const FGSTN_AdditiveReactionLayer& Layer) const;
	void ApplyPlaceholder(FPoseContext& Output, const FGSTN_AdditiveReactionLayer& Layer) const;

	/** This frame's request, copied on the game thread */
	FGSTN_AdditiveReactionRequest Request;
	bool bHasRequest = false;

	TWeakObjectPtr<UObject> CachedSource;
	TWeakObjectPtr<const AActor> CachedOwner;

	/** Per compact bone: inside BranchRoot's branch */
	TArray<bool> InBranch;
};
