// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_AnimNode_GroundContactFix.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameplayTagAssetInterface.h"
#include "HAL/IConsoleManager.h"
#include "TwoBoneIK.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AnimNode_GroundContactFix)

namespace GSTNGroundContact
{
	static TAutoConsoleVariable<int32> CVarEnable(TEXT("a.GSTN.GroundContactFix"), 1,
		TEXT("1: keep body, hands and feet above the floor in the states of the ground contact settings (default). 0: pass the pose through."));
	static TAutoConsoleVariable<int32> CVarDebug(TEXT("gstn.Anim.Debug.GroundContactFix"), 0,
		TEXT("1: draw the floor plane, the contact spheres (red = sinking in the input pose) and the lift."));

	static bool OwnerHasAnyTag(const TArray<TWeakObjectPtr<const UObject>>& Sources, const FGameplayTagContainer& Tags)
	{
		for (const TWeakObjectPtr<const UObject>& Source : Sources)
		{
			if (const IGameplayTagAssetInterface* TagSource = Cast<const IGameplayTagAssetInterface>(Source.Get()))
			{
				if (TagSource->HasAnyMatchingGameplayTags(Tags))
				{
					return true;
				}
			}
		}
		return false;
	}
}

void FGSTN_AnimNode_GroundContactFix::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	FAnimNode_Base::Initialize_AnyThread(Context);
	Source.Initialize(Context);
	Weight = 0.f;
	Lift = 0.0;
}

void FGSTN_AnimNode_GroundContactFix::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	Source.CacheBones(Context);
	RuntimeBones.Reset();
	LiftIndex = FCompactPoseBoneIndex(INDEX_NONE);
	if (!Settings)
	{
		return;
	}
	const FBoneContainer& Bones = Context.AnimInstanceProxy->GetRequiredBones();
	auto Resolve = [&Bones](FName Name)
	{
		FBoneReference Ref(Name);
		return Ref.Initialize(Bones) && Ref.IsValidToEvaluate(Bones) ? Ref.GetCompactPoseIndex(Bones) : FCompactPoseBoneIndex(INDEX_NONE);
	};
	LiftIndex = Resolve(Settings->LiftBone);
	for (const FGSTN_GroundContactBone& Entry : Settings->Bones)
	{
		FGSTN_GroundContactRuntimeBone Bone;
		Bone.Index = Resolve(Entry.Bone);
		Bone.Role = Entry.Role;
		Bone.Radius = Entry.Radius;
		if (Bone.Index == INDEX_NONE)
		{
			continue;  // bone missing on this skeleton or stripped at this LOD
		}
		if (Bone.Role == EGSTN_GroundContactRole::LimbEnd)
		{
			Bone.Mid = Bones.GetParentBoneIndex(Bone.Index);
			Bone.Root = Bone.Mid != INDEX_NONE ? Bones.GetParentBoneIndex(Bone.Mid) : FCompactPoseBoneIndex(INDEX_NONE);
			if (Bone.Root == INDEX_NONE)
			{
				continue;
			}
		}
		RuntimeBones.Add(Bone);
	}
}

float FGSTN_AnimNode_GroundContactFix::TargetWeightFromRules(const UAnimInstance* AnimInstance, float& OutBlendIn, float& OutBlendOut)
{
	const AActor* Owner = AnimInstance->GetOwningActor();
	if (Owner && (CachedOwner.Get() != Owner || CachedComponentCount != Owner->GetComponents().Num()))
	{
		CachedOwner = Owner;
		CachedComponentCount = Owner->GetComponents().Num();
		CachedTagSources.Reset();
		if (Cast<const IGameplayTagAssetInterface>(Owner))
		{
			CachedTagSources.Add(Owner);
		}
		for (const UActorComponent* Component : Owner->GetComponents())
		{
			if (Cast<const IGameplayTagAssetInterface>(Component))
			{
				CachedTagSources.Add(Component);
			}
		}
	}

	float Best = 0.f;
	for (const FGSTN_GroundContactRule& Rule : Settings->Rules)
	{
		if (Rule.Weight <= Best)
		{
			continue;
		}
		bool bMatch = !Rule.AnyTags.IsEmpty() && GSTNGroundContact::OwnerHasAnyTag(CachedTagSources, Rule.AnyTags);
		for (int32 i = 0; !bMatch && i < Rule.Montages.Num(); ++i)
		{
			bMatch = Rule.Montages[i] && AnimInstance->Montage_IsPlaying(Rule.Montages[i]);
		}
		if (bMatch)
		{
			Best = Rule.Weight;
			OutBlendIn = Rule.BlendInTime;
			OutBlendOut = Rule.BlendOutTime;
		}
	}
	if (!Settings->CurveName.IsNone())
	{
		const float Curve = FMath::Clamp(AnimInstance->GetCurveValue(Settings->CurveName), 0.f, 1.f);
		if (Curve > Best)
		{
			// The curve is already smooth: follow it at once
			Best = Curve;
			OutBlendIn = 0.f;
			OutBlendOut = 0.f;
		}
	}
	return Best;
}

void FGSTN_AnimNode_GroundContactFix::PreUpdate(const UAnimInstance* InAnimInstance)
{
	TargetWeight = 0.f;
	bHasFloor = false;
	if (!Settings || !InAnimInstance || GSTNGroundContact::CVarEnable.GetValueOnGameThread() == 0)
	{
		return;
	}
	TargetWeight = TargetWeightFromRules(InAnimInstance, BlendIn, BlendOut);
	if (TargetWeight <= 0.f && Weight <= 0.f)
	{
		return;  // off: no trace
	}

	const USkeletalMeshComponent* Mesh = InAnimInstance->GetSkelMeshComponent();
	const AActor* Owner = InAnimInstance->GetOwningActor();
	UWorld* World = Mesh ? Mesh->GetWorld() : nullptr;
	if (!World || !Owner)
	{
		return;
	}
	// Last frame's bone: the floor under the body, whatever the capsule does (a downed body lies away from the capsule centre)
	const FVector Up = Owner->GetActorUpVector();
	const int32 StartBone = Mesh->GetBoneIndex(Settings->TraceStartBone);
	const FVector Origin = StartBone != INDEX_NONE ? Mesh->GetBoneLocation(Settings->TraceStartBone) : Owner->GetActorLocation();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSTN_GroundContactFix), false, Owner);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Origin + Up * Settings->TraceUp, Origin - Up * Settings->TraceDown, Settings->TraceChannel, Params)
		&& FVector::DotProduct(Hit.ImpactNormal, Up) >= FMath::Cos(FMath::DegreesToRadians(Settings->MaxFloorSlopeDeg)))
	{
		bHasFloor = true;
		FloorWorld.Point = Hit.ImpactPoint;
		FloorWorld.Normal = Hit.ImpactNormal.GetSafeNormal();
	}
}

void FGSTN_AnimNode_GroundContactFix::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	Source.Update(Context);
	DeltaTime = Context.GetDeltaTime();
	Weight = FGSTN_GroundContactSolver::BlendWeight(Weight, TargetWeight, DeltaTime, BlendIn, BlendOut);
}

void FGSTN_AnimNode_GroundContactFix::Evaluate_AnyThread(FPoseContext& Output)
{
	Source.Evaluate(Output);
	LastRequiredLift = 0.0;
	LastLimbFixes = 0;
	if (Weight <= ZERO_ANIMWEIGHT_THRESH || !bHasFloor || !Settings || RuntimeBones.IsEmpty() || !IsLODEnabled(Output.AnimInstanceProxy))
	{
		Lift = 0.0;
		return;
	}

	const FTransform ComponentToWorld = Output.AnimInstanceProxy->GetComponentTransform();
	FGSTN_GroundPlane Floor;
	Floor.Point = ComponentToWorld.InverseTransformPosition(FloorWorld.Point);
	Floor.Normal = ComponentToWorld.InverseTransformVectorNoScale(FloorWorld.Normal).GetSafeNormal();

	FCSPose<FCompactPose> CSPose;
	CSPose.InitPose(Output.Pose);

	// 1. Body lift from the deepest body sphere, in the input pose
	TArray<FVector, TInlineAllocator<16>> Original;
	TArray<FVector, TInlineAllocator<16>> BodyPoints;
	TArray<double, TInlineAllocator<16>> BodyRadii;
	for (const FGSTN_GroundContactRuntimeBone& Bone : RuntimeBones)
	{
		Original.Add(CSPose.GetComponentSpaceTransform(Bone.Index).GetLocation());
		if (Bone.Role == EGSTN_GroundContactRole::LiftBody)
		{
			BodyPoints.Add(Original.Last());
			BodyRadii.Add(Bone.Radius);
		}
	}
	LastRequiredLift = FGSTN_GroundContactSolver::RequiredLift(Floor, BodyPoints, BodyRadii, Settings->MaxLift);
	Lift = FGSTN_GroundContactSolver::SmoothLift(Lift, LastRequiredLift, DeltaTime, Settings->LiftReleaseHalfLife);
	const FVector LiftCS = Floor.Normal * (Lift * Weight);

	if (LiftIndex != INDEX_NONE && !LiftCS.IsNearlyZero(0.01))
	{
		// Local-space move of the lift bone (its children follow), then the component-space pose is rebuilt for the limbs
		const FCompactPoseBoneIndex Parent = Output.Pose.GetParentBoneIndex(LiftIndex);
		const FVector LocalMove = Parent != INDEX_NONE ? CSPose.GetComponentSpaceTransform(Parent).InverseTransformVectorNoScale(LiftCS) : LiftCS;
		Output.Pose[LiftIndex].AddToTranslation(LocalMove);
		CSPose.InitPose(Output.Pose);
	}

	// 2. Limb ends: keep their contacts, take them out of the floor
	bool bLimbChanged = false;
	for (int32 i = 0; i < RuntimeBones.Num(); ++i)
	{
		const FGSTN_GroundContactRuntimeBone& Bone = RuntimeBones[i];
		if (Bone.Role != EGSTN_GroundContactRole::LimbEnd)
		{
			continue;
		}
		FTransform Root = CSPose.GetComponentSpaceTransform(Bone.Root);
		FTransform Mid = CSPose.GetComponentSpaceTransform(Bone.Mid);
		FTransform End = CSPose.GetComponentSpaceTransform(Bone.Index);
		FVector Target;
		if (!FGSTN_GroundContactSolver::LimbTarget(Floor, Original[i], End.GetLocation(), Bone.Radius, Settings->ContactBand,
				Settings->bKeepLimbContacts, Settings->MaxLimbCorrection, Target))
		{
			continue;
		}
		// Bend in the plane the pose already bends in (same as the blend-out foot lock)
		const FQuat KeepRotation = End.GetRotation();
		const FVector Centre = (Root.GetLocation() + End.GetLocation()) * 0.5;
		FVector Bend = Mid.GetLocation() - Centre;
		if (!Bend.Normalize())
		{
			Bend = Floor.Normal;
		}
		AnimationCore::SolveTwoBoneIK(Root, Mid, End, Mid.GetLocation() + Bend * 50.0, Target, false, 1.0, 1.0);
		End.SetRotation(KeepRotation);
		const FBoneTransform Chain[3] = {FBoneTransform(Bone.Root, Root), FBoneTransform(Bone.Mid, Mid), FBoneTransform(Bone.Index, End)};
		CSPose.LocalBlendCSBoneTransforms(Chain, Weight);
		bLimbChanged = true;
		++LastLimbFixes;
	}
	if (bLimbChanged)
	{
		FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(MoveTemp(CSPose), Output.Pose);
	}

#if ENABLE_ANIM_DEBUG
	if (GSTNGroundContact::CVarDebug.GetValueOnAnyThread() != 0)
	{
		FAnimInstanceProxy* Proxy = Output.AnimInstanceProxy;
		const FVector P = FloorWorld.Point;
		const FVector T = FVector::CrossProduct(FloorWorld.Normal, FVector::ForwardVector).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
		const FVector B = FVector::CrossProduct(FloorWorld.Normal, T);
		Proxy->AnimDrawDebugLine(P - T * 60.0, P + T * 60.0, FColor::Cyan);
		Proxy->AnimDrawDebugLine(P - B * 60.0, P + B * 60.0, FColor::Cyan);
		for (int32 i = 0; i < RuntimeBones.Num(); ++i)
		{
			const bool bSank = FGSTN_GroundContactSolver::Penetration(Floor, Original[i], RuntimeBones[i].Radius) > 0.0;
			Proxy->AnimDrawDebugSphere(ComponentToWorld.TransformPosition(Original[i]), static_cast<float>(RuntimeBones[i].Radius), 8, bSank ? FColor::Red : FColor::Green);
		}
		Proxy->AnimDrawDebugLine(P, P + FloorWorld.Normal * (Lift * Weight), FColor::Yellow, false, -1.f, 1.5f);
	}
#endif
}

void FGSTN_AnimNode_GroundContactFix::GatherDebugData(FNodeDebugData& DebugData)
{
	DebugData.AddDebugItem(FString::Printf(TEXT("%s (weight %.2f, floor %d, lift %.1f / needed %.1f cm, limb fixes %d)"), *DebugData.GetNodeName(this),
		Weight, bHasFloor ? 1 : 0, Lift * Weight, LastRequiredLift, LastLimbFixes));
	Source.GatherDebugData(DebugData);
}
