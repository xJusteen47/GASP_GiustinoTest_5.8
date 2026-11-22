// Automation tests of the rules the move actions of UGSTN_GA_BaseCombat run with (CP2 of the Blueprint seams): the actions of an Event key run in
// order, once per crossing (through hitstop and a rollback, on the same cursor as On Move Key), skip what is not an action and stop when an action
// ended the move; the tag window slots; the deterministic random of a move. The ability only keeps the cursors and the slots in sync variables and
// calls these, so they run here without a world or an ability system.

#include "Abilities/GSTN_MoveActions.h"
#include "Abilities/GSTN_MoveEvents.h"
#include "Misc/AutomationTest.h"
#include "StructUtils/InstancedStruct.h"
#include "Tests/GSTN_MoveActionTestTypes.h"
#include "Timeline/GSTN_TimelineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_MOVE_ACTIONS_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNMoveActionsTest
{
	FInstancedStruct MakeAction(int32 Id, bool bEndsMove = false)
	{
		FGSTN_TestMoveAction Action;
		Action.Id = Id;
		Action.bEndsMove = bEndsMove;
		return FInstancedStruct::Make(Action);
	}

	/** Keys on 2 (actions 1, 2) and 6 (action 3), as a timeline holds them */
	FGSTN_CombatTimelineData MakeData()
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 10;
		Data.FirstActionableFrame = 10;
		Data.OverrideFrames = 0;
		FGSTN_TimelineTrack_Event First;
		First.StartFrame = 2;
		First.EndFrame = 3;
		First.Actions.Add(MakeAction(1));
		First.Actions.Add(MakeAction(2));
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Event>(First));
		FGSTN_TimelineTrack_Event Second;
		Second.StartFrame = 6;
		Second.EndFrame = 7;
		Second.Actions.Add(MakeAction(3));
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Event>(Second));
		return Data;
	}

	/** The ability's key step: the keys crossed since the cursor, each key's actions in order (the cursor moves first, as in RaiseMoveEvents) */
	struct FCursor
	{
		int32 EvaluatedFrame = INDEX_NONE;

		void Step(const FGSTN_CombatTimelineData& Data, int32 TimelineFrame)
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
				const FGSTN_TimelineTrack_Event* Key = Data.Tracks[Track].GetPtr<FGSTN_TimelineTrack_Event>();
				FGSTN_MoveActionContext Context;
				Context.TimelineFrame = TimelineFrame;
				FGSTN_MoveActionRules::RunActions(Key->Actions, Context, [] { return FGSTN_TestMoveAction::MoveRunning(); });
			}
		}
	};

	void Reset()
	{
		FGSTN_TestMoveAction::Log().Reset();
		FGSTN_TestMoveAction::MoveRunning() = true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveActionsRunTest, "GSTNCombat.NP.MoveActions.RunInOrderAndStop", GSTN_MOVE_ACTIONS_TEST_FLAGS)
bool FGSTN_MoveActionsRunTest::RunTest(const FString& Parameters)
{
	using namespace GSTNMoveActionsTest;
	Reset();

	// In order; an empty entry and a struct that is not a move action (a track) run nothing
	TArray<FInstancedStruct> Actions;
	Actions.Add(MakeAction(1));
	Actions.AddDefaulted();
	Actions.Add(FInstancedStruct::Make(FGSTN_TimelineTrack_Tags()));
	Actions.Add(MakeAction(2));
	FGSTN_MoveActionContext Context;
	Context.TimelineFrame = 4;
	const int32 Ran = FGSTN_MoveActionRules::RunActions(Actions, Context, [] { return FGSTN_TestMoveAction::MoveRunning(); });
	TestEqual(TEXT("two actions ran"), Ran, 2);
	TestEqual(TEXT("in list order, the rest skipped"), FGSTN_TestMoveAction::Log(), TArray<FString>({ TEXT("1@4"), TEXT("2@4") }));

	// An action that ended the move (an event that started a replacing move) leaves the rest unrun
	Reset();
	TArray<FInstancedStruct> Ending;
	Ending.Add(MakeAction(5, true));
	Ending.Add(MakeAction(6));
	FGSTN_MoveActionRules::RunActions(Ending, Context, [] { return FGSTN_TestMoveAction::MoveRunning(); });
	TestEqual(TEXT("the action after the one that ended the move does not run"), FGSTN_TestMoveAction::Log(), TArray<FString>({ TEXT("5@4") }));

	// A move that is not running runs nothing
	Reset();
	FGSTN_TestMoveAction::MoveRunning() = false;
	TestEqual(TEXT("nothing runs for a move that stopped"), FGSTN_MoveActionRules::RunActions(Actions, Context, [] { return FGSTN_TestMoveAction::MoveRunning(); }), 0);
	Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveActionsKeysTest, "GSTNCombat.NP.MoveActions.KeyActionsOncePerCrossing", GSTN_MOVE_ACTIONS_TEST_FLAGS)
bool FGSTN_MoveActionsKeysTest::RunTest(const FString& Parameters)
{
	using namespace GSTNMoveActionsTest;
	const FGSTN_CombatTimelineData Data = MakeData();

	// Hitstop holds timeline frame 2 for three more simulation frames: the key's actions run once, on the crossing
	Reset();
	FCursor Cursor;
	const int32 Path[] = { 0, 1, 2, 2, 2, 2, 3, 4, 5, 6, 7 };
	for (const int32 Frame : Path)
	{
		Cursor.Step(Data, Frame);
	}
	TestEqual(TEXT("each key's actions once, in order"), FGSTN_TestMoveAction::Log(), TArray<FString>({ TEXT("1@2"), TEXT("2@2"), TEXT("3@6") }));

	// Rollback: the cursor of timeline frame 4 is restored (sync variable) and the frames after it are simulated again. The key on 6 runs again on
	// the corrected path (the predicted run was undone with the rest of the frame), the key on 2 does not
	Reset();
	FCursor Predicted;
	FCursor Restored;
	for (int32 Frame = 0; Frame <= 7; ++Frame)
	{
		Predicted.Step(Data, Frame);
		if (Frame == 4)
		{
			Restored = Predicted;
		}
	}
	FGSTN_TestMoveAction::Log().Reset();
	for (int32 Frame = 5; Frame <= 8; ++Frame)
	{
		Restored.Step(Data, Frame);
	}
	TestEqual(TEXT("the re-simulated frames run the corrected path's actions once"), FGSTN_TestMoveAction::Log(), TArray<FString>({ TEXT("3@6") }));

	// The first action of the key on 2 ends the move: its second action and the key on 6 never run
	Reset();
	FGSTN_CombatTimelineData Ending = MakeData();
	FGSTN_TimelineTrack_Event& First = Ending.Tracks[0].GetMutable<FGSTN_TimelineTrack_Event>();
	First.Actions[0] = MakeAction(1, true);
	FCursor EndCursor;
	for (int32 Frame = 0; Frame <= 7 && FGSTN_TestMoveAction::MoveRunning(); ++Frame)
	{
		EndCursor.Step(Ending, Frame);
	}
	TestEqual(TEXT("nothing runs after the action that ended the move"), FGSTN_TestMoveAction::Log(), TArray<FString>({ TEXT("1@2") }));
	Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveActionsTagWindowTest, "GSTNCombat.NP.MoveActions.TagWindowSlots", GSTN_MOVE_ACTIONS_TEST_FLAGS)
bool FGSTN_MoveActionsTagWindowTest::RunTest(const FString& Parameters)
{
	const FGameplayTag A = FGameplayTag::RequestGameplayTag(TEXT("Combat.Ability.Timeline"));
	const FGameplayTag B = FGameplayTag::RequestGameplayTag(TEXT("Combat.Ability.AwaitingAdoption"));
	const FGameplayTag C = FGameplayTag::RequestGameplayTag(TEXT("Combat"));

	TArray<FGameplayTag> Tags = { FGameplayTag(), FGameplayTag() };
	TArray<int32> Remaining = { 0, 0 };
	TestEqual(TEXT("an empty set opens in the first slot"), FGSTN_MoveActionRules::PickTagWindowSlot(Tags, Remaining, A), 0);

	Tags = { A, FGameplayTag() };
	Remaining = { 5, 0 };
	TestEqual(TEXT("a new tag takes the free slot"), FGSTN_MoveActionRules::PickTagWindowSlot(Tags, Remaining, B), 1);
	TestEqual(TEXT("the same tag re-arms its own slot"), FGSTN_MoveActionRules::PickTagWindowSlot(Tags, Remaining, A), 0);

	Tags = { A, B };
	Remaining = { 7, 3 };
	TestEqual(TEXT("a third tag replaces the window closing first"), FGSTN_MoveActionRules::PickTagWindowSlot(Tags, Remaining, C), 1);
	Remaining = { 3, 3 };
	TestEqual(TEXT("on a tie, the first slot"), FGSTN_MoveActionRules::PickTagWindowSlot(Tags, Remaining, C), 0);
	TestEqual(TEXT("no slots, no window"), FGSTN_MoveActionRules::PickTagWindowSlot(TArray<FGameplayTag>(), TArray<int32>(), A), (int32)INDEX_NONE);

	// Combat clock: a window of 3 opened on clock 10 is open on 10..12 and over on 13; the clock stands still in hitstop, so the window does too
	TestFalse(TEXT("open on its first frame"), FGSTN_MoveActionRules::IsTagWindowOver(10, 10, 3));
	TestFalse(TEXT("open on its last frame"), FGSTN_MoveActionRules::IsTagWindowOver(12, 10, 3));
	TestTrue(TEXT("over after its frames"), FGSTN_MoveActionRules::IsTagWindowOver(13, 10, 3));
	TestTrue(TEXT("a slot never opened counts as over"), FGSTN_MoveActionRules::IsTagWindowOver(13, INDEX_NONE, 3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MoveActionsRandomTest, "GSTNCombat.NP.MoveActions.DeterministicRandom", GSTN_MOVE_ACTIONS_TEST_FLAGS)
bool FGSTN_MoveActionsRandomTest::RunTest(const FString& Parameters)
{
	// The same frame, move and salt give the same number (every machine, every re-simulation); salts and frames tell draws apart
	const uint32 Seed = FGSTN_MoveActionRules::MakeSeed(120, 100, 0);
	TestEqual(TEXT("same inputs, same seed"), static_cast<int64>(Seed), static_cast<int64>(FGSTN_MoveActionRules::MakeSeed(120, 100, 0)));
	TestEqual(TEXT("same seed, same float"), FGSTN_MoveActionRules::RandomFloatInRange(Seed, -5.f, 5.f), FGSTN_MoveActionRules::RandomFloatInRange(Seed, -5.f, 5.f));
	TestTrue(TEXT("another salt, another seed"), Seed != FGSTN_MoveActionRules::MakeSeed(120, 100, 1));
	TestTrue(TEXT("another frame, another seed"), Seed != FGSTN_MoveActionRules::MakeSeed(121, 100, 0));

	bool bInRange = true;
	bool bHitMin = false;
	bool bHitMax = false;
	for (int32 Frame = 0; Frame < 2000; ++Frame)
	{
		const uint32 FrameSeed = FGSTN_MoveActionRules::MakeSeed(Frame, 7, 3);
		const float Value = FGSTN_MoveActionRules::RandomFloatInRange(FrameSeed, 2.f, 4.f);
		const int32 Integer = FGSTN_MoveActionRules::RandomIntInRange(FrameSeed, 1, 3);
		bInRange &= Value >= 2.f && Value <= 4.f && Integer >= 1 && Integer <= 3;
		bHitMin |= Integer == 1;
		bHitMax |= Integer == 3;
	}
	TestTrue(TEXT("draws stay in range"), bInRange);
	TestTrue(TEXT("the integer range is inclusive at both ends"), bHitMin && bHitMax);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
