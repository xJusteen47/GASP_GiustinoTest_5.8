// Tests of the covered landing (FGSTN_CoveredLandingLogic, 2026-11-17): GSTNAnimation.Core.CoveredLanding.*
//
// Pure: the rules of the window, no mesh. Frames at 60 Hz.

#if WITH_DEV_AUTOMATION_TESTS

#include "GSTN_CoveredLanding.h"
#include "Misc/AutomationTest.h"

namespace GSTNCoveredLandingTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Dt = 1.f / 60.f;

	/** Falls for a few frames under a montage, then touches down on the next Step (returned) */
	FGSTN_CoveredLandingOutput FallAndLand(FGSTN_CoveredLandingLogic& Logic, const FGSTN_CoveredLandingSettings& Settings, bool bMontage)
	{
		for (int32 Frame = 0; Frame < 4; ++Frame)
		{
			Logic.Step(Settings, Dt, -600.f, 0.f, bMontage, bMontage ? 1.f : 0.f);
		}
		return Logic.Step(Settings, Dt, 0.f, 0.f, bMontage, bMontage ? 1.f : 0.f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CoveredLandingHoldTest, "GSTNAnimation.Core.CoveredLanding.Hold", GSTNCoveredLandingTest::TestFlags)
bool FGSTN_CoveredLandingHoldTest::RunTest(const FString& Parameters)
{
	using namespace GSTNCoveredLandingTest;
	const FGSTN_CoveredLandingSettings Settings;
	FGSTN_CoveredLandingLogic Logic;
	const FGSTN_CoveredLandingOutput Touch = FallAndLand(Logic, Settings, true);
	TestTrue(TEXT("touchdown under a full montage: capture"), Touch.bCapture);
	TestEqual(TEXT("window opens at the touchdown"), Logic.GetWindowTime(), 0.f);
	TestEqual(TEXT("no hold while the montage drives"), Touch.HoldAlpha, 0.f);

	// Landing lag of 8 frames, then the montage blends out: the held pose covers the base layer's landing until HoldUntil
	for (int32 Frame = 0; Frame < 8; ++Frame)
	{
		TestTrue(TEXT("capture while the montage drives"), Logic.Step(Settings, Dt, 0.f, 0.f, true, 1.f).bCapture);
	}
	const FGSTN_CoveredLandingOutput BlendOut = Logic.Step(Settings, Dt, 0.f, 0.f, false, 0.7f);
	TestFalse(TEXT("no capture once it stops"), BlendOut.bCapture);
	TestEqual(TEXT("full hold during the montage's blend-out"), BlendOut.HoldAlpha, 1.f);

	float Alpha = 1.f;
	int32 Frames = 0;
	while (Logic.GetWindowTime() >= 0.f && Frames < 200)
	{
		const float Next = Logic.Step(Settings, Dt, 0.f, 0.f, false, 0.f).HoldAlpha;
		TestTrue(TEXT("never rises again"), Next <= Alpha + 1e-4f);
		Alpha = Next;
		++Frames;
	}
	TestEqual(TEXT("live pose at the end"), Alpha, 0.f);
	const float Expected = (Settings.HoldUntil + Settings.BlendOut) / Dt - 10.f;
	TestTrue(TEXT("lasts HoldUntil + BlendOut from the touchdown"), FMath::Abs(Frames - Expected) <= 3.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CoveredLandingCasesTest, "GSTNAnimation.Core.CoveredLanding.Cases", GSTNCoveredLandingTest::TestFlags)
bool FGSTN_CoveredLandingCasesTest::RunTest(const FString& Parameters)
{
	using namespace GSTNCoveredLandingTest;
	FGSTN_CoveredLandingSettings Settings;

	// A plain landing (no montage): the base layer's landing is the landing, nothing held
	FGSTN_CoveredLandingLogic Plain;
	TestFalse(TEXT("uncovered: no capture"), FallAndLand(Plain, Settings, false).bCapture);
	TestEqual(TEXT("uncovered: no window"), Plain.GetWindowTime(), -1.f);

	// The touchdown frame lifts the actor (grounded capsule restore): an upward spike is still a touchdown
	FGSTN_CoveredLandingLogic Spike;
	for (int32 Frame = 0; Frame < 3; ++Frame)
	{
		Spike.Step(Settings, Dt, -600.f, 0.f, true, 1.f);
	}
	TestTrue(TEXT("upward spike on contact: touchdown"), Spike.Step(Settings, Dt, 3000.f, 0.f, true, 1.f).bCapture);

	// A jump's apex is not a touchdown (never fell fast)
	FGSTN_CoveredLandingLogic Apex;
	Apex.Step(Settings, Dt, 300.f, 0.f, true, 1.f);
	Apex.Step(Settings, Dt, 0.f, 0.f, true, 1.f);
	TestEqual(TEXT("apex: no window"), Apex.GetWindowTime(), -1.f);

	// Another montage after the covering one: it owns the pose, the window ends
	FGSTN_CoveredLandingLogic Next;
	FallAndLand(Next, Settings, true);
	Next.Step(Settings, Dt, 0.f, 0.f, false, 0.5f);
	const FGSTN_CoveredLandingOutput Other = Next.Step(Settings, Dt, 0.f, 0.f, true, 1.f);
	TestFalse(TEXT("new montage: no capture"), Other.bCapture);
	TestEqual(TEXT("new montage: window closed"), Next.GetWindowTime(), -1.f);

	// Walking away from the landing: the live pose comes back within BlendOut
	FGSTN_CoveredLandingLogic Walk;
	FallAndLand(Walk, Settings, true);
	Walk.Step(Settings, Dt, 0.f, 0.f, false, 0.5f);
	float Alpha = 1.f;
	for (int32 Frame = 0; Frame <= FMath::CeilToInt(Settings.BlendOut / Dt) + 1; ++Frame)
	{
		Alpha = Walk.Step(Settings, Dt, 0.f, 300.f, false, 0.f).HoldAlpha;
	}
	TestEqual(TEXT("walking: live pose after BlendOut"), Alpha, 0.f);

	// Off: nothing
	Settings.bEnabled = false;
	FGSTN_CoveredLandingLogic Off;
	TestFalse(TEXT("disabled: no capture"), FallAndLand(Off, Settings, true).bCapture);
	return true;
}

#endif
