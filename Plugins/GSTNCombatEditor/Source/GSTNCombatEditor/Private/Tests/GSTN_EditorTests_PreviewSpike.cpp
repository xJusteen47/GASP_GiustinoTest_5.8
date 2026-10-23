#include "Animation/AnimMontage.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/AutomationTest.h"
#include "PreviewScene.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// Spike S1 of the M5 plan (E0): can the pose of a montage be set per frame on a UDebugSkelMeshComponent, without ticking, and read back
// as component-space socket transforms that repeat? Needs sample content (any montage whose skeleton has a preview mesh; the GASP mannequin's montages here), found
// through the Asset Registry so no project path is named. Without any, the test skips with a warning. Findings: docs/notes/m5-spike-report.md. The production path is FGSTN_PreviewPose,
// which does the same calls; this test keeps the raw engine calls so a change of the engine's behaviour shows here first.

namespace GSTNSpikeS1
{
	struct FProbe
	{
		TStrongObjectPtr<UDebugSkelMeshComponent> Component;
		TArray<FName> Bones;
	};

	static FVector SampleBone(UDebugSkelMeshComponent& Component, FName Bone)
	{
		return Component.GetSocketTransform(Bone, RTS_Component).GetLocation();
	}

	/**
	 * Component-space location of a bone read from the raw sequence of the montage's first slot track at a montage position: the parent chain
	 * of local bone transforms, with or without the root bone's own transform (root motion is the root bone's track).
	 */
	static bool GroundTruth(const UAnimMontage& Montage, const USkeletalMesh& Mesh, int32 MeshBone, float Position, bool bIncludeRoot, FVector& OutLocation)
	{
		if (Montage.SlotAnimTracks.Num() == 0)
		{
			return false;
		}
		const FAnimTrack& Track = Montage.SlotAnimTracks[0].AnimTrack;
		const FAnimSegment* Segment = Track.GetSegmentAtTime(Position);
		const UAnimSequence* Sequence = Segment ? Cast<UAnimSequence>(Segment->GetAnimReference()) : nullptr;
		if (Sequence == nullptr)
		{
			return false;
		}
		const float AnimPosition = Segment->ConvertTrackPosToAnimPos(Position);
		const FReferenceSkeleton& Ref = Mesh.GetRefSkeleton();
		FTransform Component = FTransform::Identity;
		for (int32 Bone = MeshBone; Bone != INDEX_NONE; Bone = Ref.GetParentIndex(Bone))
		{
			if (Bone == 0 && !bIncludeRoot)
			{
				break;
			}
			FTransform Local = Ref.GetRefBonePose()[Bone];
			const int32 SkeletonBone = const_cast<USkeleton*>(Mesh.GetSkeleton())->GetSkeletonBoneIndexFromMeshBoneIndex(&Mesh, Bone);
			if (SkeletonBone != INDEX_NONE)
			{
				Sequence->GetBoneTransform(Local, FSkeletonPoseBoneIndex(SkeletonBone), FAnimExtractContext(static_cast<double>(AnimPosition), false), false);
			}
			Component = Component * Local;
		}
		OutLocation = Component.GetLocation();
		return true;
	}

	/** Position of the montage the way the preview sets it: section start plus the frame time at the play rate, no ticking of the clock */
	static void SetMontagePosition(UDebugSkelMeshComponent& Component, float Position)
	{
		Component.PreviewInstance->MontagePreview_JumpToPosition(Position);
		Component.TickAnimation(0.f, false);
		Component.RefreshBoneTransforms();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNSpikeMontageSampling, "GSTNCombat.Editor.Preview.Spike.MontageSampling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTNSpikeMontageSampling::RunTest(const FString& Parameters)
{
	// Sample content by discovery, no path named: the first montage with a mesh to play on and some length, one with root motion and one without
	struct FCase
	{
		const TCHAR* Label;
		USkeletalMesh* Mesh = nullptr;
		UAnimMontage* Montage = nullptr;
	};
	FCase Cases[2] = { { TEXT("with root motion") }, { TEXT("without root motion") } };
	{
		TArray<FAssetData> Montages;
		IAssetRegistry::GetChecked().GetAssetsByClass(UAnimMontage::StaticClass()->GetClassPathName(), Montages, true);
		Montages.Sort([](const FAssetData& A, const FAssetData& B) { return A.GetSoftObjectPath().LexicalLess(B.GetSoftObjectPath()); });
		for (const FAssetData& Data : Montages)
		{
			if (Cases[0].Montage != nullptr && Cases[1].Montage != nullptr)
			{
				break;
			}
			UAnimMontage* Candidate = Cast<UAnimMontage>(Data.GetAsset());
			USkeletalMesh* Preview = Candidate && Candidate->GetSkeleton() ? Candidate->GetSkeleton()->GetPreviewMesh(true) : nullptr;
			if (Candidate == nullptr || Preview == nullptr || Candidate->GetPlayLength() < 1.f)
			{
				continue;
			}
			FCase& Slot = Candidate->HasRootMotion() ? Cases[0] : Cases[1];
			if (Slot.Montage == nullptr)
			{
				Slot.Montage = Candidate;
				Slot.Mesh = Preview;
			}
		}
	}

	for (const FCase& Case : Cases)
	{
		USkeletalMesh* Mesh = Case.Mesh;
		UAnimMontage* Montage = Case.Montage;
		if (Mesh == nullptr || Montage == nullptr)
		{
			AddWarning(FString::Printf(TEXT("Skipped '%s': no montage with a preview mesh and a second of length in the project's content"), Case.Label));
			continue;
		}
		const UAnimSequence* FirstSequence = Montage->SlotAnimTracks.Num() > 0 && Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.Num() > 0
			? Cast<UAnimSequence>(Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].GetAnimReference()) : nullptr;
		AddInfo(FString::Printf(TEXT("S1 sample '%s': montage %s on mesh %s; slot tracks=%d, first sequence additive=%d, blend-in %.2f s"), Case.Label, *Montage->GetPathName(), *Mesh->GetPathName(),
			Montage->SlotAnimTracks.Num(), FirstSequence != nullptr && FirstSequence->IsValidAdditive(), Montage->BlendIn.GetBlendTime()));

		TArray<FVector> RunWithPostProcess;
		for (const bool bPostProcess : { true, false })
		{
			FPreviewScene Scene;
			TStrongObjectPtr<UDebugSkelMeshComponent> Component(NewObject<UDebugSkelMeshComponent>(GetTransientPackage()));
			Component->SetSkeletalMesh(Mesh);
			Component->SetDisablePostProcessBlueprint(!bPostProcess);
			Scene.AddComponent(Component.Get(), FTransform::Identity);
			Component->EnablePreview(true, Montage);
			if (!TestNotNull(TEXT("preview instance"), Component->PreviewInstance.Get()))
			{
				return true;
			}
			Component->PreviewInstance->MontagePreview_PreviewNormal(INDEX_NONE, false);
			Component->PreviewInstance->MontagePreview_RemoveBlendOut();

			const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
			const int32 NumBones = RefSkeleton.GetNum();
			// Three bones with no name assumed: the last, one in the middle, one a quarter in
			TArray<FName> Bones;
			Bones.Add(RefSkeleton.GetBoneName(NumBones - 1));
			Bones.Add(RefSkeleton.GetBoneName(NumBones / 2));
			Bones.Add(RefSkeleton.GetBoneName(FMath::Max(1, NumBones / 4)));

			const float PlayRate = 1.f;
			const float TickRate = 62.5f;
			const int32 NumFrames = 60;
			const float Start = 0.f;

			auto Bake = [&](bool bBackward, TArray<FVector>& Out)
			{
				Out.Reset();
				Out.SetNum(NumFrames * Bones.Num());
				for (int32 Step = 0; Step < NumFrames; ++Step)
				{
					const int32 Frame = bBackward ? NumFrames - 1 - Step : Step;
					GSTNSpikeS1::SetMontagePosition(*Component, FMath::Min(Start + Frame / TickRate * PlayRate, Montage->GetPlayLength()));
					for (int32 B = 0; B < Bones.Num(); ++B)
					{
						Out[Frame * Bones.Num() + B] = GSTNSpikeS1::SampleBone(*Component, Bones[B]);
					}
				}
			};

			// Ground truth: the raw sequence under the montage, no anim instance at all. Two flavours, with the root bone's track and without it
			double GroundWithRoot = 0.0;
			double GroundWithoutRoot = 0.0;
			int32 GroundFrames = 0;
			for (int32 Frame = 0; Frame < NumFrames; ++Frame)
			{
				const float Position = FMath::Min(Start + Frame / TickRate * PlayRate, Montage->GetPlayLength());
				GSTNSpikeS1::SetMontagePosition(*Component, Position);
				for (const FName Bone : Bones)
				{
					FVector WithRoot, WithoutRoot;
					const int32 MeshBone = RefSkeleton.FindBoneIndex(Bone);
					if (GSTNSpikeS1::GroundTruth(*Montage, *Mesh, MeshBone, Position, true, WithRoot) && GSTNSpikeS1::GroundTruth(*Montage, *Mesh, MeshBone, Position, false, WithoutRoot))
					{
						const FVector Pose = GSTNSpikeS1::SampleBone(*Component, Bone);
						GroundWithRoot = FMath::Max(GroundWithRoot, (Pose - WithRoot).Size());
						GroundWithoutRoot = FMath::Max(GroundWithoutRoot, (Pose - WithoutRoot).Size());
						++GroundFrames;
					}
				}
			}

			TArray<FVector> Run1, Run2, Run3;
			const double T0 = FPlatformTime::Seconds();
			Bake(false, Run1);
			const double T1 = FPlatformTime::Seconds();
			Bake(true, Run2);
			Bake(false, Run3);

			double MaxRepeat = 0.0;
			double MaxBackward = 0.0;
			double MaxMotion = 0.0;
			for (int32 I = 0; I < Run1.Num(); ++I)
			{
				MaxRepeat = FMath::Max(MaxRepeat, (Run1[I] - Run3[I]).Size());
				MaxBackward = FMath::Max(MaxBackward, (Run1[I] - Run2[I]).Size());
				MaxMotion = FMath::Max(MaxMotion, (Run1[I] - Run1[I % Bones.Num()]).Size());
			}

			if (bPostProcess)
			{
				RunWithPostProcess = Run1;
			}
			else if (RunWithPostProcess.Num() == Run1.Num())
			{
				double PostProcessDelta = 0.0;
				for (int32 I = 0; I < Run1.Num(); ++I)
				{
					PostProcessDelta = FMath::Max(PostProcessDelta, (Run1[I] - RunWithPostProcess[I]).Size());
				}
				AddInfo(FString::Printf(TEXT("S1 '%s': max difference of the tracked bones with the post process graph on vs off = %.4g cm"), Case.Label, PostProcessDelta));
			}

			// Root bone (index 0) over the frames: the extracted root motion should not be in the evaluated pose
			double RootTravel = 0.0;
			double ChildTravel = 0.0;
			{
				const FName Root = RefSkeleton.GetBoneName(0);
				GSTNSpikeS1::SetMontagePosition(*Component, 0.f);
				const FName Child = RefSkeleton.GetBoneName(FMath::Min(1, NumBones - 1));
				const FVector First = GSTNSpikeS1::SampleBone(*Component, Root);
				const FVector FirstChild = GSTNSpikeS1::SampleBone(*Component, Child);
				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					GSTNSpikeS1::SetMontagePosition(*Component, FMath::Min(Frame / TickRate, Montage->GetPlayLength()));
					RootTravel = FMath::Max(RootTravel, (GSTNSpikeS1::SampleBone(*Component, Root) - First).Size());
					ChildTravel = FMath::Max(ChildTravel, (GSTNSpikeS1::SampleBone(*Component, Child) - FirstChild).Size());
				}
				AddInfo(FString::Printf(TEXT("S1 '%s' pp=%d: extraction ON: bone0 (%s) travel=%.4g cm, bone1 (%s) travel=%.4g cm"), Case.Label, bPostProcess, *Root.ToString(), RootTravel, *Child.ToString(), ChildTravel));
			}

			// Root motion on (the montage's default: extracted, so the pose has the root locked) and off (the root bone's track stays in the pose)
			double RootTravelNoExtraction = 0.0;
			double PelvisTravelNoExtraction = 0.0;
			{
				Component->PreviewInstance->SetRootMotionMode(ERootMotionMode::NoRootMotionExtraction);
				const FName Root = RefSkeleton.GetBoneName(0);
				const FName Child = RefSkeleton.GetBoneName(FMath::Min(1, NumBones - 1));
				GSTNSpikeS1::SetMontagePosition(*Component, 0.f);
				const FVector FirstRoot = GSTNSpikeS1::SampleBone(*Component, Root);
				const FVector FirstChild = GSTNSpikeS1::SampleBone(*Component, Child);
				for (int32 Frame = 0; Frame < NumFrames; ++Frame)
				{
					GSTNSpikeS1::SetMontagePosition(*Component, FMath::Min(Frame / TickRate, Montage->GetPlayLength()));
					RootTravelNoExtraction = FMath::Max(RootTravelNoExtraction, (GSTNSpikeS1::SampleBone(*Component, Root) - FirstRoot).Size());
					PelvisTravelNoExtraction = FMath::Max(PelvisTravelNoExtraction, (GSTNSpikeS1::SampleBone(*Component, Child) - FirstChild).Size());
				}
				Component->PreviewInstance->SetRootMotionMode(ERootMotionMode::RootMotionFromMontagesOnly);
			}

			AddInfo(FString::Printf(TEXT("S1 raw-sequence ground truth '%s' pp=%d: %d samples, max |pose - truth| with root track=%.4g cm, without root track=%.4g cm; "
				"root motion extraction off: bone 0 travel=%.4g cm, bone 1 travel=%.4g cm"),
				Case.Label, bPostProcess, GroundFrames, GroundWithRoot, GroundWithoutRoot, RootTravelNoExtraction, PelvisTravelNoExtraction));
			AddInfo(FString::Printf(TEXT("S1 '%s' postprocess=%d: montage %.3fs rootmotion=%d bones=%d; repeat(run1 vs run3)=%.6g cm, backward(run1 vs run2)=%.6g cm, "
				"pose travel of the tracked bones=%.3g cm, travel of bone 0 over 60 frames=%.3g cm, time per 60 frames x 3 sockets=%.2f ms, post process instance=%d"),
				Case.Label, bPostProcess, Montage->GetPlayLength(), Montage->HasRootMotion(), NumBones, MaxRepeat, MaxBackward, MaxMotion, RootTravel,
				(T1 - T0) * 1000.0, Component->GetPostProcessInstance() != nullptr));

			TestTrue(*FString::Printf(TEXT("'%s' pp=%d repeats to 1e-4 cm"), Case.Label, bPostProcess), MaxRepeat < 1e-4);
			TestTrue(*FString::Printf(TEXT("'%s' pp=%d forward equals backward to 1e-4 cm"), Case.Label, bPostProcess), MaxBackward < 1e-4);

			Component->UnregisterComponent();
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
