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

#endif // WITH_DEV_AUTOMATION_TESTS
