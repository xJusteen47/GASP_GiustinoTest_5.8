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

int32 FGSTN_AnimNode_GreyboxShape::UpAxisOf(const FQuat& ComponentRotation)
{
	int32 UpAxis = 0;
	float Best = -1.f;
	const FVector Axes[3] = {ComponentRotation.GetAxisX(), ComponentRotation.GetAxisY(), ComponentRotation.GetAxisZ()};
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const float Dot = FMath::Abs(static_cast<float>(Axes[Axis].Z));
		if (Dot > Best)
		{
			Best = Dot;
			UpAxis = Axis;
		}
	}
	return UpAxis;
}

float FGSTN_AnimNode_GreyboxShape::PelvisHeightAfterSquash(float Height, float Squash)
{
	return Height * FMath::Clamp(Squash, 0.2f, 3.f);
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
	// Rigid body in locomotion: blend toward the reference pose, the root stays the copied one (Mover's placement and facing)
	const float PoseWeight = FMath::Clamp(Shape.PoseWeight, 0.f, 1.f);
	if (PoseWeight < 0.999f)
	{
		const FTransform CopiedRoot = Output.Pose[Root];
		FPoseContext Reference(Output);
		Reference.ResetToRefPose();
		if (PoseWeight <= 0.001f)
		{
			Output.Pose.CopyBonesFrom(Reference.Pose);
		}
		else
		{
			FPoseContext Blended(Output);
			FAnimationPoseData SourceData(Output);
			FAnimationPoseData ReferenceData(Reference);
			FAnimationPoseData BlendedData(Blended);
			FAnimationRuntime::BlendTwoPosesTogether(SourceData, ReferenceData, PoseWeight, BlendedData);
			Output = Blended;
		}
		Output.Pose[Root] = CopiedRoot;
	}
	// Squash of the whole body around the ground point (run 3). Never a non-uniform scale on the root: a bone's scale composes per local
	// axis (FTransform keeps no shear), and the mannequin's pelvis and spine bones have their local X as the body's up, so a root scale
	// (wide, wide, short) became (tall, wide, short) on them: the crouch pushed the lower hemisphere through the floor and an air stretch
	// made the bent spine lumpy. Instead the pelvis bone takes the squash along its own up axis (its spine children share that axis),
	// and its height above the root is scaled by the same amount, which is the same as squashing about the ground: the bottom of a
	// capsule hanging from the pelvis ends at Squash times its height and never goes under the feet.
	// A rigid body cannot lower its pelvis: its share of the ground squash goes to the height as well (PoseWeight 0); a posed body (an
	// attack) already lowers it, so only the bone squashes for it.
	const float HeightSquash = Shape.Squash * FMath::Lerp(Shape.PelvisSquash, 1.f, PoseWeight);
	const float BodySquash = Shape.Squash * Shape.PelvisSquash;
	const FCompactPoseBoneIndex Pelvis = PelvisBone.GetCompactPoseIndex(Output.Pose.GetBoneContainer());
	if (Output.Pose.IsValidIndex(Pelvis))
	{
		if (!FMath::IsNearlyEqual(BodySquash, 1.f, 1.e-3f))
		{
			// The local axis that is the body's up in the reference pose (measured in component space)
			const FQuat RefPelvis = Output.Pose.GetRefPose(Root).GetRotation() * Output.Pose.GetRefPose(Pelvis).GetRotation();
			const int32 UpAxis = UpAxisOf(RefPelvis);
			const FQuat Current = Output.Pose[Root].GetRotation() * Output.Pose[Pelvis].GetRotation();
			const FVector CurrentUp = UpAxis == 0 ? Current.GetAxisX() : (UpAxis == 1 ? Current.GetAxisY() : Current.GetAxisZ());
			// Lying flat fades the squash out (a body lying down must not be squashed through its thickness); the rigid capsule is always
			// upright enough, so it keeps all of it
			const float Verticality = FMath::Lerp(1.f, FMath::Square(FMath::Abs(static_cast<float>(CurrentUp.Z))), PoseWeight);
			Output.Pose[Pelvis].SetScale3D(Output.Pose[Pelvis].GetScale3D() * AxisSquashScale(UpAxis, BodySquash, Verticality));
		}
		if (!FMath::IsNearlyEqual(HeightSquash, 1.f, 1.e-3f))
		{
			// The pelvis is a child of the root: its local Z is its height above the feet along the (leaned) up axis
			FVector Location = Output.Pose[Pelvis].GetTranslation();
			Location.Z = PelvisHeightAfterSquash(static_cast<float>(Location.Z), HeightSquash);
			Output.Pose[Pelvis].SetTranslation(Location);
		}
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
	// The root's parent is the component, so its local transform is in component space: rotate it around the ground point
	FTransform& RootTransform = Output.Pose[Root];
	RootTransform.SetRotation(FQuat(Shape.Lean) * RootTransform.GetRotation());
}

void FGSTN_AnimNode_GreyboxShape::GatherDebugData(FNodeDebugData& DebugData)
{
	DebugData.AddDebugItem(FString::Printf(TEXT("%s (squash %.2f, lean P %.1f R %.1f, move %s x%.2f)"), *DebugData.GetNodeName(this), Shape.Squash,
		Shape.Lean.Pitch, Shape.Lean.Roll, *GetNameSafe(Shape.OverrideClip), Shape.OverrideWeight));
	Source.GatherDebugData(DebugData);
}
