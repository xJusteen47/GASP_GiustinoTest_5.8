#include "GSTN_AnimNode_GreyboxShape.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimNode_GreyboxShape)

FVector FGSTN_AnimNode_GreyboxShape::SquashToScale(float Squash)
{
	const float Height = FMath::Clamp(Squash, 0.2f, 3.f);
	const float Width = 1.f / FMath::Sqrt(Height);
	return FVector(Width, Width, Height);
}

void FGSTN_AnimNode_GreyboxShape::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);
}

void FGSTN_AnimNode_GreyboxShape::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Source.CacheBones(Context);
}

void FGSTN_AnimNode_GreyboxShape::PreUpdate(const UAnimInstance* InAnimInstance)
{
	const IGSTN_GreyboxShapeSource* ShapeSource = Cast<IGSTN_GreyboxShapeSource>(InAnimInstance);
	Shape = ShapeSource ? ShapeSource->GetGreyboxShape() : FGSTN_GreyboxShape();
}

void FGSTN_AnimNode_GreyboxShape::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	Source.Update(Context);
}

void FGSTN_AnimNode_GreyboxShape::Evaluate_AnyThread(FPoseContext& Output)
{
	Source.Evaluate(Output);
	const FCompactPoseBoneIndex Root(0);
	if (!Output.Pose.IsValidIndex(Root))
	{
		return;
	}
	if (Shape.OverrideClip && Shape.OverrideWeight > 0.f)
	{
		// The capsule version of the move at the montage's time; the root stays the copied one (Mover places the character)
		FPoseContext OverridePose(Output);
		FAnimationPoseData OverrideData(OverridePose);
		Shape.OverrideClip->GetAnimationPose(OverrideData, FAnimExtractContext(static_cast<double>(Shape.OverrideTime), false));
		OverridePose.Pose[Root] = Output.Pose[Root];
		if (Shape.OverrideWeight >= 1.f)
		{
			Output = OverridePose;
		}
		else
		{
			FPoseContext Blended(Output);
			FAnimationPoseData SourceData(Output);
			FAnimationPoseData BlendedData(Blended);
			FAnimationRuntime::BlendTwoPosesTogether(SourceData, OverrideData, 1.f - Shape.OverrideWeight, BlendedData);
			Output = Blended;
		}
	}
	// The root's parent is the component, so its local transform is in component space: rotate it around the ground point, scale it
	FTransform& RootTransform = Output.Pose[Root];
	RootTransform.SetRotation(FQuat(Shape.Lean) * RootTransform.GetRotation());
	RootTransform.SetScale3D(RootTransform.GetScale3D() * SquashToScale(Shape.Squash));
}

void FGSTN_AnimNode_GreyboxShape::GatherDebugData(FNodeDebugData& DebugData)
{
	DebugData.AddDebugItem(FString::Printf(TEXT("%s (squash %.2f, lean P %.1f R %.1f, move %s x%.2f)"), *DebugData.GetNodeName(this), Shape.Squash,
		Shape.Lean.Pitch, Shape.Lean.Roll, *GetNameSafe(Shape.OverrideClip), Shape.OverrideWeight));
	Source.GatherDebugData(DebugData);
}
