// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_AnimNode_BlendOutFootLock.h"

#include "GSTN_CharacterAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimTrace.h"
#include "HAL/IConsoleManager.h"
#include "TwoBoneIK.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimNode_BlendOutFootLock)

namespace GSTNFootLock
{
	static TAutoConsoleVariable<int32> CVarEnable(TEXT("a.GSTN.BlendOutFootLock"), 1,
		TEXT("1: plant the feet while a montage blends back to the game pose, then step (default). 0: the node passes the pose through."));

	/** The calf and thigh of a foot: its parent and grandparent in the compact pose */
	void ResolveLeg(FGSTN_FootLockLeg& Leg, const FBoneContainer& Bones)
	{
		Leg.FootIndex = Leg.CalfIndex = Leg.ThighIndex = FCompactPoseBoneIndex(INDEX_NONE);
		if (!Leg.Foot.Initialize(Bones) || !Leg.Foot.IsValidToEvaluate(Bones))
		{
			return;
		}
		Leg.FootIndex = Leg.Foot.GetCompactPoseIndex(Bones);
		Leg.CalfIndex = Bones.GetParentBoneIndex(Leg.FootIndex);
		Leg.ThighIndex = Leg.CalfIndex != INDEX_NONE ? Bones.GetParentBoneIndex(Leg.CalfIndex) : FCompactPoseBoneIndex(INDEX_NONE);
	}

	float SmoothStep(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
}

FGSTN_AnimNode_BlendOutFootLock::FGSTN_AnimNode_BlendOutFootLock()
{
	LeftLeg.Foot.BoneName = TEXT("foot_l");
	RightLeg.Foot.BoneName = TEXT("foot_r");
}

void FGSTN_AnimNode_BlendOutFootLock::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);
	FreeAll();
	bPrevRequest = false;
	bHasLastComponent = false;
}

void FGSTN_AnimNode_BlendOutFootLock::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Source.CacheBones(Context);
	const FBoneContainer& Bones = Context.AnimInstanceProxy->GetRequiredBones();
	GSTNFootLock::ResolveLeg(LeftLeg, Bones);
	GSTNFootLock::ResolveLeg(RightLeg, Bones);
}

void FGSTN_AnimNode_BlendOutFootLock::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	Source.Update(Context);
	DeltaTime = Context.GetDeltaTime();

	// Written by the anim instance on the game thread before the parallel update starts (NativeUpdateAnimation)
	UGSTN_CharacterAnimInstance* Instance = Cast<UGSTN_CharacterAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject());
	if (Instance)
	{
		// Debug readout for captures (last evaluated phases): left * 10 + right, see EGSTN_FootLockPhase
		Instance->FootLockDebugPhase = static_cast<int32>(LeftLeg.Phase) * 10 + static_cast<int32>(RightLeg.Phase);
	}
	const bool bEnabled = GSTNFootLock::CVarEnable.GetValueOnAnyThread() != 0;
	bRequest = bEnabled && Instance && Instance->bFootLockRequest;
	bInstantRelease = !bEnabled || !Instance || Instance->bFootLockInstantRelease;
	bMoving = Instance && Instance->bFootLockMoving;
	MontageWeight = Instance ? Instance->FootLockMontageWeight : 0.f;
}

void FGSTN_AnimNode_BlendOutFootLock::FreeAll()
{
	for (FGSTN_FootLockLeg* Leg : {&LeftLeg, &RightLeg})
	{
		Leg->Phase = EGSTN_FootLockPhase::Free;
		Leg->Alpha = 0.f;
		Leg->PhaseTime = 0.f;
		Leg->SettledFrames = 0;
		Leg->bHasLastInput = false;
	}
}

void FGSTN_AnimNode_BlendOutFootLock::BeginRelease(const FTransform InputWorld[2], bool bAllowStep)
{
	FGSTN_FootLockLeg* Legs[2] = {&LeftLeg, &RightLeg};
	float Distance[2] = {0.f, 0.f};
	bool bSteps[2] = {false, false};
	for (int32 i = 0; i < 2; ++i)
	{
		if (Legs[i]->Phase != EGSTN_FootLockPhase::Locked)
		{
			continue;
		}
		Distance[i] = FVector::Dist(Legs[i]->Held.GetLocation(), InputWorld[i].GetLocation());
		bSteps[i] = bAllowStep && Distance[i] > StepThreshold;
		Legs[i]->Phase = bSteps[i] ? EGSTN_FootLockPhase::Stepping : EGSTN_FootLockPhase::Releasing;
		Legs[i]->PhaseTime = 0.f;
		Legs[i]->StepDelay = 0.f;
	}
	// Both feet step: the one farther from its spot goes first, the other follows (a walk-like shuffle, never both feet in the air)
	if (bSteps[0] && bSteps[1])
	{
		const int32 Second = Distance[0] >= Distance[1] ? 1 : 0;
		Legs[Second]->StepDelay = StepTime * SecondStepStart;
	}
}

void FGSTN_AnimNode_BlendOutFootLock::Evaluate_AnyThread(FPoseContext& Output)
{
	Source.Evaluate(Output);

	const bool bRising = bRequest && !bPrevRequest;
	bPrevRequest = bRequest;

	const bool bAnyHeld = LeftLeg.Phase != EGSTN_FootLockPhase::Free || RightLeg.Phase != EGSTN_FootLockPhase::Free;
	if (bInstantRelease || (!bAnyHeld && !bRising))
	{
		// Nothing to hold (or a new montage took over: its first frame stays exactly as it is): the pose passes through untouched
		if (bAnyHeld)
		{
			FreeAll();
		}
		bHasLastComponent = false;
		return;
	}
	if (!LeftLeg.IsValid() || !RightLeg.IsValid())
	{
		return;
	}

	const FTransform ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
	if (bHasLastComponent && FVector::Dist(ComponentToWorld.GetLocation(), LastComponentLocation) > TeleportDistance)
	{
		FreeAll();
		bHasLastComponent = false;
		return;
	}
	LastComponentLocation = ComponentToWorld.GetLocation();
	bHasLastComponent = true;

	FCSPose<FCompactPose> CSPose;
	CSPose.InitPose(Output.Pose);

	FGSTN_FootLockLeg* Legs[2] = {&LeftLeg, &RightLeg};
	FTransform InputWorld[2];
	const float Dt = FMath::Max(DeltaTime, KINDA_SMALL_NUMBER);
	for (int32 i = 0; i < 2; ++i)
	{
		FGSTN_FootLockLeg& Leg = *Legs[i];
		const FTransform InputCS = CSPose.GetComponentSpaceTransform(Leg.FootIndex);
		InputWorld[i] = InputCS * ComponentToWorld;
		const bool bSlow = Leg.bHasLastInput && FVector::Dist(InputCS.GetLocation(), Leg.LastInputCS) / Dt < SettleSpeed;
		Leg.SettledFrames = bSlow ? Leg.SettledFrames + 1 : 0;
		Leg.LastInputCS = InputCS.GetLocation();
		Leg.bHasLastInput = true;
		if (bRising && Leg.Phase == EGSTN_FootLockPhase::Free)
		{
			// Blend-out starts: the foot keeps the spot it has now (the montage's last pose)
			Leg.Phase = EGSTN_FootLockPhase::Locked;
			Leg.Held = InputWorld[i];
			Leg.Alpha = 1.f;
			Leg.PhaseTime = 0.f;
			Leg.SettledFrames = 0;
		}
	}

	// Locked feet let go together: the capsule moves (blend back, locomotion takes over), the request ends (time cap) or the game pose
	// has settled with the montage gone (each foot steps to its spot). A foot dragged too far steps on its own.
	const bool bBothLocked = LeftLeg.Phase == EGSTN_FootLockPhase::Locked && RightLeg.Phase == EGSTN_FootLockPhase::Locked;
	const bool bSettled = MontageWeight < ZERO_ANIMWEIGHT_THRESH && LeftLeg.SettledFrames >= SettleFrames && RightLeg.SettledFrames >= SettleFrames;
	if (LeftLeg.Phase == EGSTN_FootLockPhase::Locked || RightLeg.Phase == EGSTN_FootLockPhase::Locked)
	{
		if (bMoving)
		{
			BeginRelease(InputWorld, false);
		}
		else if (!bRequest || (bBothLocked && bSettled))
		{
			BeginRelease(InputWorld, true);
		}
		else
		{
			for (int32 i = 0; i < 2; ++i)
			{
				if (Legs[i]->Phase == EGSTN_FootLockPhase::Locked && FVector::Dist(Legs[i]->Held.GetLocation(), InputWorld[i].GetLocation()) > MaxHoldDistance)
				{
					Legs[i]->Phase = EGSTN_FootLockPhase::Stepping;
					Legs[i]->PhaseTime = 0.f;
					Legs[i]->StepDelay = 0.f;
				}
			}
		}
	}

	for (int32 i = 0; i < 2; ++i)
	{
		FGSTN_FootLockLeg& Leg = *Legs[i];
		if (Leg.Phase == EGSTN_FootLockPhase::Free)
		{
			continue;
		}
		FTransform Target = Leg.Held;
		switch (Leg.Phase)
		{
		case EGSTN_FootLockPhase::Stepping:
		{
			if (bMoving)
			{
				Leg.Phase = EGSTN_FootLockPhase::Releasing;
				Leg.PhaseTime = 0.f;
				break;
			}
			Leg.PhaseTime += DeltaTime;
			const float T = FMath::Clamp((Leg.PhaseTime - Leg.StepDelay) / FMath::Max(StepTime, KINDA_SMALL_NUMBER), 0.f, 1.f);
			const float E = GSTNFootLock::SmoothStep(T);
			Target.SetLocation(FMath::Lerp(Leg.Held.GetLocation(), InputWorld[i].GetLocation(), E) + FVector::UpVector * StepLift * FMath::Sin(PI * T));
			Target.SetRotation(FQuat::Slerp(Leg.Held.GetRotation(), InputWorld[i].GetRotation(), E).GetNormalized());
			Leg.Alpha = 1.f;
			if (T >= 1.f)
			{
				Leg.Phase = EGSTN_FootLockPhase::Free;
				Leg.Alpha = 0.f;
			}
			break;
		}
		case EGSTN_FootLockPhase::Releasing:
			Leg.PhaseTime += DeltaTime;
			Leg.Alpha = 1.f - GSTNFootLock::SmoothStep(Leg.PhaseTime / FMath::Max(ReleaseTime, KINDA_SMALL_NUMBER));
			if (Leg.Alpha <= 0.f)
			{
				Leg.Phase = EGSTN_FootLockPhase::Free;
			}
			break;
		default:
			break;
		}
		if (Leg.Phase == EGSTN_FootLockPhase::Free || Leg.Alpha < ZERO_ANIMWEIGHT_THRESH)
		{
			continue;
		}

		// Two-bone IK of thigh-calf-foot to the target, bending in the plane the pose already bends in
		const FTransform TargetCS = Target.GetRelativeTransform(ComponentToWorld);
		FTransform Thigh = CSPose.GetComponentSpaceTransform(Leg.ThighIndex);
		FTransform Calf = CSPose.GetComponentSpaceTransform(Leg.CalfIndex);
		FTransform Foot = CSPose.GetComponentSpaceTransform(Leg.FootIndex);
		const FVector Mid = (Thigh.GetLocation() + Foot.GetLocation()) * 0.5;
		FVector Bend = Calf.GetLocation() - Mid;
		if (!Bend.Normalize())
		{
			Bend = ComponentToWorld.InverseTransformVectorNoScale(FVector::ForwardVector);
		}
		const FVector JointTarget = Calf.GetLocation() + Bend * 50.0;
		AnimationCore::SolveTwoBoneIK(Thigh, Calf, Foot, JointTarget, TargetCS.GetLocation(), false, 1.0, 1.0);
		Foot.SetRotation(TargetCS.GetRotation());
		const FBoneTransform Chain[3] = {FBoneTransform(Leg.ThighIndex, Thigh), FBoneTransform(Leg.CalfIndex, Calf), FBoneTransform(Leg.FootIndex, Foot)};
		CSPose.LocalBlendCSBoneTransforms(Chain, Leg.Alpha);
	}

	FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CSPose), Output.Pose);
}

void FGSTN_AnimNode_BlendOutFootLock::GatherDebugData(FNodeDebugData& DebugData)
{
	const UEnum* Phases = StaticEnum<EGSTN_FootLockPhase>();
	DebugData.AddDebugItem(FString::Printf(TEXT("%s (L %s %.2f, R %s %.2f, request %d)"), *DebugData.GetNodeName(this),
		*Phases->GetNameStringByValue(static_cast<int64>(LeftLeg.Phase)), LeftLeg.Alpha,
		*Phases->GetNameStringByValue(static_cast<int64>(RightLeg.Phase)), RightLeg.Alpha, bRequest ? 1 : 0));
	Source.GatherDebugData(DebugData);
}
