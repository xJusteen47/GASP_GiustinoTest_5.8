#include "GSTN_AnimNode_ClipAtTime.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimationPoseData.h"
#include "GSTN_ClipTimeSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimNode_ClipAtTime)

float FGSTN_AnimNode_ClipAtTime::ClampTime(float InTime, float Length)
{
	return Length > 0.f ? FMath::Clamp(InTime, 0.f, Length) : 0.f;
}

void FGSTN_AnimNode_ClipAtTime::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
}

void FGSTN_AnimNode_ClipAtTime::PreUpdate(const UAnimInstance* InAnimInstance)
{
	UAnimSequenceBase* NewClip = nullptr;
	float NewTime = 0.f;
	const IGSTN_ClipTimeSource* Source = Cast<IGSTN_ClipTimeSource>(InAnimInstance);
	if (Source && Source->GetClipAtTime(NewClip, NewTime) && NewClip)
	{
		Clip = NewClip;
		Time = ClampTime(NewTime, NewClip->GetPlayLength());
	}
	else
	{
		Clip = nullptr;
		Time = 0.f;
	}
}

void FGSTN_AnimNode_ClipAtTime::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	// The time is the source's (PreUpdate): nothing advances here
}

void FGSTN_AnimNode_ClipAtTime::Evaluate_AnyThread(FPoseContext& Output)
{
	if (!Clip)
	{
		Output.ResetToRefPose();
		return;
	}
	FAnimationPoseData PoseData(Output);
	Clip->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(Time), false));
}

void FGSTN_AnimNode_ClipAtTime::GatherDebugData(FNodeDebugData& DebugData)
{
	DebugData.AddDebugItem(FString::Printf(TEXT("%s (%s @ %.3f s)"), *DebugData.GetNodeName(this), *GetNameSafe(Clip), Time), true);
}
