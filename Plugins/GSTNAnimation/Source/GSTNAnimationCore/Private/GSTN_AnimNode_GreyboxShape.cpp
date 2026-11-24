#include "GSTN_AnimNode_GreyboxShape.h"

#include "Animation/AnimInstance.h"

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
	// The root's parent is the component, so its local transform is in component space: rotate it around the ground point, scale it
	FTransform& RootTransform = Output.Pose[Root];
	RootTransform.SetRotation(FQuat(Shape.Lean) * RootTransform.GetRotation());
	RootTransform.SetScale3D(RootTransform.GetScale3D() * SquashToScale(Shape.Squash));
}

void FGSTN_AnimNode_GreyboxShape::GatherDebugData(FNodeDebugData& DebugData)
{
	DebugData.AddDebugItem(FString::Printf(TEXT("%s (squash %.2f, lean P %.1f R %.1f)"), *DebugData.GetNodeName(this), Shape.Squash,
		Shape.Lean.Pitch, Shape.Lean.Roll));
	Source.GatherDebugData(DebugData);
}
