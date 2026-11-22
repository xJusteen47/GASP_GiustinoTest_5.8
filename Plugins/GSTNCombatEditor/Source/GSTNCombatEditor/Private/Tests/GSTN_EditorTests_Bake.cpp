#include "Animation/AnimMontage.h"
#include "Bake/GSTN_BakeAll.h"
#include "Bake/GSTN_SocketBaker.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Engine/SkeletalMesh.h"
#include "GSTN_CombatEditorUtils.h"
#include "Misc/AutomationTest.h"
#include "Preview/GSTN_PreviewPose.h"
#include "Settings/GSTN_CombatEditorSettings.h"
#include "Preview/GSTN_TimelinePreviewScene.h"
#include "Tests/GSTN_PreviewTestHelpers.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineUndo.h"
#include "UObject/StrongObjectPtr.h"
#include "Validation/GSTN_CombatValidators.h"

#if WITH_DEV_AUTOMATION_TESTS

// Socket baking and staleness (plan m5 E8, combat-m5-editor 7). They use the engine's one-bone SkeletalCube and the in-memory montage of the
// preview tests (the bone slides along X at 300 cm/s), so the expected position of a frame can be computed by hand. No renderer is needed.

#define GSTN_BAKE_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

using namespace GSTNPreviewTest;

namespace GSTNBakeTest
{
	constexpr float TickRate = 62.5f;

	/** A cube, its montage and a timeline of Total frames with one BakedSocket hitbox on the cube's bone */
	struct FFixture
	{
		USkeletalMesh* Cube = nullptr;
		TStrongObjectPtr<UAnimSequence> Sequence;
		TStrongObjectPtr<UAnimMontage> MontageHolder;
		UAnimMontage* Montage = nullptr;
		FName Bone;
		FGSTN_CombatTimelineData Data;

		explicit FFixture(int32 Total = 40, FName Section = NAME_None, float PlayRate = 1.f)
		{
			Cube = LoadEngineCube();
			if (Cube == nullptr)
			{
				return;
			}
			Montage = MakeLinearMontage(*Cube, Sequence, MontageHolder);
			Bone = Cube->GetRefSkeleton().GetBoneName(0);
			Data.TotalFrames = Total;
			Data.Montage = TSoftObjectPtr<UAnimMontage>(Montage);
			Data.MontageSection = Section;
			Data.MontagePlayRate = PlayRate;
			AddHitbox(Bone);
		}

		bool IsReady() const { return Cube != nullptr && Montage != nullptr; }

		void AddHitbox(FName Socket)
		{
			FGSTN_TimelineTrack_Hitbox Track;
			Track.StartFrame = 5;
			Track.EndFrame = 15;
			FGSTN_HitboxSpec Spec;
			Spec.Space = EGSTN_HitboxSpace::BakedSocket;
			Spec.Socket = Socket;
			FGSTN_HitboxKey Key;
			Spec.Keys.Add(Key);
			Track.Hitboxes.Add(Spec);
			Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
		}

		FGSTN_BakeInputs Inputs(float Rate = TickRate) const
		{
			FGSTN_BakeInputs Result;
			Result.Montage = Montage;
			Result.Mesh = Cube;
			Result.TickRateHz = Rate;
			return Result;
		}

		/** Bakes with a scene of its own and applies the result */
		FGSTN_BakeResult BakeAndApply(float Rate = TickRate)
		{
			FGSTN_TimelinePreviewScene Scene;
			Scene.SetSubject(ResolvedFor(Cube));
			const FGSTN_BakeResult Result = FGSTN_SocketBaker::Bake(Data, Scene.GetPose(), Inputs(Rate));
			FGSTN_SocketBaker::Apply(Data, Result);
			return Result;
		}
	};

	/** The pose a fresh scene gives a frame, in root space: what the preview shows */
	bool PoseAt(const FFixture& Fixture, int32 Frame, FTransform& OutRoot)
	{
		FGSTN_TimelinePreviewScene Scene;
		Scene.SetSubject(ResolvedFor(Fixture.Cube));
		Scene.GetPose().SetMontage(Fixture.Montage, Fixture.Data.MontageSection, Fixture.Data.MontagePlayRate, TickRate);
		return Scene.GetPose().Evaluate(Frame) && Scene.GetPose().GetSocketRootSpace(Fixture.Bone, OutRoot);
	}
}

using namespace GSTNBakeTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBakeFrameMatchesPoseTest, "GSTNCombat.Editor.Bake.FrameAtFEqualsPoseAtFOverRate", GSTN_BAKE_TEST_FLAGS)
bool FGSTNBakeFrameMatchesPoseTest::RunTest(const FString& Parameters)
{
	FFixture Fixture(40, TEXT("Late"), 2.f);
	if (!TestTrue(TEXT("the cube and its montage are there"), Fixture.IsReady()))
	{
		return false;
	}
	const FGSTN_BakeResult Result = Fixture.BakeAndApply();
	if (!TestTrue(TEXT("the bake worked"), Result.bBaked))
	{
		return false;
	}
	const FGSTN_BakedSocketFrames* Baked = Fixture.Data.BakedSockets.Find(Fixture.Bone);
	if (!TestNotNull(TEXT("the socket has frames"), Baked))
	{
		return false;
	}

	// Backwards on purpose: a fresh scene, the last frame first. A frame's pose must not remember where the montage was before
	for (int32 Frame = Fixture.Data.TotalFrames - 1; Frame >= 0; --Frame)
	{
		FTransform Expected;
		if (!TestTrue(TEXT("the pose of the frame was read"), PoseAt(Fixture, Frame, Expected)))
		{
			return false;
		}
		TestTrue(*FString::Printf(TEXT("frame %d is the preview pose of the frame"), Frame), Baked->Frames[Frame].Equals(Expected, 1e-3f));
	}

	// And the montage position is the one the formula gives: the bone is at 300 cm/s x (SectionStart + Frame / 62.5 x PlayRate) along the mesh's own X
	FGSTN_TimelinePreviewScene Scene;
	Scene.SetSubject(ResolvedFor(Fixture.Cube));
	Scene.GetPose().SetMontage(Fixture.Montage, TEXT("Late"), 2.f, TickRate);
	for (const int32 Frame : { 0, 7, 25, 39 })
	{
		Scene.GetPose().Evaluate(Frame);
		FTransform InComponent;
		Scene.GetPose().GetSocketComponentSpace(Fixture.Bone, InComponent);
		const double Time = FMath::Min(1.0 + Frame / static_cast<double>(TickRate) * 2.0, static_cast<double>(Fixture.Montage->GetPlayLength()));
		TestEqual(*FString::Printf(TEXT("frame %d: component X is the montage at %.4f s"), Frame, Time), InComponent.GetLocation().X, LinearSpeed * Time, 0.05);
	}
	for (const FTransform& Frame : Baked->Frames)
	{
		TestTrue(TEXT("the baked scale is 1: the key carries the size"), Frame.GetScale3D().Equals(FVector::OneVector, 1e-6));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBakeFrameCountTest, "GSTNCombat.Editor.Bake.FrameCountEqualsTotalFrames", GSTN_BAKE_TEST_FLAGS)
bool FGSTNBakeFrameCountTest::RunTest(const FString& Parameters)
{
	for (const int32 Total : { 1, 7, 60 })
	{
		FFixture Fixture(Total);
		if (!TestTrue(TEXT("the cube and its montage are there"), Fixture.IsReady()))
		{
			return false;
		}
		const FGSTN_BakeResult Result = Fixture.BakeAndApply();
		TestTrue(TEXT("the bake worked"), Result.bBaked);
		TestEqual(TEXT("one transform per frame"), Fixture.Data.BakedSockets.FindChecked(Fixture.Bone).Frames.Num(), Total);
		TestEqual(TEXT("the result says so too"), Result.FrameCount, Total);
		TestEqual(TEXT("one socket"), Fixture.Data.BakedSockets.Num(), 1);
	}

	// No BakedSocket hitbox: an empty bake, which also clears the frames of an earlier one
	FFixture Fixture(10);
	Fixture.Data.Tracks.Reset();
	FGSTN_BakedSocketFrames Old;
	Old.Frames.Add(FTransform::Identity);
	Fixture.Data.BakedSockets.Add(TEXT("Old"), Old);
	const FGSTN_BakeResult Empty = Fixture.BakeAndApply();
	TestTrue(TEXT("nothing to bake is a successful bake"), Empty.bBaked);
	TestTrue(TEXT("the old frames are gone"), Fixture.Data.BakedSockets.IsEmpty());
	TestFalse(TEXT("and no record is kept of a bake of nothing"), Fixture.Data.BakeInfo.IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBakeBitIdenticalTest, "GSTNCombat.Editor.Bake.TwoBakesBitIdentical", GSTN_BAKE_TEST_FLAGS)
bool FGSTNBakeBitIdenticalTest::RunTest(const FString& Parameters)
{
	FFixture Fixture(50, TEXT("Late"), 1.5f);
	if (!TestTrue(TEXT("the cube and its montage are there"), Fixture.IsReady()))
	{
		return false;
	}
	const FGSTN_BakeResult First = Fixture.BakeAndApply();
	// A different timeline baked in between, with the same montage, and a second scene: no state may leak from one bake to the next
	FFixture Other(20);
	Other.BakeAndApply();
	const FGSTN_BakeResult Second = Fixture.BakeAndApply();

	if (!TestTrue(TEXT("both baked"), First.bBaked && Second.bBaked))
	{
		return false;
	}
	const TArray<FTransform>& A = First.Sockets.FindChecked(Fixture.Bone).Frames;
	const TArray<FTransform>& B = Second.Sockets.FindChecked(Fixture.Bone).Frames;
	if (!TestEqual(TEXT("same frame count"), A.Num(), B.Num()))
	{
		return false;
	}
	bool bIdentical = true;
	for (int32 Frame = 0; Frame < A.Num(); ++Frame)
	{
		// Exactly equal, not nearly: Equals with a zero tolerance compares the doubles
		bIdentical &= A[Frame].Equals(B[Frame], 0.0);
	}
	TestTrue(TEXT("every frame of the two bakes is bit-identical"), bIdentical);
	TestEqual(TEXT("the same inputs give the same hash"), First.Info.InputHash, Second.Info.InputHash);
	TestEqual(TEXT("and the hash is the one of the inputs"), First.Info.InputHash, GSTNBake::ComputeInputHash(Fixture.Data, Fixture.Inputs()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBakeStatusTest, "GSTNCombat.Editor.Bake.StatusTracksTheInputs", GSTN_BAKE_TEST_FLAGS)
bool FGSTNBakeStatusTest::RunTest(const FString& Parameters)
{
	FFixture Fixture(40);
	if (!TestTrue(TEXT("the cube and its montage are there"), Fixture.IsReady()))
	{
		return false;
	}
	TestTrue(TEXT("before any bake the frames are missing"), GSTNBake::GetStatus(Fixture.Data, Fixture.Inputs()).Status == EGSTN_BakeStatus::NeverBaked);
	Fixture.BakeAndApply();
	TestTrue(TEXT("after a bake the status is fresh"), GSTNBake::GetStatus(Fixture.Data, Fixture.Inputs()).Status == EGSTN_BakeStatus::Fresh);
	TestFalse(TEXT("fresh does not need a bake"), GSTNBake::GetStatus(Fixture.Data, Fixture.Inputs()).NeedsBake());

	// No hitbox on a baked socket: nothing to bake, whatever the old frames say
	{
		FGSTN_CombatTimelineData NoHitboxes = Fixture.Data;
		NoHitboxes.Tracks.Reset();
		TestTrue(TEXT("no baked-socket hitbox, no need"), GSTNBake::GetStatus(NoHitboxes, Fixture.Inputs()).Status == EGSTN_BakeStatus::NotNeeded);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBakeTickRateTest, "GSTNCombat.Editor.Bake.TickRateChangeMarksStale", GSTN_BAKE_TEST_FLAGS)
bool FGSTNBakeTickRateTest::RunTest(const FString& Parameters)
{
	FFixture Fixture(40);
	if (!TestTrue(TEXT("the cube and its montage are there"), Fixture.IsReady()))
	{
		return false;
	}
	Fixture.BakeAndApply();
	const FGSTN_BakeStatusResult Changed = GSTNBake::GetStatus(Fixture.Data, Fixture.Inputs(60.f));
	TestTrue(TEXT("a project tick rate of 60 Hz makes the 62.5 Hz bake stale"), Changed.Status == EGSTN_BakeStatus::Stale);
	TestFalse(TEXT("with a reason"), Changed.Reason.IsEmpty());
	TestTrue(TEXT("the original rate is still fresh"), GSTNBake::GetStatus(Fixture.Data, Fixture.Inputs(TickRate)).Status == EGSTN_BakeStatus::Fresh);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBakePlayRateTest, "GSTNCombat.Editor.Bake.PlayRateChangeMarksStale", GSTN_BAKE_TEST_FLAGS)
bool FGSTNBakePlayRateTest::RunTest(const FString& Parameters)
{
	FFixture Fixture(40, NAME_None, 1.f);
	if (!TestTrue(TEXT("the cube and its montage are there"), Fixture.IsReady()))
	{
		return false;
	}
	Fixture.BakeAndApply();

	FGSTN_CombatTimelineData Faster = Fixture.Data;
	Faster.MontagePlayRate = 1.25f;
	TestTrue(TEXT("a play rate of 1.25 makes the bake stale"), GSTNBake::GetStatus(Faster, Fixture.Inputs()).Status == EGSTN_BakeStatus::Stale);

	FGSTN_CombatTimelineData OtherSection = Fixture.Data;
	OtherSection.MontageSection = TEXT("Late");
	TestTrue(TEXT("another section makes it stale"), GSTNBake::GetStatus(OtherSection, Fixture.Inputs()).Status == EGSTN_BakeStatus::Stale);

	// The montage itself: the speed of the segment the section plays is part of the inputs, and so is the data of its sequence
	const float OriginalRate = Fixture.Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].AnimPlayRate;
	Fixture.Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].AnimPlayRate = OriginalRate * 0.5f;
	TestTrue(TEXT("a change in the montage segment makes it stale"), GSTNBake::GetStatus(Fixture.Data, Fixture.Inputs()).Status == EGSTN_BakeStatus::Stale);
	Fixture.Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0].AnimPlayRate = OriginalRate;
	TestTrue(TEXT("put back, it is fresh again"), GSTNBake::GetStatus(Fixture.Data, Fixture.Inputs()).Status == EGSTN_BakeStatus::Fresh);

	// Total frames: the frame count of the baked array no longer fits
	FGSTN_CombatTimelineData Longer = Fixture.Data;
	Longer.TotalFrames = 45;
	TestTrue(TEXT("a longer timeline makes it stale"), GSTNBake::GetStatus(Longer, Fixture.Inputs()).Status == EGSTN_BakeStatus::Stale);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBakeSocketListTest, "GSTNCombat.Editor.Bake.SocketListChangeMarksStale", GSTN_BAKE_TEST_FLAGS)
bool FGSTNBakeSocketListTest::RunTest(const FString& Parameters)
{
	FFixture Fixture(40);
	if (!TestTrue(TEXT("the cube and its montage are there"), Fixture.IsReady()))
	{
		return false;
	}
	Fixture.BakeAndApply();
	const uint32 HashOne = GSTNBake::ComputeInputHash(Fixture.Data, Fixture.Inputs());

	// A second hitbox on another socket whose frames exist (copied, so only the list changed): everything cheap matches and only the hash can tell
	Fixture.AddHitbox(TEXT("Other"));
	Fixture.Data.BakedSockets.Add(TEXT("Other"), Fixture.Data.BakedSockets.FindChecked(Fixture.Bone));
	TestNotEqual(TEXT("the socket list is part of the hash"), GSTNBake::ComputeInputHash(Fixture.Data, Fixture.Inputs()), HashOne);
	TestTrue(TEXT("a new socket in the list makes the bake stale"), GSTNBake::GetStatus(Fixture.Data, Fixture.Inputs()).Status == EGSTN_BakeStatus::Stale);

	// The order of the hitboxes does not matter, only which sockets: a duplicate of a socket already in the list changes nothing
	FFixture Same(40);
	Same.BakeAndApply();
	Same.AddHitbox(Same.Bone);
	TestTrue(TEXT("a second hitbox on the same socket leaves the bake fresh"), GSTNBake::GetStatus(Same.Data, Same.Inputs()).Status == EGSTN_BakeStatus::Fresh);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBakeNoRecordTest, "GSTNCombat.Editor.Bake.FramesFromAnotherToolAreBakedElsewhere", GSTN_BAKE_TEST_FLAGS)
bool FGSTNBakeNoRecordTest::RunTest(const FString& Parameters)
{
	FFixture Fixture(10);
	if (!TestTrue(TEXT("the cube and its montage are there"), Fixture.IsReady()))
	{
		return false;
	}
	FGSTN_BakedSocketFrames Frames;
	Frames.Frames.Init(FTransform::Identity, 10);
	Fixture.Data.BakedSockets.Add(Fixture.Bone, Frames);

	const FGSTN_BakeStatusResult Status = GSTNBake::GetStatus(Fixture.Data, Fixture.Inputs());
	TestTrue(TEXT("frames with no record are baked elsewhere"), Status.Status == EGSTN_BakeStatus::BakedElsewhere);
	TestFalse(TEXT("which is not a stale bake, the editor cannot tell"), Status.NeedsBake());

	// The validator: a warning, not an error, for frames the editor did not write; an error once the editor's own record disagrees
	FGSTN_TimelineRuleContext Context;
	Context.BakeStatus = Status;
	FGSTN_ValidationReport Report;
	FGSTN_CombatValidators::CheckBakeStatus(Fixture.Data, Context, Report);
	TestEqual(TEXT("one finding"), Report.Num(), 1);
	TestEqual(TEXT("a warning"), Report.CountSeverity(EGSTN_Severity::Warning), 1);
	TestTrue(TEXT("with the BakedStale code"), Report.HasCode(GSTNValidationCodes::BakedStale()));

	FGSTN_TimelineRuleContext StaleContext;
	FGSTN_BakeStatusResult Stale;
	Stale.Status = EGSTN_BakeStatus::Stale;
	Stale.Reason = INVTEXT("the montage changed");
	StaleContext.BakeStatus = Stale;
	FGSTN_ValidationReport StaleReport;
	FGSTN_CombatValidators::CheckBakeStatus(Fixture.Data, StaleContext, StaleReport);
	TestEqual(TEXT("a stale bake is an error (it blocks the cook)"), StaleReport.CountSeverity(EGSTN_Severity::Error), 1);

	FGSTN_ValidationReport Unchecked;
	FGSTN_CombatValidators::CheckBakeStatus(Fixture.Data, FGSTN_TimelineRuleContext(), Unchecked);
	TestTrue(TEXT("a context without a status checks nothing"), Unchecked.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBakeMissingSocketTest, "GSTNCombat.Editor.Bake.MissingSocketIsErrorKeepsPreviousBake", GSTN_BAKE_TEST_FLAGS)
bool FGSTNBakeMissingSocketTest::RunTest(const FString& Parameters)
{
	FFixture Fixture(30);
	if (!TestTrue(TEXT("the cube and its montage are there"), Fixture.IsReady()))
	{
		return false;
	}
	Fixture.BakeAndApply();
	const FGSTN_BakedSocketFrames Before = Fixture.Data.BakedSockets.FindChecked(Fixture.Bone);
	const uint32 HashBefore = Fixture.Data.BakeInfo.InputHash;

	Fixture.AddHitbox(TEXT("socket_that_does_not_exist"));
	const int32 BadTrack = Fixture.Data.Tracks.Num() - 1;
	FGSTN_TimelinePreviewScene Scene;
	Scene.SetSubject(ResolvedFor(Fixture.Cube));
	const FGSTN_BakeResult Result = FGSTN_SocketBaker::Bake(Fixture.Data, Scene.GetPose(), Fixture.Inputs());

	TestFalse(TEXT("the bake did not happen"), Result.bBaked);
	if (!TestEqual(TEXT("one problem"), Result.Problems.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("it names the track"), Result.Problems[0].TrackIndex, BadTrack);
	TestEqual(TEXT("it names the hitbox"), Result.Problems[0].HitboxIndex, 0);
	TestEqual(TEXT("it names the socket"), Result.Problems[0].Socket, FName(TEXT("socket_that_does_not_exist")));
	TestTrue(TEXT("and says it in words"), Result.Problems[0].Message.ToString().Contains(TEXT("socket_that_does_not_exist")));
	TestTrue(TEXT("the result holds no frames"), Result.Sockets.IsEmpty());

	TestFalse(TEXT("applying a failed bake does nothing"), FGSTN_SocketBaker::Apply(Fixture.Data, Result));
	TestEqual(TEXT("the previous frames are kept"), Fixture.Data.BakedSockets.FindChecked(Fixture.Bone).Frames.Num(), Before.Frames.Num());
	TestTrue(TEXT("exactly"), Fixture.Data.BakedSockets.FindChecked(Fixture.Bone).Frames[10].Equals(Before.Frames[10], 0.0));
	TestEqual(TEXT("and the previous record"), Fixture.Data.BakeInfo.InputHash, HashBefore);

	// No mesh at all, no length, a montage that does not load: each one a refusal with a reason
	FGSTN_BakeInputs NoMesh = Fixture.Inputs();
	NoMesh.Mesh = nullptr;
	FGSTN_PreviewPose Unbound;
	TestFalse(TEXT("without a mesh nothing is baked"), FGSTN_SocketBaker::Bake(Fixture.Data, Unbound, NoMesh).bBaked);

	FGSTN_CombatTimelineData NoLength = Fixture.Data;
	NoLength.Tracks.Reset();
	NoLength.TotalFrames = 0;
	Fixture.AddHitbox(Fixture.Bone);
	NoLength.Tracks.Add(Fixture.Data.Tracks.Last());
	TestFalse(TEXT("a timeline without length has no frames to bake"), FGSTN_SocketBaker::Bake(NoLength, Scene.GetPose(), Fixture.Inputs()).bBaked);

	FGSTN_BakeInputs NoMontage = Fixture.Inputs();
	NoMontage.Montage = nullptr;
	FGSTN_CombatTimelineData WithMontage = Fixture.Data;
	WithMontage.Tracks.Reset();
	WithMontage.Tracks.Add(Fixture.Data.Tracks.Last());
	TestFalse(TEXT("a montage that did not load is not baked as the reference pose"), FGSTN_SocketBaker::Bake(WithMontage, Scene.GetPose(), NoMontage).bBaked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBakeUndoTest, "GSTNCombat.Editor.Bake.OneUndoRestoresPreviousBake", GSTN_BAKE_TEST_FLAGS)
bool FGSTNBakeUndoTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the editor has a transaction buffer"), GEditor != nullptr && GEditor->Trans != nullptr))
	{
		return false;
	}
	FFixture Fixture(30);
	if (!TestTrue(TEXT("the cube and its montage are there"), Fixture.IsReady()))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
	Timeline->Data = Fixture.Data;
	FGSTN_TimelineEditorModel Model(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
	FGSTN_TimelineUndoClient UndoClient(Model);

	// The previous bake: a first one, applied through the model like the toolbar does
	FGSTN_TimelinePreviewScene Scene;
	Scene.SetSubject(ResolvedFor(Fixture.Cube));
	const FGSTN_BakeResult First = FGSTN_SocketBaker::Bake(Timeline->Data, Scene.GetPose(), Fixture.Inputs());
	Model.ApplyDataEdit([&First](FGSTN_CombatTimelineData& Data) { FGSTN_SocketBaker::Apply(Data, First); }, INVTEXT("Bake sockets"));
	const FGSTN_BakedSocketFrames FirstFrames = Timeline->Data.BakedSockets.FindChecked(Fixture.Bone);
	const FGSTN_BakeInfo FirstInfo = Timeline->Data.BakeInfo;
	const int32 StepsAfterFirst = GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount();

	// A different bake on top (another play rate): one step, and one undo gives the first back exactly
	Timeline->Data.MontagePlayRate = 2.f;
	const FGSTN_BakeResult Second = FGSTN_SocketBaker::Bake(Timeline->Data, Scene.GetPose(), Fixture.Inputs());
	Model.ApplyDataEdit([&Second](FGSTN_CombatTimelineData& Data) { FGSTN_SocketBaker::Apply(Data, Second); }, INVTEXT("Bake sockets"));
	TestEqual(TEXT("the second bake is one undo step"), GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount(), StepsAfterFirst + 1);
	TestNotEqual(TEXT("the second bake changed the record"), Timeline->Data.BakeInfo.InputHash, FirstInfo.InputHash);

	TestTrue(TEXT("one undo"), GEditor->UndoTransaction());
	const FGSTN_BakedSocketFrames& Restored = Timeline->Data.BakedSockets.FindChecked(Fixture.Bone);
	bool bSame = Restored.Frames.Num() == FirstFrames.Frames.Num();
	for (int32 Frame = 0; bSame && Frame < Restored.Frames.Num(); ++Frame)
	{
		bSame &= Restored.Frames[Frame].Equals(FirstFrames.Frames[Frame], 0.0);
	}
	TestTrue(TEXT("the undo restores the previous frames exactly"), bSame);
	TestEqual(TEXT("and the previous record"), Timeline->Data.BakeInfo.InputHash, FirstInfo.InputHash);

	// Leave the editor's history where it began
	while (GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() > StepsAfterFirst - 1 && GEditor->UndoTransaction())
	{
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNBakeStandaloneTest, "GSTNCombat.Editor.Bake.StandaloneMatchesPreviewBake", GSTN_BAKE_TEST_FLAGS)
bool FGSTNBakeStandaloneTest::RunTest(const FString& Parameters)
{
	FFixture Fixture(30, TEXT("Late"), 1.f);
	if (!TestTrue(TEXT("the cube and its montage are there"), Fixture.IsReady()))
	{
		return false;
	}
	const FGSTN_BakeResult ViaPreview = Fixture.BakeAndApply();
	const FGSTN_BakeResult Standalone = FGSTN_SocketBaker::BakeStandalone(Fixture.Data, Fixture.Cube, TickRate);
	if (!TestTrue(TEXT("both baked"), ViaPreview.bBaked && Standalone.bBaked))
	{
		AddInfo(Standalone.DescribeProblems().ToString());
		return false;
	}
	TestEqual(TEXT("the same record hash"), Standalone.Info.InputHash, ViaPreview.Info.InputHash);
	const TArray<FTransform>& A = ViaPreview.Sockets.FindChecked(Fixture.Bone).Frames;
	const TArray<FTransform>& B = Standalone.Sockets.FindChecked(Fixture.Bone).Frames;
	bool bSame = A.Num() == B.Num();
	for (int32 Frame = 0; bSame && Frame < A.Num(); ++Frame)
	{
		bSame &= A[Frame].Equals(B[Frame], 0.0);
	}
	TestTrue(TEXT("the standalone bake (Bake all, scripts) gives the frames of the editor bake"), bSame);

	// The Blueprint-callable surface goes through the same code
	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
	Timeline->Data = Fixture.Data;
	Timeline->Data.BakedSockets.Reset();
	Timeline->Data.BakeInfo = FGSTN_BakeInfo();
	TestTrue(TEXT("a timeline with no frames is stale"), UGSTN_CombatEditorUtils::IsBakeStale(Timeline.Get(), Fixture.Cube));
	FString Message;
	TestTrue(TEXT("BakeTimeline bakes"), UGSTN_CombatEditorUtils::BakeTimeline(Timeline.Get(), Fixture.Cube, Message));
	TestFalse(TEXT("and then the timeline is not stale"), UGSTN_CombatEditorUtils::IsBakeStale(Timeline.Get(), Fixture.Cube));
	TestTrue(TEXT("the tick rate is the settings one"), FMath::IsNearlyEqual(UGSTN_CombatEditorUtils::GetTickRateHz(), UGSTN_CombatEditorSettings::GetSimulationTickRate()));
	Timeline->Data.Tracks.Reset();
	Timeline->Data.Tracks.Add(Fixture.Data.Tracks[0]);
	Timeline->Data.Tracks[0].GetMutable<FGSTN_TimelineTrack_Hitbox>().Hitboxes[0].Socket = TEXT("nope");
	TestFalse(TEXT("BakeTimeline refuses a missing socket"), UGSTN_CombatEditorUtils::BakeTimeline(Timeline.Get(), Fixture.Cube, Message));
	TestTrue(TEXT("and says why"), Message.Contains(TEXT("nope")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
