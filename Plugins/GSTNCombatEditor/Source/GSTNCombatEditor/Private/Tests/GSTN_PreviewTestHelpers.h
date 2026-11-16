// Helpers of the GSTNCombat.Editor.Preview.* and GSTN.Visual.Timeline.Preview tests: engine content to preview and a montage built in memory.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Preview/GSTN_PreviewSubject.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPreviewTest
{
	/** A one-bone skeletal mesh of the engine's own content: a non-humanoid subject with nothing a character convention could match */
	inline USkeletalMesh* LoadEngineCube()
	{
		return LoadObject<USkeletalMesh>(nullptr, TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	inline UStaticMesh* LoadEngineStaticCube()
	{
		return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** Centimetres per second the synthetic montage moves its bone along X */
	constexpr double LinearSpeed = 300.0;

	/**
	 * A montage of 2 seconds on the mesh's skeleton whose first bone slides along X at LinearSpeed (so its pose at montage time t is x = 300 t) with a
	 * second section "Late" starting at 1 s. Dynamic (transient) assets: Sequence keeps the sequence alive for the caller.
	 */
	inline UAnimMontage* MakeLinearMontage(USkeletalMesh& Mesh, TStrongObjectPtr<UAnimSequence>& OutSequence, TStrongObjectPtr<UAnimMontage>& OutMontage)
	{
		USkeleton* Skeleton = Mesh.GetSkeleton();
		if (Skeleton == nullptr || Mesh.GetRefSkeleton().GetNum() == 0)
		{
			return nullptr;
		}
		const FName Bone = Mesh.GetRefSkeleton().GetBoneName(0);

		UAnimSequence* Sequence = NewObject<UAnimSequence>(GetTransientPackage());
		Sequence->SetSkeleton(Skeleton);
		IAnimationDataController& Controller = Sequence->GetController();
		Controller.InitializeModel();
		Controller.OpenBracket(FText::FromString(TEXT("GSTN test sequence")), false);
		Controller.SetFrameRate(FFrameRate(30, 1), false);
		Controller.SetNumberOfFrames(FFrameNumber(60), false);
		Controller.AddBoneCurve(Bone, false);
		TArray<FVector> Positions;
		TArray<FQuat> Rotations;
		TArray<FVector> Scales;
		for (int32 Key = 0; Key <= 60; ++Key)
		{
			Positions.Add(FVector(LinearSpeed * Key / 30.0, 0.0, 0.0));
			Rotations.Add(FQuat::Identity);
			Scales.Add(FVector::OneVector);
		}
		Controller.SetBoneTrackKeys(Bone, Positions, Rotations, Scales, false);
		Controller.NotifyPopulated();
		Controller.CloseBracket(false);
		Sequence->CacheDerivedDataForCurrentPlatform();
		OutSequence.Reset(Sequence);

		UAnimMontage* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Sequence, TEXT("DefaultSlot"), 0.25f, 0.25f);
		if (Montage == nullptr)
		{
			return nullptr;
		}
		Montage->AddAnimCompositeSection(TEXT("Late"), 1.f);
		OutMontage.Reset(Montage);
		return Montage;
	}

	inline FGSTN_ResolvedSubject ResolvedFor(USkeletalMesh* Mesh)
	{
		FGSTN_ResolvedSubject Subject;
		Subject.Mesh = Mesh;
		Subject.Label = FText::FromString(Mesh ? Mesh->GetName() : TEXT("none"));
		return Subject;
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS
