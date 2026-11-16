// Automation tests of the wait nodes (move-timeline-decoupling 2A and 3A): the rules Wait Combat Frames and Wait Command decide with, the
// condition they share with the cancel windows, and the sync data a rollback restores them from. The nodes themselves need an ability system
// in its simulation step and run in the PIE harness on the desktop (see the handover of this checkpoint).

#include "Abilities/GSTN_CombatWaitTasks.h"
#include "Misc/AutomationTest.h"
#include "NetworkPredictionReplicationProxy.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_WAIT_TASK_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNWaitTaskTest
{
	template <typename TData>
	TData RoundTrip(TData& Source)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		FNetSerializeParams WriteParams(Writer);
		Source.NetSerialize(WriteParams);
		TData Copy;
		FMemoryReader Reader(Bytes);
		FNetSerializeParams ReadParams(Reader);
		Copy.NetSerialize(ReadParams);
		return Copy;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WaitCombatFramesRulesTest, "GSTNCombat.NP.WaitTasks.CombatFrames", GSTN_WAIT_TASK_TEST_FLAGS)
bool FGSTN_WaitCombatFramesRulesTest::RunTest(const FString& Parameters)
{
	// Started on clock 10 for 3 frames: 11 and 12 wait, 13 finishes. The clock stands still in hitstop, so frames there do not count
	TestFalse(TEXT("1 of 3 frames"), FGSTN_CombatWaitRules::HasElapsed(10, 11, 3));
	TestFalse(TEXT("2 of 3 frames"), FGSTN_CombatWaitRules::HasElapsed(10, 12, 3));
	TestTrue(TEXT("3 of 3 frames"), FGSTN_CombatWaitRules::HasElapsed(10, 13, 3));
	TestFalse(TEXT("the start frame never finishes a wait"), FGSTN_CombatWaitRules::HasElapsed(10, 10, 0));
	TestTrue(TEXT("a wait of 0 lasts one frame"), FGSTN_CombatWaitRules::HasElapsed(10, 11, 0));
	TestEqual(TEXT("frames left at the start"), FGSTN_CombatWaitRules::FramesLeft(10, 10, 3), 3);
	TestEqual(TEXT("frames left on the way"), FGSTN_CombatWaitRules::FramesLeft(10, 12, 3), 1);
	TestEqual(TEXT("frames left past the end"), FGSTN_CombatWaitRules::FramesLeft(10, 20, 3), 0);

	// What a rollback restores the node from
	FGSTN_WaitCombatFramesTaskData Data;
	Data.StartClock = 1234;
	Data.Frames = 17;
	const FGSTN_WaitCombatFramesTaskData Copy = GSTNWaitTaskTest::RoundTrip(Data);
	TestEqual(TEXT("start clock survives the wire"), Copy.StartClock, 1234);
	TestEqual(TEXT("frames survive the wire"), Copy.Frames, 17);
	TestFalse(TEXT("equal data does not reconcile"), Copy.ShouldReconcile(Data));
	FGSTN_WaitCombatFramesTaskData Other = Data;
	Other.StartClock = 1235;
	TestTrue(TEXT("another start reconciles"), Other.ShouldReconcile(Data));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WaitCommandRulesTest, "GSTNCombat.NP.WaitTasks.Command", GSTN_WAIT_TASK_TEST_FLAGS)
bool FGSTN_WaitCommandRulesTest::RunTest(const FString& Parameters)
{
	// Window of 4 frames from clock 20: [20, 24)
	TestFalse(TEXT("open on its first frame"), FGSTN_CombatWaitRules::IsWindowOver(20, 20, 4));
	TestFalse(TEXT("open on its last frame"), FGSTN_CombatWaitRules::IsWindowOver(20, 23, 4));
	TestTrue(TEXT("closed after it"), FGSTN_CombatWaitRules::IsWindowOver(20, 24, 4));
	TestFalse(TEXT("no window never closes"), FGSTN_CombatWaitRules::IsWindowOver(20, 100000, 0));

	TestTrue(TEXT("entered and takeable fires"), FGSTN_CombatWaitRules::StepWaitCommand(20, 21, 4, true, true) == EGSTN_WaitCommandStep::Fire);
	TestTrue(TEXT("entered on the last frame still fires"), FGSTN_CombatWaitRules::StepWaitCommand(20, 23, 4, true, true) == EGSTN_WaitCommandStep::Fire);
	TestTrue(TEXT("not entered keeps waiting"), FGSTN_CombatWaitRules::StepWaitCommand(20, 21, 4, true, false) == EGSTN_WaitCommandStep::Keep);
	TestTrue(TEXT("entered in hitstop or a reaction waits (the press stays buffered)"), FGSTN_CombatWaitRules::StepWaitCommand(20, 21, 4, false, true) == EGSTN_WaitCommandStep::Keep);
	TestTrue(TEXT("past the window expires, even with the input there"), FGSTN_CombatWaitRules::StepWaitCommand(20, 24, 4, true, true) == EGSTN_WaitCommandStep::Expire);
	TestTrue(TEXT("no window: waits for ever"), FGSTN_CombatWaitRules::StepWaitCommand(20, 5000, 0, true, false) == EGSTN_WaitCommandStep::Keep);

	// A kept press (Consume off) does not fire a repeating node twice
	TestTrue(TEXT("first input is new"), FGSTN_CombatWaitRules::IsNewInput(30, INDEX_NONE));
	TestFalse(TEXT("the same input again is not"), FGSTN_CombatWaitRules::IsNewInput(30, 30));
	TestTrue(TEXT("a later input is"), FGSTN_CombatWaitRules::IsNewInput(34, 30));

	// The condition: the cancel windows' rule
	FGSTN_CombatSyncCore Core;
	TestTrue(TEXT("Always holds"), FGSTN_CombatTimelineLogic::IsConditionMet(EGSTN_CancelCondition::Always, Core, 0));
	TestTrue(TEXT("On Whiff holds with no hit"), FGSTN_CombatTimelineLogic::IsConditionMet(EGSTN_CancelCondition::OnWhiff, Core, 0));
	TestFalse(TEXT("On Hit does not hold with no hit"), FGSTN_CombatTimelineLogic::IsConditionMet(EGSTN_CancelCondition::OnHit, Core, 0));
	Core.Hit.HitConfirmFlags = 1;
	TestTrue(TEXT("On Hit holds once a hit confirmed"), FGSTN_CombatTimelineLogic::IsConditionMet(EGSTN_CancelCondition::OnHit, Core, 0));
	TestFalse(TEXT("On Whiff does not after a hit"), FGSTN_CombatTimelineLogic::IsConditionMet(EGSTN_CancelCondition::OnWhiff, Core, 0));
	TestFalse(TEXT("On Block only from an extension"), FGSTN_CombatTimelineLogic::IsConditionMet(EGSTN_CancelCondition::OnBlock, Core, 0));
	TestTrue(TEXT("On Block when an extension holds it"), FGSTN_CombatTimelineLogic::IsConditionMet(EGSTN_CancelCondition::OnBlock, Core, 1u << static_cast<uint32>(EGSTN_CancelCondition::OnBlock)));

	// What a rollback restores the node from: every pin, the command as its table index
	FGSTN_WaitCommandTaskData Data;
	Data.StartClock = 400;
	Data.StartFrame = 512;
	Data.WindowFrames = 12;
	Data.LastFiredInputFrame = INDEX_NONE;
	Data.CommandIndex = 7;
	Data.BufferFrames = -1;
	Data.Flags = FGSTN_WaitCommandTaskData::FlagConsume | FGSTN_WaitCommandTaskData::FlagBeforeBindings;
	Data.Condition = static_cast<uint8>(EGSTN_CancelCondition::OnHit);
	const FGSTN_WaitCommandTaskData Copy = GSTNWaitTaskTest::RoundTrip(Data);
	TestEqual(TEXT("start clock"), Copy.StartClock, 400);
	TestEqual(TEXT("start frame"), Copy.StartFrame, 512);
	TestEqual(TEXT("window"), Copy.WindowFrames, 12);
	TestEqual(TEXT("no input fired yet"), Copy.LastFiredInputFrame, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("command index"), static_cast<int32>(Copy.CommandIndex), 7);
	TestEqual(TEXT("own buffer (-1)"), static_cast<int32>(Copy.BufferFrames), -1);
	TestEqual(TEXT("flags"), static_cast<int32>(Copy.Flags), static_cast<int32>(Data.Flags));
	TestEqual(TEXT("condition"), static_cast<int32>(Copy.Condition), static_cast<int32>(EGSTN_CancelCondition::OnHit));
	TestFalse(TEXT("equal data does not reconcile"), Copy.ShouldReconcile(Data));
	FGSTN_WaitCommandTaskData Fired = Data;
	Fired.LastFiredInputFrame = 515;
	TestTrue(TEXT("a fired input reconciles"), Fired.ShouldReconcile(Data));

	// An inactive node claims nothing, whatever it held
	const UGSTN_WaitCommandTask* Idle = GetDefault<UGSTN_WaitCommandTask>();
	TestFalse(TEXT("an inactive node does not hold the bindings back"), Idle->ClaimsCommand(0));
	return true;
}

#endif
