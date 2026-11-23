// Body Capsule shift scheduling (dropkick-capsule-3, 2026-11-15): FGSTN_BodyCapsuleModifier queues its shift effect from OnPreMovement for the
// substep it runs in. Scheduled with Mover's network delay (QueueInstantMovementEffect_Internal, 0.3 s) the capsule changed size frames before
// the actor moved: at the dropkick's end it grew back to standing in place, under the floor, and was popped out 69 cm too high. The Mover
// fork's "this step" scheduling must execute on the issuing step itself, in both time modes.

#include "Misc/AutomationTest.h"
#include "MoverSchedulingUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BodyCapsuleScheduleTest, "GSTNCombat.NP.BodyCapsule.ThisStepSchedule",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_BodyCapsuleScheduleTest::RunTest(const FString& Parameters)
{
	FMoverTimeStep Step;
	Step.ServerFrame = 1256;
	Step.BaseSimTimeMs = 1256 * 16.0;
	Step.StepMs = 16.f;

	const FMoverSchedulingInfo Fixed = UE::Mover::MakeSchedulingInfoForThisStep(/*bIsFixedDt*/ true, Step);
	TestTrue(TEXT("fixed dt: executes on the issuing frame"), Fixed.ShouldExecuteAtFrame(Step.ServerFrame));
	TestFalse(TEXT("fixed dt: not before it"), Fixed.ShouldExecuteAtFrame(Step.ServerFrame - 1));
	TestTrue(TEXT("fixed dt: a resim of the issuing frame rolls it back"), Fixed.ShouldRollBackAtFrame(Step.ServerFrame));

	const FMoverSchedulingInfo Variable = UE::Mover::MakeSchedulingInfoForThisStep(/*bIsFixedDt*/ false, Step);
	// The state machine's test: the step's end time (MovementModeStateMachine::ApplyInstantEffects)
	TestTrue(TEXT("variable dt: executes within the issuing step"), Variable.ShouldExecuteAtTimeMs(Step.BaseSimTimeMs + Step.StepMs));
	TestTrue(TEXT("variable dt: at its start already"), Variable.ShouldExecuteAtTimeMs(Step.BaseSimTimeMs));
	TestFalse(TEXT("variable dt: not before it"), Variable.ShouldExecuteAtTimeMs(Step.BaseSimTimeMs - Step.StepMs));
	return true;
}

#endif
