// Hold Until Landed (dropkick, 2026-11-15): the clock rule and the montage rule of FGSTN_TimelineTrack_HoldUntilLanded, pure.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Timeline/GSTN_HoldUntilLanded.h"
#include "Timeline/GSTN_TimelineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNHoldUntilLandedTests
{
	// The Overlord dropkick as built by its kit script: startup 16 frames, the loop 11 frames (written 4 times in the montage), land from frame 60
	constexpr int32 LoopStart = 16;
	constexpr int32 LoopEnd = 27;
	constexpr int32 Land = 60;
	// One 60 fps animation frame per 62.5 Hz tick
	constexpr float SecondsPerFrame = 0.016f * (62.5f / 60.f);

	FGSTN_CombatTimelineData MakeDropkick(int32 LandFrame = Land, int32 LandFromFrame = 0)
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 92;
		Data.FirstActionableFrame = 91;
		return UGSTN_TimelineUtils::SetHoldUntilLandedTrack(Data, LoopStart, LoopEnd, LandFrame, LandFromFrame);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HoldUntilLandedClockTest, "GSTNCombat.Core.HoldUntilLanded.Clock", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HoldUntilLandedClockTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHoldUntilLandedTests;
	const FGSTN_CombatTimelineData Data = MakeDropkick();
	int32 Index = INDEX_NONE;
	TestNotNull(TEXT("the authoring helper adds the track"), FGSTN_HoldUntilLanded::FindTrack(Data, &Index));
	TestEqual(TEXT("as the only track"), Index, 0);
	TestEqual(TEXT("a second call replaces it"), UGSTN_TimelineUtils::SetHoldUntilLandedTrack(Data, 1, 2).Tracks.Num(), 1);
	TestEqual(TEXT("and it can be removed"), UGSTN_TimelineUtils::RemoveHoldUntilLandedTrack(Data).Tracks.Num(), 0);

	auto Action = [&Data](int32 Current, bool bKnown, bool bGround) { return FGSTN_HoldUntilLanded::ResolveClock(Data, Current, bKnown, bGround).Action; };

	// No ground fact (no Mover): the timeline plays straight through
	TestTrue(TEXT("unknown ground: nothing on the last hold frame"), Action(LoopEnd - 1, false, false) == EGSTN_HoldClockAction::None);
	TestTrue(TEXT("unknown ground: nothing in the startup"), Action(5, false, true) == EGSTN_HoldClockAction::None);

	// In the air: plays into the hold, stands on its last frame
	TestTrue(TEXT("air, startup: plays on"), Action(5, true, false) == EGSTN_HoldClockAction::None);
	TestTrue(TEXT("air, inside the hold: plays on"), Action(LoopStart + 2, true, false) == EGSTN_HoldClockAction::None);
	TestTrue(TEXT("air, last hold frame: stands"), Action(LoopEnd - 1, true, false) == EGSTN_HoldClockAction::Stall);

	// On the ground: the land part starts now, from the startup or the hold
	const FGSTN_HoldClockDecision FromHold = FGSTN_HoldUntilLanded::ResolveClock(Data, LoopEnd - 1, true, true);
	TestTrue(TEXT("ground on the held frame: lands"), FromHold.Action == EGSTN_HoldClockAction::Land);
	TestEqual(TEXT("on the land frame"), FromHold.TargetFrame, Land);
	TestEqual(TEXT("naming the track"), FromHold.TrackIndex, 0);
	TestTrue(TEXT("ground in the startup: lands too (LandFromFrame 0)"), Action(3, true, true) == EGSTN_HoldClockAction::Land);
	TestTrue(TEXT("ground after the land frame: plays on"), Action(Land, true, true) == EGSTN_HoldClockAction::None);
	TestTrue(TEXT("ground past the hold (it landed already): plays on"), Action(LoopEnd + 3, true, true) == EGSTN_HoldClockAction::None);

	// Land frame = EndFrame: the held frame steps onto it by itself, no jump
	const FGSTN_CombatTimelineData Adjacent = MakeDropkick(-1);
	TestTrue(TEXT("land frame -1 = the end: the next frame is the land frame, no jump"), FGSTN_HoldUntilLanded::ResolveClock(Adjacent, LoopEnd - 1, true, true).Action == EGSTN_HoldClockAction::None);
	TestEqual(TEXT("a landing earlier jumps to the end frame"), FGSTN_HoldUntilLanded::ResolveClock(Adjacent, LoopStart, true, true).TargetFrame, LoopEnd);

	// LandFromFrame: a move that starts on the ground does not land out of its first frames
	const FGSTN_CombatTimelineData Late = MakeDropkick(Land, 10);
	TestTrue(TEXT("before LandFromFrame the ground is ignored"), FGSTN_HoldUntilLanded::ResolveClock(Late, 5, true, true).Action == EGSTN_HoldClockAction::None);
	TestTrue(TEXT("from it the ground lands"), FGSTN_HoldUntilLanded::ResolveClock(Late, 9, true, true).Action == EGSTN_HoldClockAction::Land);

	TestTrue(TEXT("a timeline without the track: nothing"), FGSTN_HoldUntilLanded::ResolveClock(FGSTN_CombatTimelineData(), 3, true, true).Action == EGSTN_HoldClockAction::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HoldUntilLandedMontageTest, "GSTNCombat.Core.HoldUntilLanded.Montage", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HoldUntilLandedMontageTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHoldUntilLandedTests;
	constexpr float Tolerance = 1.e-4f;
	const FGSTN_CombatTimelineData Data = MakeDropkick();
	const FGSTN_TimelineTrack_HoldUntilLanded* Track = FGSTN_HoldUntilLanded::FindTrack(Data);
	if (!TestNotNull(TEXT("track"), Track))
	{
		return false;
	}

	// Without sections the frames map as at the start of the move
	const FGSTN_HoldMontageTimes Times = FGSTN_HoldUntilLanded::ResolveMontageTimes(*Track, nullptr, 0.f, SecondsPerFrame);
	TestEqual(TEXT("loop start = frame 16"), Times.LoopStart, LoopStart * SecondsPerFrame, Tolerance);
	TestEqual(TEXT("loop end = frame 27"), Times.LoopEnd, LoopEnd * SecondsPerFrame, Tolerance);
	TestEqual(TEXT("land = frame 60"), Times.LandStart, Land * SecondsPerFrame, Tolerance);
	TestEqual(TEXT("a section start offsets them"), FGSTN_HoldUntilLanded::ResolveMontageTimes(*Track, nullptr, 0.5f, SecondsPerFrame).LoopStart, 0.5f + LoopStart * SecondsPerFrame, Tolerance);

	// The startup plays untouched
	TestTrue(TEXT("startup: nothing"), FGSTN_HoldUntilLanded::ResolveMontage(*Track, Times, 10, 9 * SecondsPerFrame, SecondsPerFrame, SecondsPerFrame).Action == EGSTN_HoldMontageAction::None);

	// Held on frame 26, the montage loops: a tick that would reach the loop's end goes back by one loop
	const float NearEnd = (LoopEnd - 1) * SecondsPerFrame;
	const FGSTN_HoldMontageDecision Wrap = FGSTN_HoldUntilLanded::ResolveMontage(*Track, Times, LoopEnd - 1, NearEnd, SecondsPerFrame, SecondsPerFrame);
	TestTrue(TEXT("the last loop frame wraps"), Wrap.Action == EGSTN_HoldMontageAction::Wrap);
	TestEqual(TEXT("back by one loop length"), Wrap.Time, NearEnd - (LoopEnd - LoopStart) * SecondsPerFrame, Tolerance);
	TestTrue(TEXT("after this tick's advance it is the loop's first frame"), FMath::IsNearlyEqual(Wrap.Time + SecondsPerFrame, Times.LoopStart, Tolerance));
	TestTrue(TEXT("mid loop: nothing"), FGSTN_HoldUntilLanded::ResolveMontage(*Track, Times, LoopEnd - 1, (LoopStart + 3) * SecondsPerFrame, SecondsPerFrame, SecondsPerFrame).Action == EGSTN_HoldMontageAction::None);
	// 30 ticks of a fall: the montage never leaves the loop
	float Time = (LoopEnd - 1) * SecondsPerFrame;
	bool bInside = true;
	for (int32 Tick = 0; Tick < 30; ++Tick)
	{
		const FGSTN_HoldMontageDecision Step = FGSTN_HoldUntilLanded::ResolveMontage(*Track, Times, LoopEnd - 1, Time, SecondsPerFrame, SecondsPerFrame);
		Time = (Step.Action == EGSTN_HoldMontageAction::Wrap ? Step.Time : Time) + SecondsPerFrame;
		bInside &= Time >= Times.LoopStart - Tolerance && Time < Times.LoopEnd - Tolerance;
	}
	TestTrue(TEXT("a long fall stays inside the loop"), bInside);

	// The clock jumped to the land frame: the montage jumps to the land part, wherever it was
	const FGSTN_HoldMontageDecision Landed = FGSTN_HoldUntilLanded::ResolveMontage(*Track, Times, Land, (LoopStart + 4) * SecondsPerFrame, SecondsPerFrame, SecondsPerFrame);
	TestTrue(TEXT("landed: jump"), Landed.Action == EGSTN_HoldMontageAction::Land);
	TestEqual(TEXT("to the land part's start"), Landed.Time, Times.LandStart, Tolerance);
	TestTrue(TEXT("once there: nothing"), FGSTN_HoldUntilLanded::ResolveMontage(*Track, Times, Land + 1, Times.LandStart, SecondsPerFrame, SecondsPerFrame).Action == EGSTN_HoldMontageAction::None);
	TestEqual(TEXT("a late call lands where the timeline is"), FGSTN_HoldUntilLanded::ResolveMontage(*Track, Times, Land + 2, 0.2f, SecondsPerFrame, SecondsPerFrame).Time,
		Times.LandStart + 2 * SecondsPerFrame, Tolerance);
	return true;
}

#endif
