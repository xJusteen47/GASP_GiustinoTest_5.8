// Automation tests of the run stop (stick let go during a run, no reverse input: owner 2026-11-18). Pure: no world.

#include "Misc/AutomationTest.h"
#include "Presentation/GSTN_LocomotionLean.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_RSTOP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNRunStopTest
{
	/** A stop clip like GASP's: 0.7 s at 500 cm/s, a linear brake to rest over 0.3 s, then still */
	FGSTN_RunStopTrack MakeTrack()
	{
		TArray<FVector> Positions;
		float Y = 0.f;
		for (int32 i = 0; i <= 120; ++i)
		{
			const float T = i / 60.f;
			const float V = T < 0.7f ? 500.f : FMath::Max(500.f * (1.f - (T - 0.7f) / 0.3f), 0.f);
			Positions.Add(FVector(0.f, Y, 0.f));
			Y += V / 60.f;
		}
		FGSTN_RunStopTrack Track;
		Track.Build(Positions, 60.f);
		return Track;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunStopTrackTest, "GSTNCombat.NP.RunStop.Track", GSTN_RSTOP_TEST_FLAGS)
bool FGSTN_RunStopTrackTest::RunTest(const FString& Parameters)
{
	const FGSTN_RunStopTrack Track = GSTNRunStopTest::MakeTrack();
	TestTrue(TEXT("valid"), Track.IsValid());
	TestTrue(TEXT("run speed ~500"), FMath::IsNearlyEqual(Track.RunSpeed, 500.f, 15.f));
	TestTrue(TEXT("brake starts near 0.7 s"), FMath::Abs(Track.BrakeStartTime() - 0.7f) < 0.06f);
	TestTrue(TEXT("stop near 1.0 s"), FMath::Abs(Track.StopTime() - 1.f) < 0.06f);
	const float Half = Track.TimeForSpeedFraction(0.5f);
	TestTrue(TEXT("half speed mid brake"), FMath::Abs(Half - 0.85f) < 0.04f);
	TestTrue(TEXT("monotonic"), Track.TimeForSpeedFraction(0.8f) < Half && Half < Track.TimeForSpeedFraction(0.2f));
	TestTrue(TEXT("zero speed = stop"), FMath::IsNearlyEqual(Track.TimeForSpeedFraction(0.f), Track.StopTime(), 0.02f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunStopStateTest, "GSTNCombat.NP.RunStop.State", GSTN_RSTOP_TEST_FLAGS)
bool FGSTN_RunStopStateTest::RunTest(const FString& Parameters)
{
	const FGSTN_RunStopTrack Track = GSTNRunStopTest::MakeTrack();
	const FGSTN_RunStopSettings S;
	const float Dt = 1.f / 60.f;
	auto Run = [&](FGSTN_RunStopState& State, float Speed, float Stick, bool bBlocked = false)
	{
		State.Step(S, EGSTN_RunStopMode::Clip, Dt, Speed, Stick, bBlocked, true, &Track, &Track);
	};

	// A run, then the stick let go: the stop waits ConfirmFrames, then plays with the sim's brake (450 -> 0 in 7 frames)
	FGSTN_RunStopState State;
	for (int32 i = 0; i < 10; ++i)
	{
		Run(State, 450.f, 1.f);
	}
	TestFalse(TEXT("no stop while running"), State.IsClip());
	float Speed = 450.f;
	int32 Frames = 0;
	while (!State.IsClip() && Frames < 10)
	{
		Speed = FMath::Max(Speed - 4000.f * Dt, 0.f);
		Run(State, Speed, 0.f);
		++Frames;
	}
	TestTrue(TEXT("stop starts after the confirm frames"), State.IsClip() && Frames == S.ConfirmFrames + 1);
	float Previous = State.ClipTime;
	bool bMonotonic = true;
	for (int32 i = 0; i < 10; ++i)
	{
		Speed = FMath::Max(Speed - 4000.f * Dt, 0.f);
		Run(State, Speed, 0.f);
		bMonotonic &= State.ClipTime >= Previous;
		Previous = State.ClipTime;
	}
	TestTrue(TEXT("clip time never goes back"), bMonotonic);
	TestTrue(TEXT("at rest the clip is past its stop"), State.ClipTime >= Track.StopTime());
	// Settle, then idle
	for (int32 i = 0; i < 120 && State.IsClip(); ++i)
	{
		Run(State, 0.f, 0.f);
	}
	TestFalse(TEXT("the stop hands over after the settle"), State.IsClip());
	Run(State, 0.f, 0.f);
	TestFalse(TEXT("one stop per release"), State.IsClip());

	// The stick pushed again ends it at once
	FGSTN_RunStopState Again;
	for (int32 i = 0; i < 5; ++i) { Run(Again, 450.f, 1.f); }
	for (int32 i = 0; i < 4; ++i) { Run(Again, 350.f, 0.f); }
	TestTrue(TEXT("stopping"), Again.IsClip());
	Run(Again, 350.f, 1.f);
	TestFalse(TEXT("stick pushed: stop ends"), Again.IsClip());

	// A body that keeps sliding with the stick let go does not hold the brake forever
	FGSTN_RunStopState Slide;
	for (int32 i = 0; i < 5; ++i) { Run(Slide, 450.f, 1.f); }
	for (int32 i = 0; i < 200 && (i < 4 || Slide.IsClip()); ++i) { Run(Slide, 267.f, 0.f); }
	TestFalse(TEXT("constant slide: the stop still ends"), Slide.IsClip());

	// A walk never stops this way; a skid (blocked) never does either
	FGSTN_RunStopState Walk;
	for (int32 i = 0; i < 5; ++i) { Run(Walk, 150.f, 0.5f); }
	for (int32 i = 0; i < 5; ++i) { Run(Walk, 100.f, 0.f); }
	TestFalse(TEXT("walk: no stop"), Walk.IsClip());
	FGSTN_RunStopState Skid;
	for (int32 i = 0; i < 5; ++i) { Run(Skid, 450.f, 1.f); }
	for (int32 i = 0; i < 5; ++i) { Run(Skid, 400.f, 0.f, true); }
	for (int32 i = 0; i < 5; ++i) { Run(Skid, 0.f, 0.f); }
	TestFalse(TEXT("skid: no stop, not even after it"), Skid.IsClip());
	return true;
}

#endif
