#include "GSTN_AnimNode_ChooserPlayer.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_Inertialization.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimationPoseData.h"
#include "Chooser.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimNode_ChooserPlayer)

float FGSTN_AnimNode_ChooserPlayer::AdvanceTime(float InTime, float DeltaSeconds, float Rate, float Length, bool bInLooping)
{
	if (Length <= 0.f)
	{
		return 0.f;
	}
	const float Next = InTime + DeltaSeconds * Rate;
	if (bInLooping)
	{
		const float Wrapped = FMath::Fmod(Next, Length);
		return Wrapped < 0.f ? Wrapped + Length : Wrapped;
	}
	return FMath::Clamp(Next, 0.f, Length);
}

bool FGSTN_AnimNode_ChooserPlayer::ResolveLooping(EGSTN_ChooserPlayerLoop Mode, bool bAssetLooping)
{
	switch (Mode)
	{
	case EGSTN_ChooserPlayerLoop::Loop:
		return true;
	case EGSTN_ChooserPlayerLoop::Once:
		return false;
	default:
		return bAssetLooping;
	}
}

void FGSTN_AnimNode_ChooserPlayer::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	Clip = nullptr;
	Time = 0.f;
	UpdateCounter.Reset();
}

bool FGSTN_AnimNode_ChooserPlayer::Choose(const FAnimationBaseContext& Context)
{
	if (!Chooser)
	{
		return false;
	}
	// Same evaluation GASP's state entry does (EvaluateChooser with the ABP and the OUT struct); the columns read plain properties of the
	// instance, which is safe on the worker thread (the instance's thread-safe update wrote them before the graph update)
	UObject* InstanceObject = Context.AnimInstanceProxy ? Context.AnimInstanceProxy->GetAnimInstanceObject() : nullptr;
	FGSTN_ChooserPlayerOutput NewOutput;
	FChooserEvaluationContext ChooserContext;
	ChooserContext.AddObjectParam(InstanceObject);
	ChooserContext.AddStructParam(NewOutput);
	UObject* Result = nullptr;
	UChooserTable::EvaluateChooser(ChooserContext, Chooser, FObjectChooserBase::FObjectChooserIteratorCallback::CreateLambda(
		[&Result](UObject* InResult)
		{
			Result = InResult;
			return FObjectChooserBase::EIteratorStatus::Stop;
		}));
	UAnimSequenceBase* NewClip = Cast<UAnimSequenceBase>(Result);
	if (!NewClip)
	{
		return false;
	}
	Clip = NewClip;
	Output = NewOutput;
	return true;
}

void FGSTN_AnimNode_ChooserPlayer::StartClip(const FAnimationUpdateContext& Context)
{
	bLooping = ResolveLooping(Output.Loop, Clip && Clip->bLoop);
	Time = Clip ? FMath::Clamp(Output.StartTime, 0.f, Clip->GetPlayLength()) : 0.f;
	if (Output.BlendTime > 0.f)
	{
		if (UE::Anim::IInertializationRequester* Requester = Context.GetMessage<UE::Anim::IInertializationRequester>())
		{
			FInertializationRequest Request;
			Request.Duration = Output.BlendTime;
			Requester->RequestInertialization(Request);
		}
	}
}

void FGSTN_AnimNode_ChooserPlayer::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	const bool bJustBecameRelevant = !UpdateCounter.HasEverBeenUpdated()
		|| !UpdateCounter.WasSynchronizedCounter(Context.AnimInstanceProxy->GetUpdateCounter());
	UpdateCounter.SynchronizeWith(Context.AnimInstanceProxy->GetUpdateCounter());

	if (bJustBecameRelevant)
	{
		if (Choose(Context))
		{
			StartClip(Context);
		}
		return;
	}
	if (Output.bReselectWhileActive)
	{
		UAnimSequenceBase* Previous = Clip;
		if (Choose(Context) && Clip != Previous)
		{
			StartClip(Context);
			return;
		}
	}
	if (Clip)
	{
		Time = AdvanceTime(Time, Context.GetDeltaTime(), Output.PlayRate, Clip->GetPlayLength(), bLooping);
	}
}

void FGSTN_AnimNode_ChooserPlayer::Evaluate_AnyThread(FPoseContext& PoseOutput)
{
	if (!Clip)
	{
		PoseOutput.ResetToRefPose();
		return;
	}
	FAnimationPoseData PoseData(PoseOutput);
	Clip->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(Time), false));
}

void FGSTN_AnimNode_ChooserPlayer::GatherDebugData(FNodeDebugData& DebugData)
{
	DebugData.AddDebugItem(FString::Printf(TEXT("%s (%s @ %.3f s, rate %.2f%s)"), *DebugData.GetNodeName(this), *GetNameSafe(Clip), Time,
		Output.PlayRate, bLooping ? TEXT(", loop") : TEXT("")), true);
}
