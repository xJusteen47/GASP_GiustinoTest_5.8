// Automation tests of the run skid and run start layer timing (polish M4). Pure: no world, no clip.

#include "Misc/AutomationTest.h"
#include "Presentation/GSTN_RunLayerAnimInstances.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_RUNLAYER_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SkidClipTimeTest, "GSTNCombat.NP.RunLayers.SkidClipTime", GSTN_RUNLAYER_TEST_FLAGS)
bool FGSTN_SkidClipTimeTest::RunTest(const FString& Parameters)
{
	const float Length = 2.f;
	const float Step = 0.016f;
	// StopFraction 0.6 of 2 s = 1.2 s; rest on frame 30
	TestEqual(TEXT("no phase, no time"), FGSTN_SkidClipTime::Compute(EGSTN_RunSkidPhase::None, 10, 30, Length, -1.f, 0.6f, 1.f, Step), 0.f);
	TestEqual(TEXT("first frame starts the clip"), FGSTN_SkidClipTime::Compute(EGSTN_RunSkidPhase::Enter, 0, 30, Length, -1.f, 0.6f, 1.f, Step), 0.f);
	TestTrue(TEXT("half way to rest = half way to the stop"), FMath::IsNearlyEqual(FGSTN_SkidClipTime::Compute(EGSTN_RunSkidPhase::Active, 15, 30, Length, -1.f, 0.6f, 1.f, Step), 0.6f, 1e-4f));
	TestTrue(TEXT("the rest frame is the stop"), FMath::IsNearlyEqual(FGSTN_SkidClipTime::Compute(EGSTN_RunSkidPhase::Active, 30, 30, Length, -1.f, 0.6f, 1.f, Step), 1.2f, 1e-4f));
	TestTrue(TEXT("a longer skid stretches the same clip"), FMath::IsNearlyEqual(FGSTN_SkidClipTime::Compute(EGSTN_RunSkidPhase::Active, 30, 60, Length, -1.f, 0.6f, 1.f, Step), 0.6f, 1e-4f));
	TestTrue(TEXT("the exit plays on from the stop"), FMath::IsNearlyEqual(FGSTN_SkidClipTime::Compute(EGSTN_RunSkidPhase::Exit, 40, 30, Length, -1.f, 0.6f, 1.f, Step), 1.2f + 10 * Step, 1e-4f));
	TestTrue(TEXT("at the exit rate"), FMath::IsNearlyEqual(FGSTN_SkidClipTime::Compute(EGSTN_RunSkidPhase::Exit, 40, 30, Length, -1.f, 0.6f, 2.f, Step), 1.2f + 20 * Step, 1e-4f));
	TestEqual(TEXT("never past the clip"), FGSTN_SkidClipTime::Compute(EGSTN_RunSkidPhase::Exit, 500, 30, Length, -1.f, 0.6f, 1.f, Step), Length);
	TestTrue(TEXT("an explicit stop time wins"), FMath::IsNearlyEqual(FGSTN_SkidClipTime::Compute(EGSTN_RunSkidPhase::Active, 30, 30, Length, 0.9f, 0.6f, 1.f, Step), 0.9f, 1e-4f));
	TestTrue(TEXT("no rest frame yet sits on the stop"), FMath::IsNearlyEqual(FGSTN_SkidClipTime::Compute(EGSTN_RunSkidPhase::Enter, 3, 0, Length, -1.f, 0.6f, 1.f, Step), 1.2f, 1e-4f));
	TestEqual(TEXT("no clip, no time"), FGSTN_SkidClipTime::Compute(EGSTN_RunSkidPhase::Active, 3, 30, 0.f, -1.f, 0.6f, 1.f, Step), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunStartClipStateTest, "GSTNCombat.NP.RunLayers.RunStartTrigger", GSTN_RUNLAYER_TEST_FLAGS)
bool FGSTN_RunStartClipStateTest::RunTest(const FString& Parameters)
{
	const float Dt = 0.016f;
	FGSTN_RunStartClipState State;
	State.Step(Dt, 0.f, true, false, 30.f, 0.5f, 1.f);
	TestFalse(TEXT("still: nothing"), State.bActive);
	State.Step(Dt, 120.f, true, false, 30.f, 0.5f, 1.f);
	TestTrue(TEXT("starting to run from still plays it"), State.bActive);
	for (int32 i = 0; i < 40; ++i)
	{
		State.Step(Dt, 400.f, true, false, 30.f, 0.5f, 1.f);
	}
	TestFalse(TEXT("once, for the clip's length"), State.bActive);
	State.Step(Dt, 450.f, true, false, 30.f, 0.5f, 1.f);
	TestFalse(TEXT("running on does not restart it"), State.bActive);

	FGSTN_RunStartClipState Air;
	Air.Step(Dt, 0.f, false, false, 30.f, 0.5f, 1.f);
	Air.Step(Dt, 200.f, false, false, 30.f, 0.5f, 1.f);
	TestFalse(TEXT("not in the air"), Air.bActive);

	FGSTN_RunStartClipState Cut;
	Cut.Step(Dt, 0.f, true, false, 30.f, 0.5f, 1.f);
	Cut.Step(Dt, 200.f, true, false, 30.f, 0.5f, 1.f);
	Cut.Step(Dt, 200.f, true, true, 30.f, 0.5f, 1.f);
	TestFalse(TEXT("a skid cuts it"), Cut.bActive);

	FGSTN_RunStartClipState NoClip;
	NoClip.Step(Dt, 0.f, true, false, 30.f, 0.f, 1.f);
	NoClip.Step(Dt, 200.f, true, false, 30.f, 0.f, 1.f);
	TestFalse(TEXT("no clip (the default today): never"), NoClip.bActive);
	return true;
}

#endif
