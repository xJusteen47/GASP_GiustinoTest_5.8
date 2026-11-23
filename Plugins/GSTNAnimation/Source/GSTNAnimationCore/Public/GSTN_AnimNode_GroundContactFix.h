// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "GSTN_GroundContactSettings.h"
#include "GSTN_GroundContactSolver.h"
#include "GSTN_AnimNode_GroundContactFix.generated.h"

/** Runtime of one contact bone (resolved from the settings) */
struct FGSTN_GroundContactRuntimeBone
{
	FCompactPoseBoneIndex Index = FCompactPoseBoneIndex(INDEX_NONE);
	FCompactPoseBoneIndex Mid = FCompactPoseBoneIndex(INDEX_NONE);   // limb end: parent (elbow / knee)
	FCompactPoseBoneIndex Root = FCompactPoseBoneIndex(INDEX_NONE);  // limb end: grandparent (shoulder / hip)
	EGSTN_GroundContactRole Role = EGSTN_GroundContactRole::LiftBody;
	double Radius = 0.0;
};

/**
 * Ground contact correction (owner 2026-11-17, ambito: reactions near the floor: knockdown, ground bounce, tech bounce, ground hit).
 * A post-process at the END of the main AnimGraph that only removes floor penetration from the pose the graph made. It never replaces the
 * authored animation: a pose that does not sink passes through unchanged, a pose that sinks is moved only by the depth it sinks, and the
 * whole correction is blended by a weight that only rises in the states listed in the settings (tags, montages, or an anim curve).
 *
 *   1. Body: the deepest LiftBody sphere (pelvis, spine, head, knees, elbows) sets one lift of LiftBone along the floor normal (instant up,
 *      eased down, capped at MaxLift).
 *   2. Limbs: each LimbEnd (hands, feet) that touched the floor before the lift is put back there by a two-bone IK (no floating hand), and
 *      one that still sinks after the lift is raised out of it.
 *
 * Floor: one line trace a frame on the game thread (PreUpdate), only while the correction is on; the plane under the trace start bone is
 * used for every bone (uneven floors under one body: later, per-bone traces). Presentation only: the simulation, timelines and hurtboxes
 * never read the mesh pose, nothing is replicated. Costs nothing outside the listed states (no trace, pose untouched).
 * Console: a.GSTN.GroundContactFix 0 = pass through; a.GSTN.GroundContactFix.Debug 1 = draw the floor, the spheres and the lift.
 */
USTRUCT(BlueprintInternalUseOnly)
struct GSTNANIMATIONCORE_API FGSTN_AnimNode_GroundContactFix : public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Links")
	FPoseLink Source;

	/** Tuning and the states where it is on. Null = the node passes the pose through */
	UPROPERTY(EditAnywhere, Category = "Settings")
	TObjectPtr<UGSTN_GroundContactSettings> Settings;

	/** Mesh LODs above this skip the correction (perf on low-end); -1 = every LOD */
	UPROPERTY(EditAnywhere, Category = "Settings")
	int32 LODThreshold = 1;

	// FAnimNode_Base
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
	virtual int32 GetLODThreshold() const override { return LODThreshold; }

private:
	float TargetWeightFromRules(const UAnimInstance* AnimInstance, float& OutBlendIn, float& OutBlendOut);

	TArray<FGSTN_GroundContactRuntimeBone> RuntimeBones;
	FCompactPoseBoneIndex LiftIndex = FCompactPoseBoneIndex(INDEX_NONE);

	// Game thread (PreUpdate) -> worker
	float TargetWeight = 0.f;
	float BlendIn = 0.05f;
	float BlendOut = 0.15f;
	bool bHasFloor = false;
	FGSTN_GroundPlane FloorWorld;

	// Worker state
	float Weight = 0.f;
	float DeltaTime = 0.f;
	double Lift = 0.0;
	double LastRequiredLift = 0.0;
	int32 LastLimbFixes = 0;

	// Owner tag sources, cached (the ability system component carries the projected combat states)
	TWeakObjectPtr<const AActor> CachedOwner;
	TArray<TWeakObjectPtr<const UObject>> CachedTagSources;
	int32 CachedComponentCount = 0;
};
