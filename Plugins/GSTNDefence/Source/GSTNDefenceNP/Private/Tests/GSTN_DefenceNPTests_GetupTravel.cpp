#include "Misc/AutomationTest.h"
#include "Abilities/GSTN_DefenceAbilities.h"
#include "Sim/GSTN_TumbleLanding.h"
#include "Timeline/GSTN_TimelineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags GetupTravelTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	TArray<FGSTN_GetupTravelKey> MakeTravelKeys(std::initializer_list<TPair<int32, float>> Points)
	{
		TArray<FGSTN_GetupTravelKey> Out;
		for (const TPair<int32, float>& Point : Points)
		{
			FGSTN_GetupTravelKey Key;
			Key.Frame = Point.Key;
			Key.Fraction = Point.Value;
			Out.Add(Key);
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceGetupTravelSegmentsTest, "GSTNDefence.NP.GetupRoll.TravelSegments", GetupTravelTestFlags)
bool FGSTN_DefenceGetupTravelSegmentsTest::RunTest(const FString& Parameters)
{
	// The forward rock-up of roll-travel.md: still until 6, 70 % by 10, all by 16
	const TArray<FGSTN_GetupTravelKey> Roll = MakeTravelKeys({ { 0, 0.f }, { 6, 0.f }, { 10, 0.7f }, { 16, 1.f } });
	float Speed = 0.f;
	int32 Frames = 0;
	TestFalse(TEXT("frame 0: the hold has no push"), UGSTN_GA_GetupRoll::SegmentStartingAt(Roll, 80.f, 0, Speed, Frames));
	TestTrue(TEXT("frame 6: the main push"), UGSTN_GA_GetupRoll::SegmentStartingAt(Roll, 80.f, 6, Speed, Frames));
	TestEqual(TEXT("frame 6: four frames"), Frames, 4);
	TestEqual(TEXT("frame 6: 56 cm over the segment"), Speed * Frames * FGSTN_TumbleLanding::FrameSeconds, 56.f, 0.01f);
	TestTrue(TEXT("frame 10: the ease out"), UGSTN_GA_GetupRoll::SegmentStartingAt(Roll, 80.f, 10, Speed, Frames));
	TestEqual(TEXT("frame 10: 24 cm over the segment"), Speed * Frames * FGSTN_TumbleLanding::FrameSeconds, 24.f, 0.01f);
	TestFalse(TEXT("frame 8: inside a segment, nothing starts"), UGSTN_GA_GetupRoll::SegmentStartingAt(Roll, 80.f, 8, Speed, Frames));
	TestFalse(TEXT("frame 16: the last key starts nothing"), UGSTN_GA_GetupRoll::SegmentStartingAt(Roll, 80.f, 16, Speed, Frames));
	TestFalse(TEXT("no distance: in place"), UGSTN_GA_GetupRoll::SegmentStartingAt(Roll, 0.f, 6, Speed, Frames));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceGetupExitTest, "GSTNDefence.NP.Getup.ExitMomentum", GetupTravelTestFlags)
bool FGSTN_DefenceGetupExitTest::RunTest(const FString& Parameters)
{
	// Owner 2026-11-21: only a get-up cut in its tail passes momentum on; a tail played out, a hit or a finished move leave the body at rest
	const FVector Along(0.0, 2.0, 0.5);
	TestEqual(TEXT("tail cancelled: speed along the flattened direction"), FGSTN_GetupExit::ExitVelocity(EGSTN_MoveEndReason::TailCancelled, Along, 220.f), FVector(0.0, 220.0, 0.0));
	TestTrue(TEXT("finished: nothing"), FGSTN_GetupExit::ExitVelocity(EGSTN_MoveEndReason::Finished, Along, 220.f).IsZero());
	TestTrue(TEXT("interrupted (a hit): nothing"), FGSTN_GetupExit::ExitVelocity(EGSTN_MoveEndReason::Interrupted, Along, 220.f).IsZero());
	TestTrue(TEXT("no speed: nothing"), FGSTN_GetupExit::ExitVelocity(EGSTN_MoveEndReason::TailCancelled, Along, 0.f).IsZero());
	TestEqual(TEXT("cvar -1: the data"), FGSTN_GetupExit::ResolveSpeed(160.f, -1.f), 160.f);
	TestEqual(TEXT("cvar 0: off"), FGSTN_GetupExit::ResolveSpeed(160.f, 0.f), 0.f);
	TestEqual(TEXT("cvar set: the cvar"), FGSTN_GetupExit::ResolveSpeed(160.f, 330.f), 330.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceGetupSameSpeedTest, "GSTNDefence.NP.Getup.SharedExitSpeed", GetupTravelTestFlags)
bool FGSTN_DefenceGetupSameSpeedTest::RunTest(const FString& Parameters)
{
	// Owner 2026-11-23: every get-up hands on the same speed (gstn.Getup.ExitSpeed); a move's own knob still wins when set
	TestEqual(TEXT("shared knob over each move's data (220 / 160 / 120)"), FGSTN_GetupExit::ResolveSpeed(220.f, -1.f, 160.f), 160.f);
	TestEqual(TEXT("same for the roll back"), FGSTN_GetupExit::ResolveSpeed(160.f, -1.f, 160.f), 160.f);
	TestEqual(TEXT("same for the stand-up"), FGSTN_GetupExit::ResolveSpeed(120.f, -1.f, 160.f), 160.f);
	TestEqual(TEXT("shared -1: the data again"), FGSTN_GetupExit::ResolveSpeed(220.f, -1.f, -1.f), 220.f);
	TestEqual(TEXT("the move's own knob wins"), FGSTN_GetupExit::ResolveSpeed(220.f, 300.f, 160.f), 300.f);
	TestEqual(TEXT("shared 0: no momentum"), FGSTN_GetupExit::ResolveSpeed(220.f, -1.f, 0.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DefenceGetupWarpTest, "GSTNDefence.NP.Getup.DirectionalWarp", GetupTravelTestFlags)
bool FGSTN_DefenceGetupWarpTest::RunTest(const FString& Parameters)
{
	// Owner 2026-11-23: the roll turns toward the stick while the body is still curled on its back, then travels along the new facing
	TestEqual(TEXT("forward rock-up: the turn ends where the travel starts (6)"),
		FGSTN_GetupShape::FirstTravelFrame(MakeTravelKeys({ { 0, 0.f }, { 6, 0.f }, { 10, 0.7f }, { 16, 1.f } })), 6);
	TestEqual(TEXT("roll back: 4"), FGSTN_GetupShape::FirstTravelFrame(MakeTravelKeys({ { 0, 0.f }, { 4, 0.f }, { 12, 1.f } })), 4);
	TestEqual(TEXT("travel from frame 0: no room to turn"), FGSTN_GetupShape::FirstTravelFrame(MakeTravelKeys({ { 0, 0.f }, { 8, 1.f } })), 0);
	TestEqual(TEXT("no travel: none"), FGSTN_GetupShape::FirstTravelFrame(MakeTravelKeys({ { 0, 0.f }, { 8, 0.f } })), static_cast<int32>(INDEX_NONE));

	FGSTN_CombatTimelineData Timeline;
	FGSTN_GetupShape::ApplyWarp(Timeline, 45.f, 6, false, FGSTN_TumbleLanding::FrameSeconds);
	TestEqual(TEXT("cap 45"), Timeline.MaxSteerDegrees, 45.f);
	TestEqual(TEXT("the whole cap in the 6 frames: rate * 6 frames = 45"), Timeline.SteerRateDegPerSecond * 6.f * FGSTN_TumbleLanding::FrameSeconds, 45.f, 0.01f);
	TestTrue(TEXT("the move stick steers"), Timeline.Steer.Source == EGSTN_SteerSource::MoveStick);
	TestEqual(TEXT("window [0, 6)"), Timeline.Steer.EndFrame, 6);
	TestFalse(TEXT("toward the feet: faces the stick"), Timeline.Steer.bAwayFromStick);
	FGSTN_GetupShape::ApplyWarp(Timeline, 120.f, 4, true, FGSTN_TumbleLanding::FrameSeconds);
	TestEqual(TEXT("the cap never passes 90 (the forward / back split covers the rest)"), Timeline.MaxSteerDegrees, 90.f);
	TestTrue(TEXT("toward the head: the back faces the stick"), Timeline.Steer.bAwayFromStick);
	FGSTN_GetupShape::ApplyWarp(Timeline, 0.f, 6, false, FGSTN_TumbleLanding::FrameSeconds);
	TestEqual(TEXT("off: no steer at all"), Timeline.MaxSteerDegrees, 0.f);
	FGSTN_GetupShape::ApplyWarp(Timeline, 45.f, 0, false, FGSTN_TumbleLanding::FrameSeconds);
	TestEqual(TEXT("no frames: no steer (never a snap)"), Timeline.MaxSteerDegrees, 0.f);

	TestTrue(TEXT("knob -1: the data"), FGSTN_GetupShape::ResolveFlag(true, -1));
	TestFalse(TEXT("knob 0: off"), FGSTN_GetupShape::ResolveFlag(true, 0));
	TestTrue(TEXT("knob 1: on"), FGSTN_GetupShape::ResolveFlag(false, 1));
	TestEqual(TEXT("value knob -1: the data"), FGSTN_GetupShape::ResolveValue(45.f, -1.f), 45.f);
	TestEqual(TEXT("value knob 0: off"), FGSTN_GetupShape::ResolveValue(45.f, 0.f), 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
