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

FGSTN_AnimNode_GreyboxShape::FGSTN_AnimNode_GreyboxShape()
{
	PelvisBone.BoneName = TEXT("pelvis");
	for (const TCHAR* Name : {TEXT("hand_l"), TEXT("hand_r"), TEXT("foot_l"), TEXT("foot_r")})
	{
		LimbBones.Add(FBoneReference(FName(Name)));
	}
}

FVector FGSTN_AnimNode_GreyboxShape::AxisSquashScale(int32 UpAxis, float Squash, float Verticality)
{
	const float Effective = FMath::Lerp(1.f, FMath::Clamp(Squash, 0.2f, 3.f), FMath::Clamp(Verticality, 0.f, 1.f));
	const float Width = 1.f / FMath::Sqrt(Effective);
	FVector Scale(Width, Width, Width);
	Scale[FMath::Clamp(UpAxis, 0, 2)] = Effective;
	return Scale;
}

void FGSTN_AnimNode_GreyboxShape::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);
}

void FGSTN_AnimNode_GreyboxShape::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Source.CacheBones(Context);
	PelvisBone.Initialize(Context.AnimInstanceProxy->GetRequiredBones());
	for (FBoneReference& Limb : LimbBones)
	{
		Limb.Initialize(Context.AnimInstanceProxy->GetRequiredBones());
	}
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
	// Ground squash on the pelvis, along the local axis that is the body's up in the reference pose (measured in component space)
	const FCompactPoseBoneIndex Pelvis = PelvisBone.GetCompactPoseIndex(Output.Pose.GetBoneContainer());
	if (Output.Pose.IsValidIndex(Pelvis) && Shape.PelvisSquash < 0.999f)
	{
		const FQuat RefPelvis = Output.Pose.GetRefPose(Root).GetRotation() * Output.Pose.GetRefPose(Pelvis).GetRotation();
		int32 UpAxis = 0;
		float Best = -1.f;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const float Dot = FMath::Abs(RefPelvis.GetAxisX().Z * (Axis == 0) + RefPelvis.GetAxisY().Z * (Axis == 1) + RefPelvis.GetAxisZ().Z * (Axis == 2));
			if (Dot > Best)
			{
				Best = Dot;
				UpAxis = Axis;
			}
		}
		const FQuat Current = Output.Pose[Root].GetRotation() * Output.Pose[Pelvis].GetRotation();
		const FVector CurrentUp = UpAxis == 0 ? Current.GetAxisX() : (UpAxis == 1 ? Current.GetAxisY() : Current.GetAxisZ());
		const float Verticality = FMath::Square(FMath::Abs(static_cast<float>(CurrentUp.Z)));
		Output.Pose[Pelvis].SetScale3D(Output.Pose[Pelvis].GetScale3D() * AxisSquashScale(UpAxis, Shape.PelvisSquash, Verticality));
	}
	// Limb markers: shrunk into the body when hidden (never exactly 0, a zero scale breaks the bone matrices' inverse)
	if (Shape.LimbScale < 0.999f)
	{
		const float LimbScale = FMath::Clamp(Shape.LimbScale, 0.01f, 1.f);
		for (const FBoneReference& Limb : LimbBones)
		{
			const FCompactPoseBoneIndex Index = Limb.GetCompactPoseIndex(Output.Pose.GetBoneContainer());
			if (Output.Pose.IsValidIndex(Index))
			{
				Output.Pose[Index].SetScale3D(Output.Pose[Index].GetScale3D() * LimbScale);
			}
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
