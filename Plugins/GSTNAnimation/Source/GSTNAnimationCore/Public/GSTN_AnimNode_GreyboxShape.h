// Pose node: squash/stretch and lean of the greybox capsule, applied to the mesh root so the body deforms around the ground point.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneContainer.h"
#include "GSTN_GreyboxShapeSource.h"
#include "GSTN_AnimNode_GreyboxShape.generated.h"

/**
 * Tilts the root bone of the incoming pose and squashes the body by the shape its anim instance gives (IGSTN_GreyboxShapeSource). The root
 * sits at the feet, so a lean pivots on it, like a bean character. The squash goes on the pelvis (along its own up axis, with its height
 * above the feet scaled the same way), never as a non-uniform scale on the root: see Evaluate_AnyThread (run 3, the crouch went through the
 * floor). Presentation only: hitboxes read the driver, never this mesh.
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

	/** Bones that carry the limb markers (scaled by the shape's LimbScale; children follow) */
	UPROPERTY(EditAnywhere, Category = "Settings")
	TArray<FBoneReference> LimbBones;

	FGSTN_AnimNode_GreyboxShape();

	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

	/** Pure: the volume-keeping scale for a squash value (height Squash, width 1/sqrt(Squash)); Squash is clamped to 0.2..3 */
	static FVector SquashToScale(float Squash);

	/** Pure: which local axis (0 X, 1 Y, 2 Z) of a bone with this component-space rotation points most up */
	static int32 UpAxisOf(const FQuat& ComponentRotation);

	/** Pure: the pelvis height above the feet after a squash about the ground (Squash clamped like SquashToScale) */
	static float PelvisHeightAfterSquash(float Height, float Squash);

	/** Pure: the local scale of a bone whose local axis UpAxis (0 X, 1 Y, 2 Z) is the body's up, squashed by Squash; Verticality 0..1 fades it
	 *  out when that axis lies down (a body lying flat must not be squashed through its thickness) */
	static FVector AxisSquashScale(int32 UpAxis, float Squash, float Verticality);

	/**
	 * Pure: the lowest height of a capsule hanging from a pelvis at PelvisLocation (component space), its axis along Up (unit), the centres of
	 * its two hemispheres BelowPelvis and AbovePelvis cm from the pelvis along the axis, Radius thick. The floor lift keeps it >= 0
	 */
	static float CapsuleLowestZ(const FVector& PelvisLocation, const FVector& Up, float BelowPelvis, float AbovePelvis, float Radius);

private:
	FGSTN_GreyboxShape Shape;
};
