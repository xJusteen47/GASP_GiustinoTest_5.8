// Automation tests of the partner window and the predicted partner presentation rules (FP G1): GSTNLink.NP.PartnerWindow.*. No world: the pure
// builder on fake fragments and an action table, and the pure playback rules (positions, lead, end, drop, suppression).

#include "GSTN_LinkNPTestFixture.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Actions/GSTN_LinkActionProvider.h"
#include "Sim/GSTN_LinkPartnerWindow.h"
#include "Timeline/GSTN_TimelineTypes.h"

// One namespace per test file: the unity build merges the files of this module, so their helpers must not share a scope
namespace GSTNLinkNPTests_PartnerWindow
{

using namespace GSTNLinkNPTest;

namespace
{
	constexpr int32 PartnerEntity = 42;
	constexpr uint8 Serial = 7;
	constexpr int32 KeyFrame = 300;
	constexpr int32 TimelineLength = 40;

	constexpr int16 MoveOffset0 = 1;
	constexpr int16 MoveOffset1 = 2;
	constexpr int16 MoveOffset12 = 3;
	constexpr int16 MoveLimited = 4;
	constexpr int16 MoveOpen = 5;
	constexpr int16 MoveNoAbility = 6;
	constexpr int16 MoveUnknown = 99;

	/** The fixture's test latch with an action table: three start offsets, a move with a duration limit, one with neither length, one that plays nothing */
	struct FScopedWindowMoves
	{
		FScopedLinkTypes Types;
		UGSTN_LinkActionTable* Table = nullptr;

		FScopedWindowMoves()
		{
			Table = NewObject<UGSTN_LinkActionTable>(GetTransientPackage());
			Table->AddToRoot();
			const int32 Offsets[] = { 0, 1, 12 };
			const int16 Ids[] = { MoveOffset0, MoveOffset1, MoveOffset12 };
			for (int32 Index = 0; Index < 3; ++Index)
			{
				FGSTN_LinkActionInfo Info;
				Info.PartnerAbility = UObject::StaticClass();
				Info.PartnerStartOffsetFrames = Offsets[Index];
				Table->SetAction(Ids[Index], Info);
			}
			FGSTN_LinkActionInfo Limited;
			Limited.PartnerAbility = UObject::StaticClass();
			Limited.MaxDurationFrames = 20;
			Table->SetAction(MoveLimited, Limited);
			FGSTN_LinkActionInfo Open;
			Open.PartnerAbility = UObject::StaticClass();
			Table->SetAction(MoveOpen, Open);
			FGSTN_LinkActionInfo Nothing;
			Nothing.PartnerStartOffsetFrames = 3;
			Nothing.MaxDurationFrames = 9;
			Table->SetAction(MoveNoAbility, Nothing);
			UGSTN_LinkRegistry::Get()->RegisterActionProvider(TestLatch, Table);
		}

		~FScopedWindowMoves()
		{
			UGSTN_LinkRegistry::Get()->UnregisterActionProvider(TestLatch);
			Table->RemoveFromRoot();
		}

		/** The initiator's fragment with the move recorded as StartPartnerMove records it */
		FGSTN_LinkFragment Initiator(int16 Id, uint8 Side = GSTNLinkActionSide::Initiator) const
		{
			FGSTN_LinkFragment Fragment = MakeLinked(Types.LatchIndex, 1, PartnerEntity, Serial, 100);
			Fragment.Actions.Active.Id = Id;
			Fragment.Actions.Active.StartFrame = KeyFrame;
			Fragment.Actions.Active.Side = Side;
			return Fragment;
		}
	};

	const FGSTN_CombatTimelineData* FixedTimeline(const UClass*)
	{
		static FGSTN_CombatTimelineData Data;
		Data.TotalFrames = TimelineLength;
		return &Data;
	}

	const FGSTN_CombatTimelineData* NoTimeline(const UClass*) { return nullptr; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPartnerWindowFramesTest, "GSTNLink.NP.PartnerWindow.StartAndEndFrames", Flags)
bool FGSTN_LinkPartnerWindowFramesTest::RunTest(const FString& Parameters)
{
	FScopedWindowMoves Scope;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	const int16 Ids[] = { MoveOffset0, MoveOffset1, MoveOffset12 };
	const int32 Offsets[] = { 0, 1, 12 };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FGSTN_LinkPartnerWindow Window;
		TestTrue(TEXT("the window builds"), FGSTN_LinkPartnerWindow::BuildFromInitiator(Scope.Initiator(Ids[Index]), Registry, &FixedTimeline, Window));
		TestTrue(TEXT("valid"), Window.bValid);
		// The same clamp as StartPartnerMove: offset 0 starts one frame after the key, like 1
		const int32 ExpectedStart = KeyFrame + FMath::Max(Offsets[Index], 1);
		TestEqual(TEXT("start = S + max(offset, 1)"), Window.PartnerStartFrame, ExpectedStart);
		TestEqual(TEXT("end = start + the timeline's TotalFrames"), Window.PartnerEndFrame, ExpectedStart + TimelineLength);
		TestTrue(TEXT("the length is known"), Window.bLengthKnown);
		TestEqual(TEXT("S"), Window.InitiatorFrame, KeyFrame);
		TestEqual(TEXT("id"), static_cast<int32>(Window.MoveId), static_cast<int32>(Ids[Index]));
		TestEqual(TEXT("serial of the link"), static_cast<int32>(Window.Serial), static_cast<int32>(Serial));
		TestTrue(TEXT("the partner's ability is the action's"), Window.PartnerAbility == UObject::StaticClass());

		TestFalse(TEXT("not inside before the start"), Window.IsInside(ExpectedStart - 1));
		TestTrue(TEXT("inside at the start"), Window.IsInside(ExpectedStart));
		TestTrue(TEXT("inside on the last frame"), Window.IsInside(ExpectedStart + TimelineLength - 1));
		TestFalse(TEXT("end exclusive"), Window.IsInside(ExpectedStart + TimelineLength));
		TestEqual(TEXT("timeline frame 0 at the start"), Window.TimelineFrameAt(ExpectedStart), 0);
		TestEqual(TEXT("timeline frame before the start is negative"), Window.TimelineFrameAt(ExpectedStart - 2), -2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPartnerWindowLengthTest, "GSTNLink.NP.PartnerWindow.MaxDurationFallback", Flags)
bool FGSTN_LinkPartnerWindowLengthTest::RunTest(const FString& Parameters)
{
	FScopedWindowMoves Scope;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();

	FGSTN_LinkPartnerWindow Limited;
	TestTrue(TEXT("a move whose timeline is unknown builds"), FGSTN_LinkPartnerWindow::BuildFromInitiator(Scope.Initiator(MoveLimited), Registry, &NoTimeline, Limited));
	TestTrue(TEXT("the action's limit is the length"), Limited.bLengthKnown);
	TestEqual(TEXT("end = start + MaxDurationFrames"), Limited.PartnerEndFrame, KeyFrame + 1 + 20);

	FGSTN_LinkPartnerWindow Known;
	FGSTN_LinkPartnerWindow::BuildFromInitiator(Scope.Initiator(MoveLimited), Registry, &FixedTimeline, Known);
	TestEqual(TEXT("a known timeline wins over the limit"), Known.PartnerEndFrame, KeyFrame + 1 + TimelineLength);

	FGSTN_LinkPartnerWindow Open;
	TestTrue(TEXT("neither length: still builds"), FGSTN_LinkPartnerWindow::BuildFromInitiator(Scope.Initiator(MoveOpen), Registry, &NoTimeline, Open));
	TestFalse(TEXT("but the length is unknown"), Open.bLengthKnown);
	TestEqual(TEXT("end at start"), Open.PartnerEndFrame, Open.PartnerStartFrame);
	TestTrue(TEXT("an open window is inside from its start on"), Open.IsInside(Open.PartnerStartFrame + 500));

	FGSTN_LinkPartnerWindow Nothing;
	TestTrue(TEXT("an action that plays nothing on the partner builds"), FGSTN_LinkPartnerWindow::BuildFromInitiator(Scope.Initiator(MoveNoAbility), Registry, &FixedTimeline, Nothing));
	TestTrue(TEXT("no ability"), Nothing.PartnerAbility == nullptr);
	TestEqual(TEXT("the limit is the length, offset 3"), Nothing.PartnerEndFrame, KeyFrame + 3 + 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPartnerWindowInvalidTest, "GSTNLink.NP.PartnerWindow.InvalidInputs", Flags)
bool FGSTN_LinkPartnerWindowInvalidTest::RunTest(const FString& Parameters)
{
	FScopedWindowMoves Scope;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	FGSTN_LinkPartnerWindow Window;
	Window.bValid = true;

	TestFalse(TEXT("an id the provider does not know"), FGSTN_LinkPartnerWindow::BuildFromInitiator(Scope.Initiator(MoveUnknown), Registry, &FixedTimeline, Window));
	TestFalse(TEXT("and the output is invalid (reset)"), Window.bValid);
	TestFalse(TEXT("no record"), FGSTN_LinkPartnerWindow::BuildFromInitiator(Scope.Initiator(0), Registry, &FixedTimeline, Window));
	TestFalse(TEXT("the partner's side of a move is not an initiator window"),
		FGSTN_LinkPartnerWindow::BuildFromInitiator(Scope.Initiator(MoveOffset1, GSTNLinkActionSide::Partner), Registry, &FixedTimeline, Window));
	TestFalse(TEXT("no registry"), FGSTN_LinkPartnerWindow::BuildFromInitiator(Scope.Initiator(MoveOffset1), nullptr, &FixedTimeline, Window));

	FGSTN_LinkFragment Pending = Scope.Initiator(MoveOffset1);
	Pending.Links.Body.Phase = EGSTN_LinkPhase::Pending;
	TestFalse(TEXT("a link that is not Active"), FGSTN_LinkPartnerWindow::BuildFromInitiator(Pending, Registry, &FixedTimeline, Window));
	FGSTN_LinkFragment NoRole = Scope.Initiator(MoveOffset1);
	NoRole.Links.Body.Role = 0;
	TestFalse(TEXT("no role"), FGSTN_LinkPartnerWindow::BuildFromInitiator(NoRole, Registry, &FixedTimeline, Window));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPartnerWindowSameMoveTest, "GSTNLink.NP.PartnerWindow.SameMoveIdentity", Flags)
bool FGSTN_LinkPartnerWindowSameMoveTest::RunTest(const FString& Parameters)
{
	FScopedWindowMoves Scope;
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	FGSTN_LinkPartnerWindow A, B, C;
	FGSTN_LinkPartnerWindow::BuildFromInitiator(Scope.Initiator(MoveOffset1), Registry, &FixedTimeline, A);
	FGSTN_LinkPartnerWindow::BuildFromInitiator(Scope.Initiator(MoveOffset1), Registry, &FixedTimeline, B);
	FGSTN_LinkFragment Later = Scope.Initiator(MoveOffset1);
	Later.Actions.Active.StartFrame = KeyFrame + 90;
	FGSTN_LinkPartnerWindow::BuildFromInitiator(Later, Registry, &FixedTimeline, C);
	TestTrue(TEXT("a rebuild of the same record is the same move"), A.IsSameMove(B));
	TestFalse(TEXT("a later key of the same id is another window"), A.IsSameMove(C));
	TestFalse(TEXT("an invalid window is nothing"), A.IsSameMove(FGSTN_LinkPartnerWindow()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LinkPartnerPlaybackTest, "GSTNLink.NP.PartnerWindow.PlaybackRules", Flags)
bool FGSTN_LinkPartnerPlaybackTest::RunTest(const FString& Parameters)
{
	using Playback = FGSTN_LinkPartnerPlayback;
	constexpr float Step = 0.016f;

	// Position: section start + frames x step x rate, and nothing before the start
	TestEqual(TEXT("frame 0 shows the section start"), Playback::PredictedPosition(0, Step, 1.f, 0.5f), 0.5f);
	TestEqual(TEXT("a frame before the start shows the start"), Playback::PredictedPosition(-3, Step, 1.f, 0.5f), 0.5f);
	TestEqual(TEXT("10 frames at rate 2"), Playback::PredictedPosition(10, Step, 2.f, 0.f), 10.f * Step * 2.f);

	// Lead: the instance shows the later of its own and the predicted position
	TestEqual(TEXT("predicted ahead: predicted"), Playback::LeadPosition(0.2f, 0.5f), 0.5f);
	TestEqual(TEXT("instance ahead (the proxy overtook): the instance"), Playback::LeadPosition(0.7f, 0.5f), 0.7f);
	TestTrue(TEXT("behind by more than epsilon: write"), Playback::ShouldWriteLead(0.2f, 0.5f, 0.01f));
	TestFalse(TEXT("inside epsilon: left alone"), Playback::ShouldWriteLead(0.495f, 0.5f, 0.01f));
	TestFalse(TEXT("ahead: never written back"), Playback::ShouldWriteLead(0.9f, 0.5f, 0.01f));

	// End: at or past the montage's (section's) end
	TestFalse(TEXT("before the end"), Playback::HasReachedEnd(0.99f, 1.f));
	TestTrue(TEXT("at the end"), Playback::HasReachedEnd(1.f, 1.f));
	TestTrue(TEXT("past the end"), Playback::HasReachedEnd(1.4f, 1.f));

	// Drop: lost before the partner began, or an abort; a loss after the start is a normal outcome
	FGSTN_LinkPartnerWindow Window;
	Window.bValid = true;
	Window.PartnerStartFrame = 100;
	Window.PartnerEndFrame = 140;
	Window.bLengthKnown = true;
	TestTrue(TEXT("lost before the start: dropped"), Playback::IsDroppedOnRecordLoss(Window, 99, false));
	TestFalse(TEXT("lost at the start: the partner's part plays on"), Playback::IsDroppedOnRecordLoss(Window, 100, false));
	TestFalse(TEXT("lost mid-move (an outcome): plays on"), Playback::IsDroppedOnRecordLoss(Window, 120, false));
	TestTrue(TEXT("an abort mid-move: dropped"), Playback::IsDroppedOnRecordLoss(Window, 120, true));

	// Suppression of the late synced copy
	TestFalse(TEXT("just ended: held"), Playback::IsSuppressionOver(0, true, false));
	TestFalse(TEXT("seen and finished but inside the minimum hold: held"), Playback::IsSuppressionOver(Playback::MinSuppressFrames - 1, true, false));
	TestTrue(TEXT("seen, finished, minimum hold passed: over"), Playback::IsSuppressionOver(Playback::MinSuppressFrames, true, false));
	TestFalse(TEXT("still playing on the partner's screen: held"), Playback::IsSuppressionOver(Playback::MinSuppressFrames + 10, true, true));
	TestFalse(TEXT("never seen yet (the proxy is behind): held"), Playback::IsSuppressionOver(Playback::MinSuppressFrames + 10, false, false));
	TestTrue(TEXT("never seen by the maximum hold (refused on the server): over"), Playback::IsSuppressionOver(Playback::MaxSuppressFrames, false, false));
	return true;
}

}

#endif
