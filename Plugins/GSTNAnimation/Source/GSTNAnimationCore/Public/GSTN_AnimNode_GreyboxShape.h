// Pose node: squash/stretch and lean of the greybox capsule, applied to the mesh root so the body deforms around the ground point.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "GSTN_GreyboxShapeSource.h"
#include "GSTN_AnimNode_GreyboxShape.generated.h"

/**
 * Scales and tilts the root bone of the incoming pose by the shape its anim instance gives (IGSTN_GreyboxShapeSource). The root sits at the
 * feet, so a squash keeps the capsule on the ground and a lean pivots on it, like a bean character. Everything above the root (the pose
 * copied from the invisible driver) follows. Presentation only: hitboxes read the driver, never this mesh.
 */
USTRUCT(BlueprintInternalUseOnly)
struct GSTNANIMATIONCORE_API FGSTN_AnimNode_GreyboxShape : public FAnimNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Links")
	FPoseLink Source;

	/** The bone the capsule hangs from (scaled by the shape's PelvisSquash) */
	UPROPERTY(EditAnywhere, Category = "Settings")
	FBoneReference PelvisBone;

	FGSTN_AnimNode_GreyboxShape();

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

	/** Pure: the root scale for a squash value (height Squash, width 1/sqrt(Squash)); Squash is clamped to 0.2..3 */
	static FVector SquashToScale(float Squash);

	/** Pure: the local scale of a bone whose local axis UpAxis (0 X, 1 Y, 2 Z) is the body's up, squashed by Squash; Verticality 0..1 fades it
	 *  out when that axis lies down (a body lying flat must not be squashed through its thickness) */
	static FVector AxisSquashScale(int32 UpAxis, float Squash, float Verticality);

private:
	FGSTN_GreyboxShape Shape;
};
