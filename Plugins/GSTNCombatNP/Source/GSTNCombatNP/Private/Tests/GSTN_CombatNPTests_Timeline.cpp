// Automation tests of the timeline part of the combat tick (combat-m3 7.2): the clock, the deferred commit, the startup override
// window, the cancel windows, the cancellable tail and the replacement of a running timeline. Pure: sync state in, sync state out.

#include "Input/GSTN_CombatCommandTags.h"
#include "Misc/AutomationTest.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** 30 frames, actionable from 20, override window of 2 frames, a cancel window [8, 14) into specials */
	FGSTN_CombatTimelineData MakeTimeline(int32 OverrideFrames = 2)
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 30;
		Data.FirstActionableFrame = 20;
		Data.OverrideFrames = OverrideFrames;

		FGSTN_TimelineTrack_CancelWindow Window;
		Window.StartFrame = 8;
		Window.EndFrame = 14;
		Window.IntoCommands = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNCommandTags::Special));
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_CancelWindow>(Window));
		return Data;
	}

	FGSTN_CombatSyncCore MakeRunning(int32 StartFrame, int32 Index = 3)
	{
		FGSTN_CombatSyncCore Core;
		FGSTN_CombatTimelineLogic::Start(Core, StartFrame, Index, EGSTN_ActivationSource::Command);
		return Core;
	}

	/** Runs the clock from the start frame (evaluated by EvaluateStart) to Last; returns the frame of the first CommitTimeline request, INDEX_NONE if none */
	int32 FirstCommitFrame(const FGSTN_CombatTimelineData& Data, int32 StartFrame, int32 Last, int32 Default = 1)
	{
		FGSTN_CombatSyncCore Core = MakeRunning(StartFrame);
		FGSTN_TimelineOutput Output;
		Output.Reserve(Data);
		FGSTN_CombatTimelineLogic::EvaluateStart(Core, StartFrame, Data, Default, Output);
		if (Core.Requests.Find(EGSTN_RequestType::CommitTimeline, StartFrame))
		{
			return StartFrame;
		}
		for (int32 Frame = StartFrame + 1; Frame <= Last; ++Frame)
		{
			Core.Requests.PruneBefore(Frame);
			FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, Default, true, Output);
			if (Core.Requests.Find(EGSTN_RequestType::CommitTimeline, Frame))
			{
				return Frame;
			}
		}
		return INDEX_NONE;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTimelineCommitDeferredTest, "GSTNCombat.NP.Timeline.CommitDeferred", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTimelineCommitDeferredTest::RunTest(const FString& Parameters)
{
	// The cost commits when the override window ends, once, and not before
	const FGSTN_CombatTimelineData Data = MakeTimeline(2);
	TestEqual(TEXT("commit at the end of a 2 frame window"), FirstCommitFrame(Data, 100, 140), 102);
	// Default of the tier (-1) uses what the caller passes
	TestEqual(TEXT("tier default of 1 frame"), FirstCommitFrame(MakeTimeline(-1), 100, 140, 1), 101);
	// No window: committed on the frame the timeline starts, in the same tick
	TestEqual(TEXT("no window commits on the start frame"), FirstCommitFrame(MakeTimeline(0), 100, 140), 100);

	// A request is made once: running past the commit frame adds nothing
	FGSTN_CombatSyncCore Core = MakeRunning(100);
	FGSTN_TimelineOutput Output;
	Output.Reserve(Data);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 100, Data, 1, Output);
	int32 Commits = 0;
	for (int32 Frame = 101; Frame < 125; ++Frame)
	{
		Core.Requests.PruneBefore(Frame);
		FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 1, true, Output);
		Commits += Core.Requests.Find(EGSTN_RequestType::CommitTimeline, Frame) ? 1 : 0;
	}
	TestEqual(TEXT("one commit in the whole run"), Commits, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTimelineClockTest, "GSTNCombat.NP.Timeline.Clock", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTimelineClockTest::RunTest(const FString& Parameters)
{
	const FGSTN_CombatTimelineData Data = MakeTimeline(2);
	FGSTN_CombatSyncCore Core = MakeRunning(100);
	FGSTN_TimelineOutput Output;
	Output.Reserve(Data);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 100, Data, 1, Output);

	// Running through the length of the timeline
	bool bRunning = true;
	int32 Frame = 101;
	for (; Frame < 140 && bRunning; ++Frame)
	{
		bRunning = FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 1, true, Output);
	}
	TestFalse(TEXT("the timeline ends"), bRunning);
	TestEqual(TEXT("at its 30th frame"), Frame - 1, 130);
	TestTrue(TEXT("back to Actionable with no timeline"), Core.ActionState == EGSTN_ActionState::Actionable && Core.ActiveTimeline == INDEX_NONE && Core.TimelineStartFrame == INDEX_NONE);

	// The ability going away ends it without waiting for the length, but not on the start frame (it is activated later in it)
	Core = MakeRunning(100);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 100, Data, 1, Output);
	TestTrue(TEXT("no ability yet on the start frame keeps it"), FGSTN_CombatTimelineLogic::AdvanceClock(Core, 100, &Data, 1, false, Output));
	TestTrue(TEXT("active ability keeps it"), FGSTN_CombatTimelineLogic::AdvanceClock(Core, 105, &Data, 1, true, Output));
	TestFalse(TEXT("a lost ability ends it"), FGSTN_CombatTimelineLogic::AdvanceClock(Core, 106, &Data, 1, false, Output));
	TestTrue(TEXT("and clears the state"), Core.ActionState == EGSTN_ActionState::Actionable && Core.ActiveTimeline == INDEX_NONE);

	// Missing data clears the state instead of running blind
	Core = MakeRunning(100);
	TestFalse(TEXT("no timeline data ends it"), FGSTN_CombatTimelineLogic::AdvanceClock(Core, 101, nullptr, 1, true, Output));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTimelineOverrideIntoTest, "GSTNCombat.NP.Timeline.OverrideInto", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTimelineOverrideIntoTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data = MakeTimeline(2);
	FGSTN_TimelineOutput Output;
	Output.Reserve(Data);
	const int32 AttackTier = 100, SpecialTier = 300;

	// Idle: everything is allowed
	{
		FGSTN_CombatSyncCore Idle;
		TestTrue(TEXT("idle allows"), FGSTN_CombatTimelineLogic::IsCommandAllowed(Idle, nullptr, Output, GSTNCommandTags::Attack_Neutral, AttackTier, 0));
	}

	FGSTN_CombatSyncCore Core = MakeRunning(100);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 100, Data, 1, Output);
	TestTrue(TEXT("frame 0 is inside the override window"), Output.bInOverrideWindow);

	// Default (empty OverrideInto): a higher tier replaces, an equal or lower one does not
	TestTrue(TEXT("a higher tier replaces"), FGSTN_CombatTimelineLogic::IsCommandAllowed(Core, &Data, Output, GSTNCommandTags::Special_Neutral, SpecialTier, AttackTier));
	TestFalse(TEXT("the same tier does not"), FGSTN_CombatTimelineLogic::IsCommandAllowed(Core, &Data, Output, GSTNCommandTags::Attack_Directional, AttackTier, AttackTier));

	// An explicit query: only what it names, whatever the tier
	Data.OverrideInto = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNCommandTags::Attack));
	TestTrue(TEXT("an explicit query takes a lower tier it names"), FGSTN_CombatTimelineLogic::IsCommandAllowed(Core, &Data, Output, GSTNCommandTags::Attack_Directional, AttackTier, SpecialTier));
	TestFalse(TEXT("and refuses one it does not name"), FGSTN_CombatTimelineLogic::IsCommandAllowed(Core, &Data, Output, GSTNCommandTags::Strong_Neutral, 200, AttackTier));

	// After the window (frame 5): busy, no cancel window open there -> nothing
	FGSTN_CombatTimelineLogic::AdvanceClock(Core, 105, &Data, 1, true, Output);
	TestFalse(TEXT("closed after the window"), FGSTN_CombatTimelineLogic::IsCommandAllowed(Core, &Data, Output, GSTNCommandTags::Special_Neutral, SpecialTier, AttackTier));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTimelineCancelWindowTest, "GSTNCombat.NP.Timeline.CancelWindow", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTimelineCancelWindowTest::RunTest(const FString& Parameters)
{
	const FGSTN_CombatTimelineData Data = MakeTimeline(0);
	FGSTN_TimelineOutput Output;
	Output.Reserve(Data);
	FGSTN_CombatSyncCore Core = MakeRunning(100);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 100, Data, 0, Output);

	auto Allowed = [&](int32 Frame, const FGameplayTag& Command)
	{
		FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 0, true, Output);
		return FGSTN_CombatTimelineLogic::IsCommandAllowed(Core, &Data, Output, Command, 100, 100);
	};

	TestFalse(TEXT("before the window"), Allowed(105, GSTNCommandTags::Special_Neutral));
	TestTrue(TEXT("window open [8, 14): frame 8"), Allowed(108, GSTNCommandTags::Special_Neutral));
	TestFalse(TEXT("window open: a command it does not name"), Allowed(110, GSTNCommandTags::Attack_Neutral));
	TestTrue(TEXT("window open: frame 13"), Allowed(113, GSTNCommandTags::Special_CrouchNeutral));
	TestFalse(TEXT("window closed at 14"), Allowed(114, GSTNCommandTags::Special_Neutral));
	// From the first actionable frame (20) everything is allowed
	TestFalse(TEXT("19 is still held"), Allowed(119, GSTNCommandTags::Attack_Neutral));
	TestTrue(TEXT("20 is free"), Allowed(120, GSTNCommandTags::Attack_Neutral));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTimelineReplaceTest, "GSTNCombat.NP.Timeline.Replace", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTimelineReplaceTest::RunTest(const FString& Parameters)
{
	// Starting a timeline over a running one asks for the old ability to be cancelled, and starts the new one on that frame
	FGSTN_CombatSyncCore Core = MakeRunning(100, 3);
	Core.Requests.PruneBefore(104);
	FGSTN_CombatTimelineLogic::Start(Core, 104, 7, EGSTN_ActivationSource::Command);

	const FGSTN_CombatRequest* Cancel = Core.Requests.Find(EGSTN_RequestType::CancelTimeline, 104);
	TestTrue(TEXT("the old entry is cancelled"), Cancel && Cancel->Arg == 3);
	TestEqual(TEXT("the new entry runs"), static_cast<int32>(Core.ActiveTimeline), 7);
	TestEqual(TEXT("from the replacing frame"), Core.TimelineStartFrame, 104);
	TestEqual(TEXT("paused frames start over"), Core.TimelinePausedFrames, 0);

	// A command that is not a timeline ability ends the running timeline: the entry is cancelled and the combatant is free
	FGSTN_CombatSyncCore Other = MakeRunning(300, 5);
	FGSTN_CombatTimelineLogic::CancelForOtherCommand(Other, 303);
	const FGSTN_CombatRequest* OtherCancel = Other.Requests.Find(EGSTN_RequestType::CancelTimeline, 303);
	TestTrue(TEXT("the running entry is cancelled by a plain ability"), OtherCancel && OtherCancel->Arg == 5);
	TestTrue(TEXT("and the combatant is Actionable"), Other.ActionState == EGSTN_ActionState::Actionable && Other.ActiveTimeline == INDEX_NONE && Other.TimelineStartFrame == INDEX_NONE);
	FGSTN_CombatSyncCore IdleOther;
	FGSTN_CombatTimelineLogic::CancelForOtherCommand(IdleOther, 10);
	TestTrue(TEXT("a plain ability from idle cancels nothing"), IdleOther.Requests.IsEmpty());

	// From idle there is nothing to cancel
	FGSTN_CombatSyncCore Idle;
	FGSTN_CombatTimelineLogic::Start(Idle, 10, 2, EGSTN_ActivationSource::Command);
	TestTrue(TEXT("nothing to cancel from idle"), Idle.Requests.IsEmpty());

	// The replaced timeline's own commit never happens: it had not reached it
	const FGSTN_CombatTimelineData Data = MakeTimeline(2);
	FGSTN_CombatSyncCore Old = MakeRunning(200, 1);
	FGSTN_TimelineOutput Output;
	Output.Reserve(Data);
	FGSTN_CombatTimelineLogic::EvaluateStart(Old, 200, Data, 1, Output);
	FGSTN_CombatTimelineLogic::AdvanceClock(Old, 201, &Data, 1, true, Output);
	TestTrue(TEXT("no commit request before the window ends"), !Old.Requests.Find(EGSTN_RequestType::CommitTimeline, 200) && !Old.Requests.Find(EGSTN_RequestType::CommitTimeline, 201));
	FGSTN_CombatTimelineLogic::Start(Old, 201, 4, EGSTN_ActivationSource::Command);
	FGSTN_CombatTimelineLogic::EvaluateStart(Old, 201, Data, 1, Output);
	TestTrue(TEXT("the replacement is not committed yet either"), !Old.Requests.Find(EGSTN_RequestType::CommitTimeline, 201));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTimelineTailTest, "GSTNCombat.NP.Timeline.TailCancel", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTimelineTailTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data = MakeTimeline(0);
	Data.FirstActionableFrame = 28;
	Data.Tail.bHasTail = true;
	Data.Tail.StartFrame = 15;
	FGSTN_TimelineOutput Output;
	Output.Reserve(Data);
	FGSTN_CombatSyncCore Core = MakeRunning(100);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 100, Data, 0, Output);

	auto Allowed = [&](int32 Frame)
	{
		FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 0, true, Output);
		return FGSTN_CombatTimelineLogic::IsCommandAllowed(Core, &Data, Output, GSTNCommandTags::Attack_Neutral, 100, 100);
	};
	TestFalse(TEXT("one frame before the tail nothing cancels"), Allowed(114));
	TestTrue(TEXT("the tail start cancels by anything"), Allowed(115));
	TestTrue(TEXT("and stays open"), Allowed(120));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTimelineAdoptTest, "GSTNCombat.NP.Timeline.Adopt", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTimelineAdoptTest::RunTest(const FString& Parameters)
{
	// Nobody waits: nothing happens
	FGSTN_CombatSyncCore Idle;
	TestEqual(TEXT("an empty list adopts nothing"), FGSTN_CombatTimelineLogic::Adopt(Idle, 50, TConstArrayView<int32>()), static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("and leaves the state alone"), Idle.ActionState == EGSTN_ActionState::Actionable && Idle.Requests.IsEmpty());

	// The first in registry order wins, with source Adopted and no ActivateEntry; the others are cancelled
	FGSTN_CombatSyncCore Core;
	const int32 Waiting[] = { 2, 5, 6 };
	TestEqual(TEXT("the first waiting entry is adopted"), FGSTN_CombatTimelineLogic::Adopt(Core, 50, Waiting), 2);
	TestTrue(TEXT("in a timeline"), Core.ActionState == EGSTN_ActionState::InTimeline);
	TestEqual(TEXT("of that entry"), static_cast<int32>(Core.ActiveTimeline), 2);
	TestEqual(TEXT("from this frame"), Core.TimelineStartFrame, 50);
	TestTrue(TEXT("with source Adopted"), Core.ActiveSource == EGSTN_ActivationSource::Adopted);
	TestTrue(TEXT("no entry is activated: the ability is active already"), !Core.Requests.Find(EGSTN_RequestType::ActivateEntry, 50));
	const FGSTN_CombatRequest* CancelFive = Core.Requests.Find(EGSTN_RequestType::CancelAbility, 50);
	TestTrue(TEXT("the losers are cancelled"), CancelFive && CancelFive->Arg == 5);
	int32 Cancels = 0;
	for (const FGSTN_CombatRequest& Slot : Core.Requests.Slots)
	{
		Cancels += (Slot.Type == EGSTN_RequestType::CancelAbility && Slot.Frame == 50) ? 1 : 0;
	}
	TestEqual(TEXT("two of them"), Cancels, 2);

	// A timeline that still runs is cancelled first: the newcomer was allowed to activate, so it wins
	FGSTN_CombatSyncCore Busy = MakeRunning(40, 1);
	Busy.Requests.PruneBefore(50);
	const int32 One[] = { 4 };
	FGSTN_CombatTimelineLogic::Adopt(Busy, 50, One);
	const FGSTN_CombatRequest* CancelOld = Busy.Requests.Find(EGSTN_RequestType::CancelTimeline, 50);
	TestTrue(TEXT("the running timeline is cancelled"), CancelOld && CancelOld->Arg == 1);
	TestEqual(TEXT("and the newcomer runs"), static_cast<int32>(Busy.ActiveTimeline), 4);

	// Deterministic: the same inputs give an equal state (a re-simulation of the adoption frame writes what the first run wrote)
	FGSTN_CombatSyncCore Again;
	FGSTN_CombatTimelineLogic::Adopt(Again, 50, Waiting);
	TestFalse(TEXT("re-adopting the same frame gives an equal state"), Again.ShouldReconcile(Core));

	// And the adopted timeline commits like any other: no override window (tier none), so on its first frame
	FGSTN_CombatTimelineData Data = MakeTimeline(0);
	FGSTN_TimelineOutput Output;
	Output.Reserve(Data);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 50, Data, 0, Output);
	TestTrue(TEXT("the cost commits on the adoption frame"), Core.Requests.Find(EGSTN_RequestType::CommitTimeline, 50) != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTimelineSameFrameCommitTest, "GSTNCombat.NP.Timeline.SameFrameCommit", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTimelineSameFrameCommitTest::RunTest(const FString& Parameters)
{
	// A (index 3, 2 frame window) reaches its commit on frame 102; the same frame B (index 4, no window) wins in the cancel window
	const FGSTN_CombatTimelineData DataA = MakeTimeline(2);
	const FGSTN_CombatTimelineData DataB = MakeTimeline(0);
	FGSTN_TimelineOutput Output;
	Output.Reserve(DataA);

	FGSTN_CombatSyncCore Core = MakeRunning(100, 3);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 100, DataA, 2, Output);
	Core.Requests.PruneBefore(101);
	FGSTN_CombatTimelineLogic::AdvanceClock(Core, 101, &DataA, 2, true, Output);
	Core.Requests.PruneBefore(102);
	FGSTN_CombatTimelineLogic::AdvanceClock(Core, 102, &DataA, 2, true, Output);
	TestTrue(TEXT("A asks for its commit on 102"), Core.Requests.Find(EGSTN_RequestType::CommitTimeline, 102, 3) != nullptr);

	FGSTN_CombatTimelineLogic::Start(Core, 102, 4, EGSTN_ActivationSource::Command);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 102, DataB, 0, Output);
	TestTrue(TEXT("the replaced timeline never commits"), Core.Requests.Find(EGSTN_RequestType::CommitTimeline, 102, 3) == nullptr);
	TestTrue(TEXT("the replacement commits on its first frame"), Core.Requests.Find(EGSTN_RequestType::CommitTimeline, 102, 4) != nullptr);
	TestTrue(TEXT("and A is cancelled"), Core.Requests.Find(EGSTN_RequestType::CancelTimeline, 102, 3) != nullptr);
	TestEqual(TEXT("the cancel and the commit only"), Core.Requests.Num(), 2);

	// The same with a cancel of the running timeline by a plain command: A's commit goes
	FGSTN_CombatSyncCore Plain = MakeRunning(100, 3);
	FGSTN_CombatTimelineLogic::EvaluateStart(Plain, 100, DataA, 2, Output);
	Plain.Requests.PruneBefore(102);
	FGSTN_CombatTimelineLogic::AdvanceClock(Plain, 102, &DataA, 2, true, Output);
	FGSTN_CombatTimelineLogic::CancelForOtherCommand(Plain, 102);
	TestTrue(TEXT("a plain command cancels without A's commit"), Plain.Requests.Find(EGSTN_RequestType::CommitTimeline, 102) == nullptr && Plain.Requests.Find(EGSTN_RequestType::CancelTimeline, 102, 3) != nullptr);

	// Adoption and a command in one frame stay inside the capacity: cancel + commit of the winner + cancels of two losers
	FGSTN_CombatSyncCore Busy = MakeRunning(100, 3);
	FGSTN_CombatTimelineLogic::EvaluateStart(Busy, 100, DataA, 2, Output);
	Busy.Requests.PruneBefore(102);
	FGSTN_CombatTimelineLogic::AdvanceClock(Busy, 102, &DataA, 2, true, Output);
	const int32 Waiting[] = { 4, 5, 6 };
	FGSTN_CombatTimelineLogic::Adopt(Busy, 102, Waiting);
	FGSTN_CombatTimelineLogic::EvaluateStart(Busy, 102, DataB, 0, Output);
	TestTrue(TEXT("the winner's commit is kept"), Busy.Requests.Find(EGSTN_RequestType::CommitTimeline, 102, 4) != nullptr);
	TestTrue(TEXT("and the old one's is gone"), Busy.Requests.Find(EGSTN_RequestType::CommitTimeline, 102, 3) == nullptr);
	TestEqual(TEXT("cancel of the old, commit, two loser cancels"), Busy.Requests.Num(), 4);

	// Remove packs the array so that equal sets stay equal arrays
	FGSTN_CombatRequests Requests;
	Requests.Add(EGSTN_RequestType::CancelTimeline, 10, 1);
	Requests.Add(EGSTN_RequestType::CommitTimeline, 10, 2);
	Requests.Add(EGSTN_RequestType::CancelAbility, 10, 3);
	TestTrue(TEXT("removes the matching request"), Requests.Remove(EGSTN_RequestType::CommitTimeline, 10, 2));
	TestFalse(TEXT("not twice"), Requests.Remove(EGSTN_RequestType::CommitTimeline, 10, 2));
	FGSTN_CombatRequests Expected;
	Expected.Add(EGSTN_RequestType::CancelTimeline, 10, 1);
	Expected.Add(EGSTN_RequestType::CancelAbility, 10, 3);
	TestTrue(TEXT("the rest is packed to the front"), Requests == Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTimelineAdoptSameClassTest, "GSTNCombat.NP.Timeline.AdoptSameClass", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTimelineAdoptSameClassTest::RunTest(const FString& Parameters)
{
	// The running timeline's own class is adopted again (a retrigger): the activation that is being adopted must not be cancelled
	FGSTN_CombatSyncCore Core = MakeRunning(40, 2);
	Core.Requests.PruneBefore(50);
	const int32 Same[] = { 2 };
	TestEqual(TEXT("adopted"), FGSTN_CombatTimelineLogic::Adopt(Core, 50, Same), 2);
	TestTrue(TEXT("no cancel of the entry that was just activated"), Core.Requests.Find(EGSTN_RequestType::CancelTimeline, 50) == nullptr);
	TestEqual(TEXT("the timeline starts over from this frame"), Core.TimelineStartFrame, 50);
	TestTrue(TEXT("with source Adopted"), Core.ActiveSource == EGSTN_ActivationSource::Adopted);
	TestTrue(TEXT("in a timeline"), Core.ActionState == EGSTN_ActionState::InTimeline);
	TestEqual(TEXT("with the pause count reset"), Core.TimelinePausedFrames, 0);

	// Another class still cancels the running one
	FGSTN_CombatSyncCore Other = MakeRunning(40, 2);
	Other.Requests.PruneBefore(50);
	const int32 Different[] = { 5 };
	FGSTN_CombatTimelineLogic::Adopt(Other, 50, Different);
	TestTrue(TEXT("a different class cancels the running one"), Other.Requests.Find(EGSTN_RequestType::CancelTimeline, 50, 2) != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTimelineHitstopTest, "GSTNCombat.NP.Timeline.Hitstop", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTimelineHitstopTest::RunTest(const FString& Parameters)
{
	// Window of 2 frames: the commit is at timeline frame 2. Started on 100, hitstop on frames 102 to 104 (three frames)
	const FGSTN_CombatTimelineData Data = MakeTimeline(2);
	FGSTN_TimelineOutput Output;
	Output.Reserve(Data);
	FGSTN_CombatSyncCore Core = MakeRunning(100);
	FGSTN_CombatTimelineLogic::EvaluateStart(Core, 100, Data, 1, Output);

	int32 CommitFrame = INDEX_NONE;
	int32 Commits = 0;
	for (int32 Frame = 101; Frame <= 110; ++Frame)
	{
		Core.Requests.PruneBefore(Frame);
		const bool bPaused = Frame >= 102 && Frame <= 104;
		FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 1, true, Output, bPaused);
		if (bPaused)
		{
			TestEqual(*FString::Printf(TEXT("the timeline frame stands still on %d"), Frame), Output.Frame, 1);
			TestTrue(*FString::Printf(TEXT("nothing is committed on %d"), Frame), !Core.Requests.Find(EGSTN_RequestType::CommitTimeline, Frame));
		}
		if (Core.Requests.Find(EGSTN_RequestType::CommitTimeline, Frame))
		{
			CommitFrame = Frame;
			++Commits;
		}
	}
	TestEqual(TEXT("the commit comes three frames later"), CommitFrame, 105);
	TestEqual(TEXT("and once"), Commits, 1);
	TestEqual(TEXT("the timeline counted its paused frames"), Core.TimelinePausedFrames, 3);

	// The end moves by the same amount: 30 frames from 100 plus 3 paused is 133
	FGSTN_CombatSyncCore Ends = MakeRunning(100);
	FGSTN_CombatTimelineLogic::EvaluateStart(Ends, 100, Data, 1, Output);
	int32 EndFrame = INDEX_NONE;
	for (int32 Frame = 101; Frame <= 140 && EndFrame == INDEX_NONE; ++Frame)
	{
		FGSTN_CombatTimelineLogic::AdvanceClock(Ends, Frame, &Data, 1, true, Output, Frame >= 102 && Frame <= 104);
		if (Ends.ActionState != EGSTN_ActionState::InTimeline)
		{
			EndFrame = Frame;
		}
	}
	TestEqual(TEXT("the timeline ends after its length plus the hitstop"), EndFrame, 133);

	// The combat clock stands still too
	TestEqual(TEXT("clock before"), FGSTN_CombatClock::FromFrame(101, 0), 101);
	TestEqual(TEXT("clock in hitstop frame 104, three paused"), FGSTN_CombatClock::FromFrame(104, 3), 101);
	TestEqual(TEXT("clock after"), FGSTN_CombatClock::FromFrame(105, 3), 102);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
