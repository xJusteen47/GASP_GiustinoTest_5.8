// Automation tests of the run start scramble and the three-section skid (docs/notes/mosse/run-start-stop-skid-brief.md). Pure: no world.

#include "Misc/AutomationTest.h"
#include "Presentation/GSTN_LocomotionLean.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_RSS_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_SkidSectionsTest, "GSTNCombat.NP.RunStartSkid.SkidSections", GSTN_RSS_TEST_FLAGS)
bool FGSTN_SkidSectionsTest::RunTest(const FString& Parameters)
{
	const FGSTN_SkidSectionSettings S;
	const float Length = 1.f;
	const float Hold = Length * S.HoldStartFraction;
	const float Push = Length * S.PushStartFraction;

	TestEqual(TEXT("no phase, no time"), S.ClipTime(EGSTN_RunSkidPhase::None, 3, 20, 4, 8, 100.f, 600.f, Length), 0.f);
	TestTrue(TEXT("entry ends on the hold whatever the speed"),
		FMath::IsNearlyEqual(S.ClipTime(EGSTN_RunSkidPhase::Enter, 3, 20, 4, 8, 500.f, 600.f, Length), Hold, 1e-4f));
	TestTrue(TEXT("entry is the same from a faster run"),
		FMath::IsNearlyEqual(S.ClipTime(EGSTN_RunSkidPhase::Enter, 1, 30, 4, 8, 800.f, 900.f, Length),
			S.ClipTime(EGSTN_RunSkidPhase::Enter, 1, 15, 4, 8, 400.f, 500.f, Length), 1e-4f));

	// Hold: driven by the speed left, so the same share of the brake gives the same pose at any run stage
	const float HalfRun = S.ClipTime(EGSTN_RunSkidPhase::Active, 8, 15, 4, 8, 250.f, 500.f, Length);
	const float HalfSprint = S.ClipTime(EGSTN_RunSkidPhase::Active, 14, 30, 4, 8, 450.f, 900.f, Length);
	TestTrue(TEXT("hold on the speed left, not the frame"), FMath::IsNearlyEqual(HalfRun, HalfSprint, 1e-4f));
	TestTrue(TEXT("hold stays inside its section"), HalfRun > Hold && HalfRun < Push);
	TestTrue(TEXT("ease-out: past the middle of the section at half the speed"), HalfRun > Hold + (Push - Hold) * 0.5f);
	TestTrue(TEXT("at rest the hold reaches the push"),
		FMath::IsNearlyEqual(S.ClipTime(EGSTN_RunSkidPhase::Active, 20, 20, 4, 8, 0.f, 600.f, Length), Push, 1e-4f));

	// Push: fixed length over the exit frames, ending on the clip's end
	TestTrue(TEXT("push starts at its section"), S.ClipTime(EGSTN_RunSkidPhase::Exit, 20, 20, 4, 8, 0.f, 600.f, Length) >= Push);
	TestTrue(TEXT("push ends on the clip's end on the last exit frame"),
		FMath::IsNearlyEqual(S.ClipTime(EGSTN_RunSkidPhase::Exit, 27, 20, 4, 8, 0.f, 600.f, Length), Length, 1e-4f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunStartScrambleTest, "GSTNCombat.NP.RunStartSkid.Scramble", GSTN_RSS_TEST_FLAGS)
bool FGSTN_RunStartScrambleTest::RunTest(const FString& Parameters)
{
	const FGSTN_RunStartScrambleSettings S;
	const float Dt = 0.016f;
	const float Target = 450.f;

	FGSTN_RunStartScrambleState Walk;
	Walk.Step(S, Dt, 0.f, Target, 0.3f, true, false, true);
	Walk.Step(S, Dt, 40.f, Target, 0.3f, true, false, true);
	TestFalse(TEXT("a light stick (walk) does not scramble"), Walk.bActive);

	FGSTN_RunStartScrambleState NoClip;
	NoClip.Step(S, Dt, 0.f, Target, 1.f, true, false, false);
	NoClip.Step(S, Dt, 40.f, Target, 1.f, true, false, false);
	TestFalse(TEXT("no run cycle, no scramble"), NoClip.bActive);

	FGSTN_RunStartScrambleState Run;
	Run.Step(S, Dt, 0.f, Target, 1.f, true, false, true);
	Run.Step(S, Dt, 40.f, Target, 1.f, true, false, true);
	TestTrue(TEXT("starting from still with a full stick scrambles"), Run.bActive);
	TestTrue(TEXT("fast at the start (rate near 1 + K)"), Run.PlayRate(S) > 1.f + S.PlayRateBoost * 0.85f);

	float MaxLean = 0.f;
	float Speed = 40.f;
	for (int32 Frame = 0; Frame < 12 && Run.bActive; ++Frame)
	{
		Speed += 30.f;
		Run.Step(S, Dt, Speed, Target, 1.f, true, false, true);
		MaxLean = FMath::Max(MaxLean, Run.LeanDeg);
	}
	TestTrue(TEXT("the extra lean comes in"), MaxLean > S.ExtraLeanDeg * 0.4f);
	TestTrue(TEXT("the rate falls as the speed builds"), Run.PlayRate(S) < 1.f + S.PlayRateBoost * 0.85f);

	Run.Step(S, Dt, Target * 0.95f, Target, 1.f, true, false, true);
	TestFalse(TEXT("ends near the run speed"), Run.bActive);
	float MinLean = Run.LeanDeg;
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Run.Step(S, Dt, Target, Target, 1.f, true, false, true);
		MinLean = FMath::Min(MinLean, Run.LeanDeg);
	}
	TestTrue(TEXT("one small rebound past the run lean"), MinLean < 0.f && MinLean > -4.f);
	TestTrue(TEXT("then settles"), FMath::Abs(Run.LeanDeg) < 0.2f);

	FGSTN_RunStartScrambleState Skid;
	Skid.Step(S, Dt, 0.f, Target, 1.f, true, false, true);
	Skid.Step(S, Dt, 40.f, Target, 1.f, true, false, true);
	Skid.Step(S, Dt, 80.f, Target, 1.f, true, true, true);
	TestFalse(TEXT("a skid stops the scramble"), Skid.bActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunStartScrambleLowFpsTest, "GSTNCombat.NP.RunStartSkid.ScrambleLowFps", GSTN_RSS_TEST_FLAGS)
bool FGSTN_RunStartScrambleLowFpsTest::RunTest(const FString& Parameters)
{
	// bugpass-1 B1: the extra lean spring must stay bounded at low frame rates, with the default and with the stiffest allowed spring
	for (const float Frequency : { 6.f, 15.f })
	{
		FGSTN_RunStartScrambleSettings S;
		S.LeanSpringFrequency = Frequency;
		for (const float Dt : { 1.f / 30.f, 1.f / 20.f })
		{
			const float Target = 450.f;
			FGSTN_RunStartScrambleState Run;
			Run.Step(S, Dt, 0.f, Target, 1.f, true, false, true);
			float Speed = 40.f;
			float MaxAbs = 0.f;
			for (int32 Frame = 0; Frame < 120; ++Frame)
			{
				Speed = FMath::Min(Speed + 30.f, Target);
				Run.Step(S, Dt, Speed, Target, 1.f, true, false, true);
				MaxAbs = FMath::Max(MaxAbs, FMath::Abs(Run.LeanDeg));
			}
			TestTrue(FString::Printf(TEXT("bounded at %.0f Hz, %.0f fps"), Frequency, 1.f / Dt), FMath::IsFinite(MaxAbs) && MaxAbs <= S.ExtraLeanDeg * 1.5f + 0.1f);
			TestTrue(FString::Printf(TEXT("settles at %.0f Hz, %.0f fps"), Frequency, 1.f / Dt), FMath::Abs(Run.LeanDeg) < 0.2f);
		}
	}
	return true;
}

#endif
