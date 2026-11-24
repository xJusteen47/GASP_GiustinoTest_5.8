// Automation tests of the rules the Blueprint move events of UGSTN_GA_BaseCombat are raised with (CP1 of the Blueprint seams): the Event keys
// crossed by a frame, the hit groups newly confirmed, the end reason. The ability only keeps the cursors in sync variables and calls these, so
// the rules run here without a world or an ability system, through hitstop, a hold and a rollback, the way the ability steps them.

#include "Abilities/GSTN_MoveEvents.h"
#include "Misc/AutomationTest.h"
#include "StructUtils/InstancedStruct.h"
#include "Timeline/GSTN_TimelinePlayer.h"
#include "Timeline/GSTN_TimelineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_MOVE_EVENTS_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNMoveEventsTest
{
	FGSTN_TimelineTrack_Event MakeKey(int32 Frame, const TCHAR* Tag = nullptr)
	{
		FGSTN_TimelineTrack_Event Key;
		Key.StartFrame = Frame;
		Key.EndFrame = Frame + 1;
		if (Tag)
		{
			Key.KeyTag = FGameplayTag::RequestGameplayTag(Tag);
		}
		return Key;
	}

	/** Keys on 0, 5, 5 and 9 (tracks 0, 2, 3, 4) and a tag window over 5 (track 1), which is not a key */
	FGSTN_CombatTimelineData MakeData()
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 12;
		Data.FirstActionableFrame = 12;
		Data.OverrideFrames = 0;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Event>(MakeKey(0)));
		FGSTN_TimelineTrack_Tags Window;
		Window.StartFrame = 4;
		Window.EndFrame = 7;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Tags>(Window));
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Event>(MakeKey(5)));
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Event>(MakeKey(5)));
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Event>(MakeKey(9)));
		return Data;
	}

	/** The ability's cursor: the last evaluated frame (a sync variable), and the log of the keys it raised as "track@simframe" */
	struct FCursor
	{
		int32 EvaluatedFrame = INDEX_NONE;

		void Step(const FGSTN_CombatTimelineData& Data, int32 SimFrame, int32 TimelineFrame, TArray<FString>& Log)
		{
			if (TimelineFrame <= EvaluatedFrame)
			{
				return;
			}
			FGSTN_MoveEventRules::FKeyList Keys;
			FGSTN_MoveEventRules::CollectKeys(Data, EvaluatedFrame, TimelineFrame, Keys);
			EvaluatedFrame = TimelineFrame;
			for (const int32 Track : Keys)
			{
				Log.Add(FString::Printf(TEXT("%d@%d"), Track, SimFrame));
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveEventsKeysTest, "GSTNCombat.NP.MoveEvents.KeysOncePerCrossing", GSTN_MOVE_EVENTS_TEST_FLAGS)
bool FGSTN_MoveEventsKeysTest::RunTest(const FString& Parameters)
{
	using namespace GSTNMoveEventsTest;
	const FGSTN_CombatTimelineData Data = MakeData();

	// Start on sim frame 100; hitstop stands the timeline still on frames 104..106 (timeline frame 3 three more times)
	TArray<FString> Log;
	FCursor Cursor;
	int32 Paused = 0;
	for (int32 Sim = 100; Sim < 115; ++Sim)
	{
		Paused += (Sim >= 104 && Sim < 107) ? 1 : 0;
		Cursor.Step(Data, Sim, Sim - 100 - Paused, Log);
	}
	TestEqual(TEXT("frame 0 key on the start frame, both frame 5 keys in track order on one frame, the frame 9 key after the hitstop"),
		Log, TArray<FString>({ TEXT("0@100"), TEXT("2@108"), TEXT("3@108"), TEXT("4@112") }));

	// Skipped frames (a correction that jumps): the keys in between still raise, once each
	TArray<FString> Jump;
	FCursor Skipping;
	Skipping.Step(Data, 0, 0, Jump);
	Skipping.Step(Data, 1, 7, Jump);
	Skipping.Step(Data, 2, 7, Jump);
	TestEqual(TEXT("a jump over 5 raises both of its keys once"), Jump, TArray<FString>({ TEXT("0@0"), TEXT("2@1"), TEXT("3@1") }));

	// The crossing rule is the timeline player's: on every frame the keys are the Event tracks of the player's output
	FGSTN_TimelineOutput Output;
	int32 Previous = INDEX_NONE;
	bool bSame = true;
	for (int32 Frame = 0; Frame < 12; ++Frame)
	{
		FGSTN_TimelineEvalParams Params;
		Params.Frame = Frame;
		Params.PreviousFrame = Previous;
		FGSTN_TimelinePlayer::Evaluate(Data, Params, Output);
		FGSTN_MoveEventRules::FKeyList Keys;
		FGSTN_MoveEventRules::CollectKeys(Data, Previous, Frame, Keys);
		bSame &= Keys.Num() == Output.Events.Num();
		for (int32 Index = 0; bSame && Index < Keys.Num(); ++Index)
		{
			bSame &= Data.Tracks[Keys[Index]].GetPtr<FGSTN_TimelineTrack_Event>() == Output.Events[Index];
		}
		Previous = Frame;
	}
	TestTrue(TEXT("the keys are the player's crossed Event tracks, frame by frame"), bSame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveEventsRollbackTest, "GSTNCombat.NP.MoveEvents.Rollback", GSTN_MOVE_EVENTS_TEST_FLAGS)
bool FGSTN_MoveEventsRollbackTest::RunTest(const FString& Parameters)
{
	using namespace GSTNMoveEventsTest;
	const FGSTN_CombatTimelineData Data = MakeData();

	// Predicted path: frames 0..7 raise keys 0, 2, 3. A correction restores the cursor of frame 3 (sync variable) and frames 4..10 are simulated
	// again; on the corrected path a hitstop of 2 frames starts on frame 4. Every key of the corrected path is raised exactly once on it
	TArray<FString> Predicted;
	FCursor Cursor;
	FCursor Snapshot;
	for (int32 Sim = 0; Sim <= 7; ++Sim)
	{
		Cursor.Step(Data, Sim, Sim, Predicted);
		if (Sim == 3)
		{
			Snapshot = Cursor;
		}
	}
	TestEqual(TEXT("predicted keys"), Predicted, TArray<FString>({ TEXT("0@0"), TEXT("2@5"), TEXT("3@5") }));

	Cursor = Snapshot;
	TArray<FString> Corrected;
	int32 Paused = 0;
	for (int32 Sim = 4; Sim <= 12; ++Sim)
	{
		Paused += (Sim == 4 || Sim == 5) ? 1 : 0;
		Cursor.Step(Data, Sim, Sim - Paused, Corrected);
	}
	TestEqual(TEXT("the re-simulated frames raise the corrected path's keys once each"), Corrected, TArray<FString>({ TEXT("2@7"), TEXT("3@7"), TEXT("4@11") }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveEventsHitGroupsTest, "GSTNCombat.NP.MoveEvents.HitConfirmGroups", GSTN_MOVE_EVENTS_TEST_FLAGS)
bool FGSTN_MoveEventsHitGroupsTest::RunTest(const FString& Parameters)
{
	// The confirm flags of the run as the combat state shows them frame by frame: group 0 confirms on frame 2, group 1 on frame 4, then both stay
	const uint8 Flags[] = { 0, 0, 1, 1, 3, 3, 3 };
	uint8 Reported = 0;
	TArray<int32> Raised;
	for (int32 Frame = 0; Frame < static_cast<int32>(UE_ARRAY_COUNT(Flags)); ++Frame)
	{
		const uint8 New = FGSTN_MoveEventRules::NewConfirmGroups(Reported, Flags[Frame]);
		Reported = Flags[Frame];
		if (New != 0)
		{
			Raised.Add(Frame * 10 + New);
		}
	}
	TestEqual(TEXT("one event per newly confirmed group: group bit 1 on frame 2, bit 2 on frame 4"), Raised, TArray<int32>({ 21, 42 }));

	// Rollback to frame 3 (reported = 1) and a corrected path where group 1 confirms on frame 5: raised once, on 5
	Reported = 1;
	const uint8 Corrected[] = { 1, 1, 3 };
	TArray<int32> Again;
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Corrected)); ++Index)
	{
		const uint8 New = FGSTN_MoveEventRules::NewConfirmGroups(Reported, Corrected[Index]);
		Reported = Corrected[Index];
		if (New != 0)
		{
			Again.Add((3 + Index) * 10 + New);
		}
	}
	TestEqual(TEXT("the corrected confirm raises once"), Again, TArray<int32>({ 52 }));
	TestEqual(TEXT("a new run starts from nothing"), static_cast<int32>(FGSTN_MoveEventRules::NewConfirmGroups(0, 0)), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveEventsEndReasonTest, "GSTNCombat.NP.MoveEvents.EndReason", GSTN_MOVE_EVENTS_TEST_FLAGS)
bool FGSTN_MoveEventsEndReasonTest::RunTest(const FString& Parameters)
{
	// A 30 frame move with a tail from 22
	TestEqual(TEXT("played to the last frame"), FGSTN_MoveEventRules::ResolveEndReason(false, false, 29, 30, 22), EGSTN_MoveEndReason::Finished);
	TestEqual(TEXT("cut in the tail"), FGSTN_MoveEventRules::ResolveEndReason(false, false, 24, 30, 22), EGSTN_MoveEndReason::TailCancelled);
	TestEqual(TEXT("cancelled in the tail"), FGSTN_MoveEventRules::ResolveEndReason(true, false, 24, 30, 22), EGSTN_MoveEndReason::TailCancelled);
	TestEqual(TEXT("hit out of the startup"), FGSTN_MoveEventRules::ResolveEndReason(true, false, 6, 30, 22), EGSTN_MoveEndReason::Interrupted);
	TestEqual(TEXT("replaced before the tail"), FGSTN_MoveEventRules::ResolveEndReason(false, false, 15, 30, 22), EGSTN_MoveEndReason::Interrupted);
	TestEqual(TEXT("no tail: early is interrupted"), FGSTN_MoveEventRules::ResolveEndReason(false, false, 24, 30, INDEX_NONE), EGSTN_MoveEndReason::Interrupted);
	TestEqual(TEXT("a recoil wins"), FGSTN_MoveEventRules::ResolveEndReason(false, true, 29, 30, 22), EGSTN_MoveEndReason::AfterRecoil);
	TestEqual(TEXT("ended before any frame"), FGSTN_MoveEventRules::ResolveEndReason(true, false, INDEX_NONE, 30, 22), EGSTN_MoveEndReason::Interrupted);
	// getup-momentum-2: a cancel on the first tail frame, seen by the ability before its own tick moved the cursor there
	TestEqual(TEXT("cancel on the tail's first frame: the clock's frame"), FGSTN_MoveEventRules::EndReasonFrame(true, 21, 22), 22);
	TestEqual(TEXT("so it is a tail cut"), FGSTN_MoveEventRules::ResolveEndReason(true, false, FGSTN_MoveEventRules::EndReasonFrame(true, 21, 22), 30, 22),
		EGSTN_MoveEndReason::TailCancelled);
	TestEqual(TEXT("a cancel before the tail stays interrupted"), FGSTN_MoveEventRules::ResolveEndReason(true, false, FGSTN_MoveEventRules::EndReasonFrame(true, 20, 21), 30, 22),
		EGSTN_MoveEndReason::Interrupted);
	TestEqual(TEXT("a natural end keeps the cursor"), FGSTN_MoveEventRules::EndReasonFrame(false, 21, 22), 21);
	return true;
}

#endif
