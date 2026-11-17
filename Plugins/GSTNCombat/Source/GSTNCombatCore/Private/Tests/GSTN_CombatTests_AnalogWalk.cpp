// Headless tests of the analog walk start and the air turn (owner playtest 2026-11-16): tilt curve, walk-start blend, air yaw step.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Movement/GSTN_AnalogWalkTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AnalogWalkCurveTest, "GSTNCombat.Core.AnalogWalk.Curve", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AnalogWalkCurveTest::RunTest(const FString& Parameters)
{
	const FGSTN_AnalogWalkTuning Tuning;
	FGSTN_AnalogWalkTuning WithDeadzone = Tuning;
	WithDeadzone.InnerDeadzone = 0.05f;
	TestEqual(TEXT("inside the dead zone nothing moves"), FGSTN_AnalogWalkLogic::StickToIntent(WithDeadzone, 0.03f), 0.f);
	// Defaults (2026-11-17): no second dead zone and no minimum speed, so the intent starts from zero without a step
	const float Tiny = FGSTN_AnalogWalkLogic::StickToIntent(Tuning, 0.03f);
	TestTrue(TEXT("a tiny tilt moves a tiny bit, no step"), Tiny > 0.f && Tiny < 0.03f);
	const float Light = FGSTN_AnalogWalkLogic::StickToIntent(Tuning, 0.1f);
	const float Half = FGSTN_AnalogWalkLogic::StickToIntent(Tuning, 0.5f);
	TestTrue(TEXT("a light tilt walks, slowly"), Light >= Tuning.MinSpeedFraction && Light < 0.2f);
	TestTrue(TEXT("speed grows with the tilt"), Half > Light && Half < 1.f);
	TestEqual(TEXT("full tilt = full speed"), FGSTN_AnalogWalkLogic::StickToIntent(Tuning, 1.f), 1.f, 1e-4f);
	TestEqual(TEXT("FullTiltAt reaches 1"), FGSTN_AnalogWalkLogic::StickToIntent(Tuning, Tuning.FullTiltAt), 1.f, 1e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AnalogWalkStartTest, "GSTNCombat.Core.AnalogWalk.StartBlend", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AnalogWalkStartTest::RunTest(const FString& Parameters)
{
	const FGSTN_AnalogWalkTuning Tuning;
	TestEqual(TEXT("full boost at rest"), FGSTN_AnalogWalkLogic::StartBlend(Tuning, 0.f, 400.f), 1.f);
	TestEqual(TEXT("half boost at a quarter of the top speed"), FGSTN_AnalogWalkLogic::StartBlend(Tuning, 100.f, 400.f), 0.5f, 1e-4f);
	TestEqual(TEXT("no boost from LowSpeedFraction up"), FGSTN_AnalogWalkLogic::StartBlend(Tuning, 300.f, 400.f), 0.f);
	TestEqual(TEXT("no top speed, no boost"), FGSTN_AnalogWalkLogic::StartBlend(Tuning, 0.f, 0.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AnalogWalkAirYawTest, "GSTNCombat.Core.AnalogWalk.AirYaw", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AnalogWalkAirYawTest::RunTest(const FString& Parameters)
{
	FGSTN_AnalogWalkTuning Tuning;
	Tuning.AirTurnRateDegPerSec = 540.f;
	const float Dt = 1.f / 62.5f;
	TestEqual(TEXT("one frame turns at most rate * dt"), FGSTN_AnalogWalkLogic::StepAirYaw(Tuning, 0.f, 90.f, Dt), 540.f * Dt, 1e-3f);
	TestEqual(TEXT("a small gap closes in one frame"), FGSTN_AnalogWalkLogic::StepAirYaw(Tuning, 0.f, 3.f, Dt), 3.f, 1e-3f);
	TestEqual(TEXT("the shortest way across 180"), FGSTN_AnalogWalkLogic::StepAirYaw(Tuning, 170.f, -170.f, Dt), 170.f + 540.f * Dt, 1e-3f);
	Tuning.AirTurnRateDegPerSec = 0.f;
	TestEqual(TEXT("rate 0 never turns"), FGSTN_AnalogWalkLogic::StepAirYaw(Tuning, 10.f, 90.f, Dt), 10.f);
	return true;
}

#endif
