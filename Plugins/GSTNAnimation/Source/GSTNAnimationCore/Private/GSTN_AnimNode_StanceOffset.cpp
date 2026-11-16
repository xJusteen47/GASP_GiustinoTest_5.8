// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_AnimNode_StanceOffset.h"

#include "GSTN_CharacterAnimInstance.h"
#include "GSTN_StanceTypes.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AttributesRuntime.h"
#include "HAL/IConsoleManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimNode_StanceOffset)

namespace GSTNStanceOffset
{
	static TAutoConsoleVariable<int32> CVarEnable(TEXT("a.GSTN.StanceOffset"), 1,
		TEXT("1: add the chosen upper-body stance at rest (default). 0: the node passes the pose through."));

	float SmoothStep(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	/** Samples one plain sequence at Time into Pose (sized for Bones) */
	void SamplePose(const UAnimSequence& Sequence, float Time, const FBoneContainer& Bones, FCompactPose& Pose)
	{
		Pose.SetBoneContainer(&Bones);
		FBlendedCurve Curve;
		Curve.InitFrom(Bones);
		UE::Anim::FStackAttributeContainer Attributes;
		FAnimationPoseData PoseData(Pose, Curve, Attributes);
		Sequence.GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(FMath::Clamp(Time, 0.f, Sequence.GetPlayLength()))));
	}
}

void FGSTN_AnimNode_StanceOffset::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);
	bCacheValid = false;
	bHasLastOutput = false;
	SwitchAlpha = 0.f;
	Snapshot.Reset();
	SnapshotBones.Reset();
}

void FGSTN_AnimNode_StanceOffset::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Source.CacheBones(Context);
	// Compact indices change with the LOD: rebuild the offsets, drop any pose kept by index
	bCacheValid = false;
	bHasLastOutput = false;
	SwitchAlpha = 0.f;
	Snapshot.Reset();
	SnapshotBones.Reset();
}

void FGSTN_AnimNode_StanceOffset::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	Source.Update(Context);
	DeltaTime = Context.GetDeltaTime();

	// Written by the anim instance on the game thread before the parallel update starts (NativeUpdateAnimation)
	const UGSTN_CharacterAnimInstance* Instance = Cast<UGSTN_CharacterAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject());
	const bool bEnabled = GSTNStanceOffset::CVarEnable.GetValueOnAnyThread() != 0;
	Stance = (bEnabled && Instance) ? Instance->ActiveStance.Get() : nullptr;
	ReturnTime = Instance ? Instance->StanceReturnTime : -1.f;
	RestAlpha = Instance ? Instance->StanceRestAlpha : 0.f;
	MontageScale = Instance ? Instance->StanceMontageScale : 1.f;
	Serial = Instance ? Instance->StanceSerial : 0;
}

bool FGSTN_AnimNode_StanceOffset::RebuildCache(const FBoneContainer& Bones, const UGSTN_StanceDefinition& InStance)
{
	bCacheValid = false;
	Offsets.Reset();
	BoneWeight.Reset();
	BoneFadeTime.Reset();
	WeightedBones.Reset();
	if (!InStance.HasCompatiblePoses())
	{
		return false;
	}

	// The offset: StancePose relative to ReferencePose, per bone in local space (rotation Qs * Qr^-1, translation Ts - Tr)
	FCompactPose StancePose;
	FCompactPose ReferencePose;
	GSTNStanceOffset::SamplePose(*InStance.StancePose, InStance.StancePoseTime, Bones, StancePose);
	GSTNStanceOffset::SamplePose(*InStance.ReferencePose, InStance.ReferencePoseTime, Bones, ReferencePose);

	const int32 NumBones = Bones.GetCompactPoseNumBones();
	Offsets.SetNum(NumBones);
	BoneWeight.Init(0.f, NumBones);
	BoneFadeTime.Init(InStance.FadeInTime, NumBones);
	TArray<bool> Inherit;
	Inherit.Init(false, NumBones);

	// Listed bones first, then children in compact order (parents always come before children)
	TArray<int32> Listed;
	Listed.Init(INDEX_NONE, NumBones);
	for (int32 EntryIndex = 0; EntryIndex < InStance.BoneWeights.Num(); ++EntryIndex)
	{
		// A mesh pose index; a bone the current LOD drops has no compact index
		const int32 MeshIndex = Bones.GetPoseBoneIndexForBoneName(InStance.BoneWeights[EntryIndex].Bone);
		if (MeshIndex == INDEX_NONE)
		{
			continue;
		}
		const FCompactPoseBoneIndex Compact = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
		if (Compact != INDEX_NONE && Compact.GetInt() < NumBones)
		{
			Listed[Compact.GetInt()] = EntryIndex;
		}
	}
	for (FCompactPoseBoneIndex Bone(0); Bone < NumBones; ++Bone)
	{
		const int32 Index = Bone.GetInt();
		if (Listed[Index] != INDEX_NONE)
		{
			const FGSTN_StanceBoneWeight& Entry = InStance.BoneWeights[Listed[Index]];
			BoneWeight[Index] = Entry.Weight;
			BoneFadeTime[Index] = InStance.FadeInTime * Entry.FadeTimeScale;
			Inherit[Index] = Entry.bIncludeChildren;
		}
		else
		{
			const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(Bone);
			if (Parent != INDEX_NONE && Inherit[Parent.GetInt()])
			{
				BoneWeight[Index] = BoneWeight[Parent.GetInt()];
				BoneFadeTime[Index] = BoneFadeTime[Parent.GetInt()];
				Inherit[Index] = true;
			}
		}

		const FTransform& S = StancePose[Bone];
		const FTransform& R = ReferencePose[Bone];
		Offsets[Index] = FTransform((S.GetRotation() * R.GetRotation().Inverse()).GetNormalized(), S.GetTranslation() - R.GetTranslation());
		if (BoneWeight[Index] > 0.f)
		{
			WeightedBones.Add(Bone);
		}
	}

	CachedStance = &InStance;
	CachedBonesSerial = Bones.GetSerialNumber();
	SwitchBlendTime = InStance.SwitchBlendTime;
	bCacheValid = true;
	return true;
}

void FGSTN_AnimNode_StanceOffset::Evaluate_AnyThread(FPoseContext& Output)
{
	Source.Evaluate(Output);

	const UGSTN_StanceDefinition* Current = Stance.Get();
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();

	// A switch of stance asset while it shows: blend from the last output (kept below) over SwitchBlendTime
	if (Serial != LastSerial)
	{
		LastSerial = Serial;
		if (bHasLastOutput && LastMaxAlpha > 0.f)
		{
			Snapshot = LastOutput;
			SnapshotBones = LastBones;
			SwitchAlpha = 1.f;
		}
	}

	const bool bShown = Current && ReturnTime >= 0.f && RestAlpha > 0.f && MontageScale > 0.f;
	if (!bShown && SwitchAlpha <= 0.f)
	{
		// Pass through: nothing touched
		bHasLastOutput = false;
		LastMaxAlpha = 0.f;
		return;
	}

	LastMaxAlpha = 0.f;
	if (bShown && (!bCacheValid || CachedStance.Get() != Current || CachedBonesSerial != Bones.GetSerialNumber()))
	{
		RebuildCache(Bones, *Current);
	}
	if (bShown && bCacheValid)
	{
		const float Common = GSTNStanceOffset::SmoothStep(RestAlpha) * MontageScale;
		for (const FCompactPoseBoneIndex Bone : WeightedBones)
		{
			const int32 Index = Bone.GetInt();
			const float Alpha = BoneWeight[Index] * Common * GSTNStanceOffset::SmoothStep(ReturnTime / FMath::Max(BoneFadeTime[Index], KINDA_SMALL_NUMBER));
			if (Alpha <= 0.f)
			{
				continue;
			}
			LastMaxAlpha = FMath::Max(LastMaxAlpha, Alpha);
			FTransform& Local = Output.Pose[Bone];
			const FQuat Delta = FQuat::Slerp(FQuat::Identity, Offsets[Index].GetRotation(), Alpha);
			Local.SetRotation((Delta * Local.GetRotation()).GetNormalized());
			Local.AddToTranslation(Offsets[Index].GetTranslation() * Alpha);
		}
	}

	if (SwitchAlpha > 0.f)
	{
		const float Blend = GSTNStanceOffset::SmoothStep(SwitchAlpha);
		for (int32 i = 0; i < SnapshotBones.Num(); ++i)
		{
			if (SnapshotBones[i].GetInt() < Bones.GetCompactPoseNumBones())
			{
				FTransform& Local = Output.Pose[SnapshotBones[i]];
				Local.BlendWith(Snapshot[i], Blend);
			}
		}
		SwitchAlpha = FMath::Max(0.f, SwitchAlpha - DeltaTime / FMath::Max(SwitchBlendTime, KINDA_SMALL_NUMBER));
		LastMaxAlpha = FMath::Max(LastMaxAlpha, Blend);
	}

	// Keep this frame's output of the weighted bones for a later switch
	if (bCacheValid)
	{
		LastBones = WeightedBones;
		LastOutput.SetNum(WeightedBones.Num());
		for (int32 i = 0; i < WeightedBones.Num(); ++i)
		{
			LastOutput[i] = Output.Pose[WeightedBones[i]];
		}
		bHasLastOutput = true;
	}
}

void FGSTN_AnimNode_StanceOffset::GatherDebugData(FNodeDebugData& DebugData)
{
	const UGSTN_StanceDefinition* Current = Stance.Get();
	DebugData.AddDebugItem(FString::Printf(TEXT("GSTN Stance Offset (%s, return %.2fs, rest %.2f, montage scale %.2f, switch %.2f)"),
		Current ? *Current->GetName() : TEXT("none"), ReturnTime, RestAlpha, MontageScale, SwitchAlpha));
	Source.GatherDebugData(DebugData);
}
