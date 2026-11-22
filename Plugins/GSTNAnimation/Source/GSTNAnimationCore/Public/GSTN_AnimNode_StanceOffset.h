// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "GSTN_AnimNode_StanceOffset.generated.h"

class UGSTN_StanceDefinition;

/**
 * Adds the chosen upper-body stance (UGSTN_StanceDefinition: StancePose minus ReferencePose, local space, per-bone weight and fade time) on
 * top of the game pose. It reads the stance and its timing from UGSTN_CharacterAnimInstance (ActiveStance, StanceReturnTime, StanceRestAlpha,
 * StanceMontageScale), so it needs no pin. With no stance, or while the stance is hidden (a montage, the capsule moving, the wait after a
 * move), the node only evaluates its input: the pose passes through untouched (GASP locomotion and foot placement unchanged by construction).
 * Legs, pelvis and root are never listed, so it does not fight the blend-out foot lock. Place it in the main AnimGraph right after the foot
 * lock node. Console: a.GSTN.StanceOffset 0 turns it off (pass through).
 *
 * The return fades are smoothstep curves (zero speed at both ends), and a stance switched while it shows blends from the node's last output,
 * so the node makes no pose jump of its own and needs no inertialization request (which would also catch GASP's legs).
 */
USTRUCT(BlueprintInternalUseOnly)
struct GSTNANIMATIONCORE_API FGSTN_AnimNode_StanceOffset : public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Links")
	FPoseLink Source;

	// FAnimNode_Base
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;

private:
	/** Offset and per-bone settings of Stance for the current required bones (recomputed when either changes) */
	bool RebuildCache(const FBoneContainer& Bones, const UGSTN_StanceDefinition& Stance);

	// Read on the update from the anim instance
	TWeakObjectPtr<const UGSTN_StanceDefinition> Stance;
	float ReturnTime = -1.f;
	float RestAlpha = 0.f;
	float MontageScale = 1.f;
	int32 Serial = 0;
	float DeltaTime = 0.f;

	// Cache, indexed by compact pose bone index
	TWeakObjectPtr<const UGSTN_StanceDefinition> CachedStance;
	uint16 CachedBonesSerial = 0;
	bool bCacheValid = false;
	TArray<FTransform> Offsets;
	TArray<float> BoneWeight;
	TArray<float> BoneFadeTime;
	TArray<FCompactPoseBoneIndex> WeightedBones;

	// Switch blend: the last output of the weighted bones, blended from when the stance asset changes while shown
	TArray<FTransform> LastOutput;           // indexed like WeightedBones
	TArray<FCompactPoseBoneIndex> LastBones;
	bool bHasLastOutput = false;
	TArray<FTransform> Snapshot;             // indexed like SnapshotBones
	TArray<FCompactPoseBoneIndex> SnapshotBones;
	int32 LastSerial = 0;
	float SwitchAlpha = 0.f;
	float SwitchBlendTime = 0.2f;
	float LastMaxAlpha = 0.f;
};
