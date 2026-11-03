// Automation tests of the loadout table swap step (loadout-table-swap-design 2c). Pure: the step is a function of the sync state.

#include "Misc/AutomationTest.h"
#include "Simulation/GSTN_LoadoutSwapLogic.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** A combatant running timeline 2 from frame 100, weapon 1 active, a press of button 0 at frame 105; the server demands revision 4 while the simulation is on 3 */
	FGSTN_CombatSyncCore MakeBusyCore()
	{
		FGSTN_CombatSyncCore Core;
		Core.LoadoutRevision = 4;
		Core.TableRevision = 3;
		Core.ActionState = EGSTN_ActionState::InTimeline;
		Core.ActiveTimeline = 2;
		Core.TimelineStartFrame = 100;
		Core.ActiveWeaponIndex = 1;

		FGSTN_InputSample Sample;
		Sample.Frame = 105;
		Sample.Held = 1;
		Sample.Pressed = 1;
		Core.Input.History.Append(Sample);
		return Core;
	}

	bool SameState(const FGSTN_CombatSyncCore& A, const FGSTN_CombatSyncCore& B)
	{
		return !A.ShouldReconcile(B) && !B.ShouldReconcile(A);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SwapBoundaryOnceTest, "GSTNCombat.NP.Swap.BoundaryOnce", GSTN_NP_TEST_FLAGS)
bool FGSTN_SwapBoundaryOnceTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatSyncCore Core = MakeBusyCore();
	TestTrue(TEXT("the first frame with the table swaps"), FGSTN_LoadoutSwapLogic::Step(Core, true, 110) == EGSTN_LoadoutSwapResult::Swapped);
	TestEqual(TEXT("the table revision caught up"), static_cast<int32>(Core.TableRevision), 4);
	TestTrue(TEXT("the next frame does not swap again"), FGSTN_LoadoutSwapLogic::Step(Core, true, 111) == EGSTN_LoadoutSwapResult::Unchanged);

	FGSTN_CombatSyncCore Idle;
	TestTrue(TEXT("a combatant that never had a loadout is unchanged"), FGSTN_LoadoutSwapLogic::Step(Idle, true, 5) == EGSTN_LoadoutSwapResult::Unchanged);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SwapClosedTest, "GSTNCombat.NP.Swap.ClosedWithoutTable", GSTN_NP_TEST_FLAGS)
bool FGSTN_SwapClosedTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatSyncCore Core = MakeBusyCore();
	const FGSTN_CombatSyncCore Before = Core;
	TestTrue(TEXT("no table for the demand: the gate is closed"), FGSTN_LoadoutSwapLogic::Step(Core, false, 110) == EGSTN_LoadoutSwapResult::Closed);
	TestTrue(TEXT("and nothing in the state moved"), SameState(Core, Before));

	TestTrue(TEXT("once the table is there the swap runs"), FGSTN_LoadoutSwapLogic::Step(Core, true, 111) == EGSTN_LoadoutSwapResult::Swapped);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SwapCancelsTimelineTest, "GSTNCombat.NP.Swap.CancelsTimeline", GSTN_NP_TEST_FLAGS)
bool FGSTN_SwapCancelsTimelineTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatSyncCore Core = MakeBusyCore();
	FGSTN_LoadoutSwapLogic::Step(Core, true, 110);
	TestTrue(TEXT("the combatant is actionable"), Core.ActionState == EGSTN_ActionState::Actionable && Core.ActiveTimeline == INDEX_NONE);
	TestEqual(TEXT("the primary weapon is active"), static_cast<int32>(Core.ActiveWeaponIndex), 0);
	TestTrue(TEXT("the cancel names the old registry slot, for this frame"), Core.Requests.Find(EGSTN_RequestType::CancelTimeline, 110, 2) != nullptr);

	// An idle combatant gets no cancel
	FGSTN_CombatSyncCore Idle;
	Idle.LoadoutRevision = 1;
	FGSTN_LoadoutSwapLogic::Step(Idle, true, 20);
	TestTrue(TEXT("no timeline, no cancel request"), Idle.Requests.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SwapConsumesBufferTest, "GSTNCombat.NP.Swap.ConsumesBuffer", GSTN_NP_TEST_FLAGS)
bool FGSTN_SwapConsumesBufferTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatSyncCore Core = MakeBusyCore();
	const FGSTN_InputHistory& History = Core.Input.History;
	TestFalse(TEXT("the press waits in the buffer"), History.GetButtonEvent(History.GetButtonSlot(0)).bConsumed);

	FGSTN_LoadoutSwapLogic::Step(Core, true, 110);
	TestTrue(TEXT("the press made for the old kit is used up"), History.GetButtonEvent(History.GetButtonSlot(0)).bConsumed);
	TestTrue(TEXT("and the stick anchors before the boundary"), History.GetLastConsumedStickFrame() >= 109);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SwapResimTest, "GSTNCombat.NP.Swap.ResimSameResult", GSTN_NP_TEST_FLAGS)
bool FGSTN_SwapResimTest::RunTest(const FString& Parameters)
{
	const FGSTN_CombatSyncCore Start = MakeBusyCore();
	FGSTN_CombatSyncCore First = Start;
	FGSTN_CombatSyncCore Second = Start;
	FGSTN_LoadoutSwapLogic::Step(First, true, 110);
	FGSTN_LoadoutSwapLogic::Step(Second, true, 110);
	TestTrue(TEXT("running the step twice on copies of one state gives one result"), SameState(First, Second));

	// A client that reconciled to the state after the boundary adopts it: the step is then a no-op
	FGSTN_CombatSyncCore Adopted = First;
	TestTrue(TEXT("no second swap on top of the adopted state"), FGSTN_LoadoutSwapLogic::Step(Adopted, true, 111) == EGSTN_LoadoutSwapResult::Unchanged);
	TestTrue(TEXT("which changes nothing"), SameState(Adopted, First));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SwapForcedCommandTest, "GSTNCombat.NP.Swap.ForcedCommandDroppedOnSwapFrame", GSTN_NP_TEST_FLAGS)
bool FGSTN_SwapForcedCommandTest::RunTest(const FString& Parameters)
{
	// The step only reports the boundary; the component drops the frame's forced command when it sees Swapped. A forced index is table
	// specific and carries no revision, so the frame before the boundary and the frame after it are the only safe ones
	FGSTN_CombatSyncCore Core = MakeBusyCore();
	TestTrue(TEXT("the swap frame is flagged for the component"), FGSTN_LoadoutSwapLogic::Step(Core, true, 110) == EGSTN_LoadoutSwapResult::Swapped);
	TestTrue(TEXT("the next frame is not"), FGSTN_LoadoutSwapLogic::Step(Core, true, 111) != EGSTN_LoadoutSwapResult::Swapped);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
