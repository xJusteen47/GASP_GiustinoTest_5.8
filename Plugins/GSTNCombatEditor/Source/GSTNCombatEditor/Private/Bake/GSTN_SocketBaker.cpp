// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Bake/GSTN_SocketBaker.h"

#include "Animation/AnimMontage.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "GSTNCombatEditorModule.h"
#include "Misc/DateTime.h"
#include "Preview/GSTN_PreviewPose.h"
#include "Preview/GSTN_PreviewSubject.h"
#include "Preview/GSTN_TimelinePreviewScene.h"
#include "UObject/StrongObjectPtr.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

FText FGSTN_BakeResult::DescribeProblems() const
{
	FString Text;
	for (const FGSTN_BakeProblem& Problem : Problems)
	{
		if (!Text.IsEmpty())
		{
			Text += TEXT("\n");
		}
		Text += Problem.Message.ToString();
	}
	return FText::FromString(Text);
}

void FGSTN_SocketBaker::CollectUses(const FGSTN_CombatTimelineData& Data, TArray<FGSTN_BakeSocketUse>& OutUses)
{
	for (int32 TrackIndex = 0; TrackIndex < Data.Tracks.Num(); ++TrackIndex)
	{
		const FGSTN_TimelineTrack_Hitbox* Track = Data.Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack_Hitbox>();
		if (Track == nullptr)
		{
			continue;
		}
		for (int32 HitboxIndex = 0; HitboxIndex < Track->Hitboxes.Num(); ++HitboxIndex)
		{
			const FGSTN_HitboxSpec& Spec = Track->Hitboxes[HitboxIndex];
			if (Spec.Space == EGSTN_HitboxSpace::BakedSocket && !Spec.Socket.IsNone())
			{
				FGSTN_BakeSocketUse Use;
				Use.Socket = Spec.Socket;
				Use.TrackIndex = TrackIndex;
				Use.HitboxIndex = HitboxIndex;
				OutUses.Add(Use);
			}
		}
	}
}

TArray<FName> FGSTN_SocketBaker::GetDistinctSockets(TConstArrayView<FGSTN_BakeSocketUse> Uses)
{
	TSet<FName> Distinct;
	for (const FGSTN_BakeSocketUse& Use : Uses)
	{
		Distinct.Add(Use.Socket);
	}
	TArray<FName> Sorted = Distinct.Array();
	// Lexical, not by name index: the index depends on the order names were created in, which differs between two editors
	Sorted.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	return Sorted;
}

bool FGSTN_SocketBaker::MeshHasSocket(const USkeletalMesh& Mesh, FName SocketOrBone)
{
	if (SocketOrBone.IsNone())
	{
		return false;
	}
	if (Mesh.GetRefSkeleton().FindBoneIndex(SocketOrBone) != INDEX_NONE)
	{
		return true;
	}
	return Mesh.FindSocket(SocketOrBone) != nullptr;
}

FGSTN_BakeResult FGSTN_SocketBaker::Bake(const FGSTN_CombatTimelineData& Data, FGSTN_PreviewPose& Pose, const FGSTN_BakeInputs& Inputs)
{
	const double StartSeconds = FPlatformTime::Seconds();
	FGSTN_BakeResult Result;
	Result.FrameCount = FMath::Max(Data.TotalFrames, 0);

	// 1. The sockets the hitboxes ask for. None: the bake is an empty one and the frames of an earlier bake go
	TArray<FGSTN_BakeSocketUse> Uses;
	CollectUses(Data, Uses);
	const TArray<FName> Sockets = GetDistinctSockets(Uses);
	if (Sockets.IsEmpty())
	{
		Result.bBaked = true;
		Result.FrameCount = 0;
		return Result;
	}

	auto AddProblem = [&Result](const FText& Message, const FGSTN_BakeSocketUse* Use = nullptr)
	{
		FGSTN_BakeProblem Problem;
		Problem.Message = Message;
		if (Use != nullptr)
		{
			Problem.TrackIndex = Use->TrackIndex;
			Problem.HitboxIndex = Use->HitboxIndex;
			Problem.Socket = Use->Socket;
		}
		Result.Problems.Add(Problem);
	};

	// 2. What the bake needs. Every refusal names what is missing, and a refused bake writes nothing (the previous one stays)
	if (Inputs.Mesh == nullptr)
	{
		AddProblem(LOCTEXT("BakeNoMesh", "There is no preview mesh to read the sockets from. Choose a preview subject (a kit and skin, or a skeletal mesh) first."));
	}
	if (Data.TotalFrames < 1)
	{
		AddProblem(LOCTEXT("BakeNoLength", "The timeline has no length (Total Frames is 0), so there are no frames to bake."));
	}
	if (!Data.Montage.IsNull() && Inputs.Montage == nullptr)
	{
		AddProblem(FText::Format(LOCTEXT("BakeMontageNotLoaded", "The montage {0} could not be loaded, so the baked frames would not be its pose."), FText::FromString(Data.Montage.ToString())));
	}
	if (Inputs.Mesh != nullptr)
	{
		for (const FGSTN_BakeSocketUse& Use : Uses)
		{
			if (!MeshHasSocket(*Inputs.Mesh, Use.Socket))
			{
				AddProblem(FText::Format(LOCTEXT("BakeMissingSocket", "Track {0}, hitbox {1} uses the socket '{2}', which {3} does not have as a bone or a socket. The previous bake was kept."),
					FText::AsNumber(Use.TrackIndex), FText::AsNumber(Use.HitboxIndex), FText::FromName(Use.Socket), FText::FromString(Inputs.Mesh->GetName())), &Use);
			}
		}
	}
	if (!Result.Problems.IsEmpty())
	{
		return Result;
	}
	if (Data.Montage.IsNull())
	{
		Result.Warnings.Add(LOCTEXT("BakeNoMontageWarning", "The timeline has no montage: the sockets were baked from the reference pose."));
	}

	// 3. One evaluation per frame, every socket read from it. Component space to root space is the placement of the mesh in the scene, the same
	// transform the preview draws with
	Pose.SetMontage(Inputs.Montage, Data.MontageSection, Data.MontagePlayRate, Inputs.TickRateHz);
	for (const FName Socket : Sockets)
	{
		Result.Sockets.Add(Socket).Frames.Reserve(Data.TotalFrames);
	}
	for (int32 Frame = 0; Frame < Data.TotalFrames; ++Frame)
	{
		if (!Pose.Evaluate(Frame))
		{
			Result.Sockets.Reset();
			AddProblem(LOCTEXT("BakeNoPose", "The preview could not pose the mesh. Open the preview subject again."));
			return Result;
		}
		for (const FName Socket : Sockets)
		{
			FTransform InRoot;
			if (!Pose.GetSocketRootSpace(Socket, InRoot))
			{
				Result.Sockets.Reset();
				AddProblem(FText::Format(LOCTEXT("BakeSocketLost", "The socket '{0}' could not be read at frame {1}."), FText::FromName(Socket), FText::AsNumber(Frame)));
				return Result;
			}
			// The scale of a bone is not the scale of a hitbox: the key carries that. The runtime reads this transform as it is stored
			InRoot.SetScale3D(FVector::OneVector);
			InRoot.NormalizeRotation();
			Result.Sockets.FindChecked(Socket).Frames.Add(InRoot);
		}
	}

	Result.Info.InputHash = GSTNBake::ComputeInputHash(Data, Inputs);
	Result.Info.TickRateHz = Inputs.TickRateHz;
	Result.Info.Mesh = FSoftObjectPath(Inputs.Mesh);
	Result.Info.When = FDateTime::UtcNow();
	Result.bBaked = true;
	Result.ElapsedMs = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	return Result;
}

FGSTN_BakeResult FGSTN_SocketBaker::BakeStandalone(const FGSTN_CombatTimelineData& Data, USkeletalMesh* Mesh, float TickRateHz)
{
	TStrongObjectPtr<UAnimMontage> Montage;
	if (!Data.Montage.IsNull())
	{
		Montage.Reset(Data.Montage.LoadSynchronous());
	}

	FGSTN_BakeInputs Inputs;
	Inputs.Montage = Montage.Get();
	Inputs.Mesh = Mesh;
	Inputs.TickRateHz = TickRateHz;
	if (Mesh == nullptr)
	{
		FGSTN_PreviewPose Unbound;
		return Bake(Data, Unbound, Inputs);
	}

	FGSTN_TimelinePreviewScene Scene;
	FGSTN_ResolvedSubject Subject;
	Subject.Mesh = Mesh;
	Scene.SetSubject(Subject);
	return Bake(Data, Scene.GetPose(), Inputs);
}

bool FGSTN_SocketBaker::Apply(FGSTN_CombatTimelineData& Data, const FGSTN_BakeResult& Result)
{
	if (!Result.bBaked)
	{
		return false;
	}
	Data.BakedSockets = Result.Sockets;
	Data.BakeInfo = Result.Info;
	return true;
}

#undef LOCTEXT_NAMESPACE
