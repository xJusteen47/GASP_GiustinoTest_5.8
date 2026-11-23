// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_AnimNode_DirectionalHitAdditive.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "BonePose.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimNode_DirectionalHitAdditive)

namespace GSTNHitAdditive
{
	static TAutoConsoleVariable<int32> CVarEnable(TEXT("a.GSTN.HitAdditive"), 1,
		TEXT("1: directional hit additives on the torso (default). 0: the node passes the pose through."));
}

FGSTN_AnimNode_DirectionalHitAdditive::FGSTN_AnimNode_DirectionalHitAdditive()
{
	// UE5 mannequin names; a bone the skeleton lacks is skipped
	BranchRoot.BoneName = TEXT("spine_01");
	const TCHAR* Names[] = { TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("spine_04"), TEXT("spine_05"), TEXT("neck_01"), TEXT("head") };
	const float Shares[] = { 0.1f, 0.15f, 0.2f, 0.2f, 0.15f, 0.1f, 0.1f };
	for (int32 i = 0; i < UE_ARRAY_COUNT(Names); ++i)
	{
		FGSTN_HitBendBone& Entry = BendChain.AddDefaulted_GetRef();
		Entry.Bone.BoneName = Names[i];
		Entry.Share = Shares[i];
	}
}

float FGSTN_AnimNode_DirectionalHitAdditive::PlaceholderCurve(float Time, float PeakSeconds, float LengthSeconds)
{
	if (Time <= 0.f || Time >= LengthSeconds)
	{
		return 0.f;
	}
	const float Peak = FMath::Clamp(PeakSeconds, KINDA_SMALL_NUMBER, LengthSeconds - KINDA_SMALL_NUMBER);
	if (Time < Peak)
	{
		return FMath::Sin(HALF_PI * Time / Peak);  // fast out of the hit, eases into the peak
	}
	const float Back = (Time - Peak) / (LengthSeconds - Peak);
	return 0.5f * (1.f + FMath::Cos(PI * Back));   // eases out of the peak and into the base pose
}

void FGSTN_AnimNode_DirectionalHitAdditive::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);
	bHasRequest = false;
}

void FGSTN_AnimNode_DirectionalHitAdditive::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Source.CacheBones(Context);
	const FBoneContainer& Bones = Context.AnimInstanceProxy->GetRequiredBones();
	BranchRoot.Initialize(Bones);
	for (FGSTN_HitBendBone& Entry : BendChain)
	{
		Entry.Bone.Initialize(Bones);
	}

	const int32 Num = Bones.GetCompactPoseNumBones();
	InBranch.Init(false, Num);
	if (!BranchRoot.IsValidToEvaluate(Bones))
	{
		return;
	}
	const FCompactPoseBoneIndex Root = BranchRoot.GetCompactPoseIndex(Bones);
	// Compact indices are sorted parents first, so one pass marks the whole branch
	for (int32 i = 0; i < Num; ++i)
	{
		const FCompactPoseBoneIndex Index(i);
		if (Index == Root)
		{
			InBranch[i] = true;
			continue;
		}
		const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Index);
		InBranch[i] = Parent.IsValid() && InBranch[Parent.GetInt()];
	}
}

void FGSTN_AnimNode_DirectionalHitAdditive::PreUpdate(const UAnimInstance* InAnimInstance)
{
	bHasRequest = false;
	ExtraRequests.Reset();
	const AActor* Owner = InAnimInstance ? InAnimInstance->GetOwningActor() : nullptr;
	if (!Owner)
	{
		return;
	}
	// A component added at runtime (the run skid source is created at BeginPlay) changes the count: the sources are looked up again
	if (CachedOwner.Get() != Owner || !CachedSource.IsValid() || CachedComponentCount != Owner->GetComponents().Num())
	{
		CachedComponentCount = Owner->GetComponents().Num();
		// Every component with the interface: the hit reactions first (the debug display reads the first), then the others in order
		CachedOwner = Owner;
		CachedSource = nullptr;
		CachedExtraSources.Reset();
		for (UActorComponent* Component : Owner->GetComponentsByInterface(UGSTN_AdditiveReactionSource::StaticClass()))
		{
			if (!CachedSource.IsValid())
			{
				CachedSource = Component;
			}
			else
			{
				CachedExtraSources.Add(Component);
			}
		}
	}
	if (const IGSTN_AdditiveReactionSource* SourceComponent = Cast<IGSTN_AdditiveReactionSource>(CachedSource.Get()))
	{
		Request = FGSTN_AdditiveReactionRequest();
		SourceComponent->GetAdditiveReaction(Request);
		bHasRequest = true;
	}
	ExtraRequests.Reset();
	for (const TWeakObjectPtr<UObject>& Extra : CachedExtraSources)
	{
		if (const IGSTN_AdditiveReactionSource* ExtraComponent = Cast<IGSTN_AdditiveReactionSource>(Extra.Get()))
		{
			ExtraComponent->GetAdditiveReaction(ExtraRequests.AddDefaulted_GetRef());
		}
	}
}

void FGSTN_AnimNode_DirectionalHitAdditive::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	Source.Update(Context);
}

void FGSTN_AnimNode_DirectionalHitAdditive::Evaluate_AnyThread(FPoseContext& Output)
{
	Source.Evaluate(Output);
	if (!bHasRequest || GSTNHitAdditive::CVarEnable.GetValueOnAnyThread() == 0)
	{
		return;
	}
	auto Draw = [this, &Output](const FGSTN_AdditiveReactionRequest& Drawn)
	{
		for (const FGSTN_AdditiveReactionLayer& Layer : Drawn.Layers)
		{
			if (!Layer.IsActive())
			{
				continue;
			}
			if (Layer.Additive)
			{
				ApplyClip(Output, Layer);
			}
			else
			{
				ApplyPlaceholder(Output, Layer);
			}
		}
	};
	Draw(Request);
	for (const FGSTN_AdditiveReactionRequest& Extra : ExtraRequests)
	{
		Draw(Extra);
	}
}

void FGSTN_AnimNode_DirectionalHitAdditive::ApplyClip(FPoseContext& Output, const FGSTN_AdditiveReactionLayer& Layer) const
{
	const UAnimSequence* Sequence = Cast<UAnimSequence>(Layer.Additive);
	if (!Sequence || !Sequence->IsValidAdditive() || InBranch.Num() != Output.Pose.GetNumBones())
	{
		return;  // a non-additive clip would replace the pose instead of adding to it: draw nothing rather than a wrong pose
	}
	FPoseContext AdditivePose(Output, true);
	FAnimationPoseData AdditiveData(AdditivePose);
	Sequence->GetAnimationPose(AdditiveData, FAnimExtractContext(static_cast<double>(Layer.Time), false));

	// The legs and the root keep the base pose (and the foot lock under it)
	for (const FCompactPoseBoneIndex Index : AdditivePose.Pose.ForEachBoneIndex())
	{
		if (!InBranch[Index.GetInt()])
		{
			AdditivePose.Pose[Index].SetIdentityZeroScale();
		}
	}

	FAnimationPoseData OutputData(Output);
	FAnimationRuntime::AccumulateAdditivePose(OutputData, AdditiveData, FMath::Clamp(Layer.Weight, 0.f, 1.f), Sequence->GetAdditiveAnimType());
	Output.Pose.NormalizeRotations();
}

void FGSTN_AnimNode_DirectionalHitAdditive::ApplyPlaceholder(FPoseContext& Output, const FGSTN_AdditiveReactionLayer& Layer) const
{
	const float Curve = PlaceholderCurve(Layer.Time, Layer.PlaceholderPeakSeconds, Layer.PlaceholderLengthSeconds);
	const float AngleDeg = Layer.PlaceholderAngleDeg * FMath::Clamp(Layer.Weight, 0.f, 1.f) * Curve;
	if (FMath::IsNearlyZero(AngleDeg))
	{
		return;
	}
	const FTransform& Component = Output.AnimInstanceProxy->GetComponentTransform();
	FVector Push = Component.InverseTransformVectorNoScale(Layer.PushDirection);
	const FVector Up = Component.InverseTransformVectorNoScale(FVector::UpVector);
	Push -= Up * FVector::DotProduct(Push, Up);
	// Turning the up vector toward the push bends the torso the way the hit pushes it
	const FVector Axis = FVector::CrossProduct(Up, Push).GetSafeNormal();
	if (Axis.IsNearlyZero())
	{
		return;
	}

	// Parent rotations of the INPUT pose: a bone turned by theta about a component-space axis keeps the turns of its parents, so
	// L' = P^-1 * R(theta) * P * L with P the original parent rotation (the turns about one axis add up along the chain)
	FCSPose<FCompactPose> CSPose;
	CSPose.InitPose(Output.Pose);
	const FBoneContainer& Bones = Output.AnimInstanceProxy->GetRequiredBones();
	for (const FGSTN_HitBendBone& Entry : BendChain)
	{
		if (!Entry.Bone.IsValidToEvaluate(Bones))
		{
			continue;
		}
		const FCompactPoseBoneIndex Index = Entry.Bone.GetCompactPoseIndex(Bones);
		const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Index);
		const FQuat ParentRot = Parent.IsValid() ? CSPose.GetComponentSpaceTransform(Parent).GetRotation() : FQuat::Identity;
		const FQuat Turn(Axis, FMath::DegreesToRadians(AngleDeg * Entry.Share));
		FTransform& Local = Output.Pose[Index];
		Local.SetRotation((ParentRot.Inverse() * Turn * ParentRot * Local.GetRotation()).GetNormalized());
	}
}

void FGSTN_AnimNode_DirectionalHitAdditive::GatherDebugData(FNodeDebugData& DebugData)
{
	const FGSTN_AdditiveReactionLayer& Newest = Request.Layers[0];
	DebugData.AddDebugItem(FString::Printf(TEXT("%s (%s, t %.2f, w %.2f)"), *DebugData.GetNodeName(this),
		Newest.Additive ? *Newest.Additive->GetName() : (Newest.IsActive() ? TEXT("PLACEHOLDER bend") : TEXT("idle")), Newest.Time, Newest.Weight));
	Source.GatherDebugData(DebugData);
}
