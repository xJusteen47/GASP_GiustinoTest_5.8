// Headless tests of the analog walk start and the air turn (owner playtest 2026-11-16): tilt curve, walk-start blend, air yaw step.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Movement/GSTN_AnalogWalkTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AnalogWalkCurveTest, "GSTNCombat.Core.AnalogWalk.Curve", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AnalogWalkCurveTest::RunTest(const FString& Parameters)
{
	const FGSTN_AnalogWalkTuning Tuning;
	TestEqual(TEXT("a stick at rest asks nothing"), FGSTN_AnalogWalkLogic::StickToIntent(Tuning, 0.f), 0.f);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AnalogWalkAirTurnRateTest, "GSTNCombat.Core.AnalogWalk.AirTurnRate", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AnalogWalkAirTurnRateTest::RunTest(const FString& Parameters)
{
	// m3-air-turn 2026-11-17: the sim turns at the profile's rate (3600 in the Mario 3DW preset), not GASP's fixed 300 deg/s
	const float Dt = 1.f / 62.5f;
	const float Rate = 3600.f;
	TestEqual(TEXT("a gap wider than a frame turns at the full rate"), FGSTN_AnalogWalkLogic::AirTurnYawRate(Rate, 0.f, 120.f, Dt), Rate, 1e-2f);
	TestEqual(TEXT("the other side turns at minus the rate"), FGSTN_AnalogWalkLogic::AirTurnYawRate(Rate, 0.f, -120.f, Dt), -Rate, 1e-2f);
	TestEqual(TEXT("a small gap closes in exactly one frame"), FGSTN_AnalogWalkLogic::AirTurnYawRate(Rate, 10.f, 20.f, Dt), 10.f / Dt, 1e-2f);
	TestEqual(TEXT("across +-180 the short way"), FGSTN_AnalogWalkLogic::AirTurnYawRate(Rate, 170.f, -170.f, Dt), 20.f / Dt, 1e-2f);
	TestEqual(TEXT("an exact half turn goes positive, like StepAirYaw"), FGSTN_AnalogWalkLogic::AirTurnYawRate(Rate, 0.f, 180.f, Dt), Rate, 1e-2f);
	TestEqual(TEXT("rate 0 never turns"), FGSTN_AnalogWalkLogic::AirTurnYawRate(0.f, 0.f, 90.f, Dt), 0.f);
	TestEqual(TEXT("no time, no turn"), FGSTN_AnalogWalkLogic::AirTurnYawRate(Rate, 0.f, 90.f, 0.f), 0.f);

	// The producer's step and the sim's rate agree: a 180 reversal at 3600 deg/s faces the new way in ceil(180 / 57.6) = 4 frames
	FGSTN_AnalogWalkTuning Tuning;
	Tuning.AirTurnRateDegPerSec = Rate;
	float Yaw = 0.f;
	int32 Frames = 0;
	while (FMath::Abs(FRotator::NormalizeAxis(180.f - Yaw)) > 0.01f && Frames < 100)
	{
		const float Intent = FGSTN_AnalogWalkLogic::StepAirYaw(Tuning, Yaw, 180.f, Dt);
		Yaw = FRotator::NormalizeAxis(Yaw + FGSTN_AnalogWalkLogic::AirTurnYawRate(Rate, Yaw, Intent, Dt) * Dt);
		++Frames;
	}
	TestEqual(TEXT("180 at 3600 deg/s takes 4 frames"), Frames, 4);
	// The old cap (GASP's falling mode, 300 deg/s) took 38: the defect the QA pass measured (qa-movement-1117 defect 3)
	Yaw = 0.f;
	Frames = 0;
	while (FMath::Abs(FRotator::NormalizeAxis(180.f - Yaw)) > 0.01f && Frames < 100)
	{
		const float Intent = FGSTN_AnalogWalkLogic::StepAirYaw(Tuning, Yaw, 180.f, Dt);
		Yaw = FRotator::NormalizeAxis(Yaw + FGSTN_AnalogWalkLogic::AirTurnYawRate(300.f, Yaw, Intent, Dt) * Dt);
		++Frames;
	}
	TestEqual(TEXT("the 300 deg/s cap takes 38 frames"), Frames, 38);
	return true;
}

#endif
