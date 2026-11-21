// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_AnimNode_StanceWidth.h"

#include "Animation/AnimInstanceProxy.h"
#include "HAL/IConsoleManager.h"
#include "TwoBoneIK.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimNode_StanceWidth)

namespace GSTNStanceWidth
{
	static TAutoConsoleVariable<float> CVarWidth(TEXT("gstn.Loco.StanceWidth"), 5.f,
		TEXT("Stance width layer: cm each foot moves outward where a clip opts in (curves GSTN_StanceWidth_L/_R, e.g. run start variant 22). 0 = off."));
	static TAutoConsoleVariable<float> CVarMinHalf(TEXT("gstn.Loco.StanceMinHalf"), 0.f,
		TEXT("Stance width layer: minimum distance (cm) of each foot from the midline where a clip opts in; a closer or crossing foot is pushed out to it. 0 = off."));
	static TAutoConsoleVariable<float> CVarForce(TEXT("gstn.Loco.StanceWidth.Force"), 0.f,
		TEXT("Stance width layer: weight (0..1) applied to every clip, also without the curves (try the layer on idle, loop, turns). 0 = only clips with the curves."));

	/** Pose index of a bone name, or none (missing on this skeleton or stripped at this LOD) */
	FCompactPoseBoneIndex Resolve(const FBoneContainer& Bones, FName Name)
	{
		if (Name.IsNone())
		{
			return FCompactPoseBoneIndex(INDEX_NONE);
		}
		FBoneReference Ref(Name);
		return Ref.Initialize(Bones) && Ref.IsValidToEvaluate(Bones) ? Ref.GetCompactPoseIndex(Bones) : FCompactPoseBoneIndex(INDEX_NONE);
	}

	/** Reference pose of a bone in component space (walks the parents) */
	FTransform RefComponentSpace(const FBoneContainer& Bones, FCompactPoseBoneIndex Bone)
	{
		FTransform Result = FTransform::Identity;
		for (FCompactPoseBoneIndex Index = Bone; Index != INDEX_NONE; Index = Bones.GetParentBoneIndex(Index))
		{
			Result = Result * Bones.GetRefPoseTransform(Index);
		}
		return Result;
	}
}

double FGSTN_AnimNode_StanceWidth::OutwardMove(double Lateral, double InOffset, double InMinHalfWidth, double Softness)
{
	double Target = Lateral + InOffset;
	if (InMinHalfWidth > 0.0)
	{
		// Smooth max(Target, MinHalfWidth): equal to Target well outside the floor, to the floor well inside it, no kink in between
		const double S = FMath::Max(Softness, 0.0);
		Target = 0.5 * (Target + InMinHalfWidth + FMath::Sqrt(FMath::Square(Target - InMinHalfWidth) + S * S));
	}
	return Target - Lateral;
}

void FGSTN_AnimNode_StanceWidth::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);
}

void FGSTN_AnimNode_StanceWidth::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Source.CacheBones(Context);
	bValid = false;
	const FBoneContainer& Bones = Context.AnimInstanceProxy->GetRequiredBones();
	Axis = GSTNStanceWidth::Resolve(Bones, AxisBone);
	const FName FootNames[2] = {LeftFoot, RightFoot};
	const FName IKNames[2] = {LeftIKFoot, RightIKFoot};
	for (int32 i = 0; i < 2; ++i)
	{
		FLeg& Leg = Legs[i];
		Leg = FLeg();
		Leg.Foot = GSTNStanceWidth::Resolve(Bones, FootNames[i]);
		Leg.Knee = Leg.Foot != INDEX_NONE ? Bones.GetParentBoneIndex(Leg.Foot) : FCompactPoseBoneIndex(INDEX_NONE);
		Leg.Hip = Leg.Knee != INDEX_NONE ? Bones.GetParentBoneIndex(Leg.Knee) : FCompactPoseBoneIndex(INDEX_NONE);
		Leg.IKFoot = GSTNStanceWidth::Resolve(Bones, IKNames[i]);
		if (Leg.Hip == INDEX_NONE)
		{
			return;
		}
	}

	// Lateral axis from the reference pose, in the axis bone's frame: from the right foot to the left foot, flattened
	const FTransform AxisRef = Axis != INDEX_NONE ? GSTNStanceWidth::RefComponentSpace(Bones, Axis) : FTransform::Identity;
	FVector Across = AxisRef.InverseTransformVectorNoScale(GSTNStanceWidth::RefComponentSpace(Bones, Legs[0].Foot).GetLocation()
		- GSTNStanceWidth::RefComponentSpace(Bones, Legs[1].Foot).GetLocation());
	Across.Z = 0.0;
	if (!Across.Normalize())
	{
		return;
	}
	SideAxis = Across;
	Legs[0].Side = 1.0;
	Legs[1].Side = -1.0;
	bValid = true;
}

void FGSTN_AnimNode_StanceWidth::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	Source.Update(Context);
	Offset = GSTNStanceWidth::CVarWidth.GetValueOnAnyThread();
	MinHalfWidth = GSTNStanceWidth::CVarMinHalf.GetValueOnAnyThread();
	Force = FMath::Clamp(GSTNStanceWidth::CVarForce.GetValueOnAnyThread(), 0.f, 1.f);
}

void FGSTN_AnimNode_StanceWidth::Evaluate_AnyThread(FPoseContext& Output)
{
	Source.Evaluate(Output);
	Legs[0].LastMove = Legs[1].LastMove = 0.0;
	Legs[0].LastWeight = Legs[1].LastWeight = 0.f;
	if (!bValid || (Offset == 0.f && MinHalfWidth <= 0.f))
	{
		return;
	}

	// Per-foot weight: the clip's curve as blended so far (no curve = 0), or the forced weight
	const FName Curves[2] = {LeftCurve, RightCurve};
	float Weights[2];
	for (int32 i = 0; i < 2; ++i)
	{
		const float Curve = Curves[i].IsNone() ? 0.f : Output.Curve.Get(Curves[i]);
		Weights[i] = FMath::Max(FMath::Clamp(Curve, 0.f, 1.f), Force);
	}
	if (Weights[0] <= ZERO_ANIMWEIGHT_THRESH && Weights[1] <= ZERO_ANIMWEIGHT_THRESH)
	{
		return;  // pass through
	}

	FCSPose<FCompactPose> CSPose;
	CSPose.InitPose(Output.Pose);
	const FTransform AxisCS = Axis != INDEX_NONE ? CSPose.GetComponentSpaceTransform(Axis) : FTransform::Identity;
	bool bChanged = false;
	for (int32 i = 0; i < 2; ++i)
	{
		FLeg& Leg = Legs[i];
		Leg.LastWeight = Weights[i];
		if (Weights[i] <= ZERO_ANIMWEIGHT_THRESH)
		{
			continue;
		}
		FTransform Hip = CSPose.GetComponentSpaceTransform(Leg.Hip);
		FTransform Knee = CSPose.GetComponentSpaceTransform(Leg.Knee);
		FTransform Foot = CSPose.GetComponentSpaceTransform(Leg.Foot);

		const FVector FootInAxis = AxisCS.InverseTransformPosition(Foot.GetLocation());
		const double Lateral = Leg.Side * FVector::DotProduct(FootInAxis, SideAxis);
		const double Move = OutwardMove(Lateral, Offset, MinHalfWidth, FloorSoftness) * Weights[i];
		Leg.LastMove = Move;
		if (FMath::Abs(Move) < 0.01)
		{
			continue;
		}
		const FVector Delta = AxisCS.TransformVectorNoScale(SideAxis * (Leg.Side * Move));

		// Knee plane kept: the pole moves with the foot; the foot keeps its component-space rotation (ball and toes follow as children)
		const FQuat KeepRotation = Foot.GetRotation();
		const FVector Target = Foot.GetLocation() + Delta;
		AnimationCore::SolveTwoBoneIK(Hip, Knee, Foot, Knee.GetLocation() + Delta, Target, false, 1.0, 1.0);
		Foot.SetRotation(KeepRotation);
		const FBoneTransform Chain[3] = {FBoneTransform(Leg.Hip, Hip), FBoneTransform(Leg.Knee, Knee), FBoneTransform(Leg.Foot, Foot)};
		CSPose.LocalBlendCSBoneTransforms(Chain, 1.f);

		if (Leg.IKFoot != INDEX_NONE)
		{
			FTransform IKFoot = CSPose.GetComponentSpaceTransform(Leg.IKFoot);
			IKFoot.AddToTranslation(Delta);
			const FBoneTransform IKChain[1] = {FBoneTransform(Leg.IKFoot, IKFoot)};
			CSPose.LocalBlendCSBoneTransforms(IKChain, 1.f);
		}
		bChanged = true;
	}
	if (bChanged)
	{
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CSPose), Output.Pose);
	}
}

void FGSTN_AnimNode_StanceWidth::GatherDebugData(FNodeDebugData& DebugData)
{
	DebugData.AddDebugItem(FString::Printf(TEXT("%s (offset %.1f, min half %.1f, force %.2f; L w %.2f move %.1f cm, R w %.2f move %.1f cm)"),
		*DebugData.GetNodeName(this), Offset, MinHalfWidth, Force, Legs[0].LastWeight, Legs[0].LastMove, Legs[1].LastWeight, Legs[1].LastMove));
	Source.GatherDebugData(DebugData);
}
