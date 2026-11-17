// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_AnimNode_CoveredLanding.h"

#include "GSTN_CharacterAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimNode_CoveredLanding)

namespace GSTNCoveredLanding
{
	static TAutoConsoleVariable<int32> CVarEnable(TEXT("a.GSTN.CoveredLanding"), 1,
		TEXT("1: hold the montage's last pose over the base layer's landing after a landing made under a montage (default). 0: pass through."));
}

void FGSTN_AnimNode_CoveredLanding::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);
	Held.Reset();
	bCapture = false;
	HoldAlpha = 0.f;
}

void FGSTN_AnimNode_CoveredLanding::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Source.CacheBones(Context);
	// Compact indices change with the LOD: a pose kept by index is no longer valid
	Held.Reset();
}

void FGSTN_AnimNode_CoveredLanding::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	Source.Update(Context);
	// Written by the anim instance on the game thread before the parallel update starts (NativeUpdateAnimation)
	const UGSTN_CharacterAnimInstance* Instance = Cast<UGSTN_CharacterAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject());
	const bool bEnabled = GSTNCoveredLanding::CVarEnable.GetValueOnAnyThread() != 0 && Instance;
	bCapture = bEnabled && Instance->bCoveredLandingCapture;
	HoldAlpha = bEnabled ? Instance->CoveredLandingAlpha : 0.f;
}

void FGSTN_AnimNode_CoveredLanding::Evaluate_AnyThread(FPoseContext& Output)
{
	Source.Evaluate(Output);
	FCompactPose& Pose = Output.Pose;
	const int32 NumBones = Pose.GetNumBones();
	if (bCapture)
	{
		Held.SetNumUninitialized(NumBones);
		for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
		{
			Held[Bone.GetInt()] = Pose[Bone];
		}
		return;
	}
	if (HoldAlpha <= 0.f || Held.Num() != NumBones)
	{
		return;
	}
	for (const FCompactPoseBoneIndex Bone : Pose.ForEachBoneIndex())
	{
		FTransform& Live = Pose[Bone];
		Live.BlendWith(Held[Bone.GetInt()], HoldAlpha);
	}
	Pose.NormalizeRotations();
}

void FGSTN_AnimNode_CoveredLanding::GatherDebugData(FNodeDebugData& DebugData)
{
	DebugData.AddDebugItem(FString::Printf(TEXT("GSTN Covered Landing (capture %d, hold %.2f)"), bCapture ? 1 : 0, HoldAlpha));
	Source.GatherDebugData(DebugData);
}
