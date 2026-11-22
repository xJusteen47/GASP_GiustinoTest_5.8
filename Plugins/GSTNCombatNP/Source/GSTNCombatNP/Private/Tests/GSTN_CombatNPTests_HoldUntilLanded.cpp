// Hold Until Landed in the combat clock (dropkick, 2026-11-15): the timeline stands on the hold's last frame while the ground fact says air, jumps
// to the land frame on the first grounded frame (skipped keys do not fire), and the negative paused count the jump can leave goes over the wire.
// Pure: sync state in, sync state out, the ground fact lent through the hook environment as the combat tick lends Mover's.

#include "Misc/AutomationTest.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "Simulation/GSTN_TimelineTrackHook.h"
#include "Timeline/GSTN_HoldUntilLanded.h"
#include "Timeline/GSTN_TimelineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNHoldClockTests
{
	constexpr int32 LoopStart = 16;
	constexpr int32 LoopEnd = 27;
	constexpr int32 Land = 60;
	constexpr int32 SkippedKey = 40;
	constexpr int32 LandKey = 61;

	/** The dropkick's shape: startup 16, held loop [16, 27), land from 60, plus a key inside the skipped frames and one in the land part */
	FGSTN_CombatTimelineData MakeData()
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 92;
		Data.FirstActionableFrame = 91;
		Data.OverrideFrames = 0;
		for (const int32 KeyFrame : { SkippedKey, LandKey })
		{
			FGSTN_TimelineTrack_Event Key;
			Key.StartFrame = KeyFrame;
			Key.EndFrame = KeyFrame + 1;
			Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Event>(Key));
		}
		return UGSTN_TimelineUtils::SetHoldUntilLandedTrack(Data, LoopStart, LoopEnd, Land);
	}

	struct FRun
	{
		FGSTN_CombatSyncCore Core;
		FGSTN_TimelineOutput Output;
		FGSTN_TrackHookEnvironment Environment;
		int32 Frame = 0;
		TArray<int32> KeysFired;

		FRun(const FGSTN_CombatTimelineData& Data, int32 StartFrame)
		{
			Output.Reserve(Data);
			Frame = StartFrame;
			FGSTN_CombatTimelineLogic::Start(Core, StartFrame, 3, EGSTN_ActivationSource::Command);
			FGSTN_CombatTimelineLogic::EvaluateStart(Core, StartFrame, Data, 0, Output, &Environment);
		}

		/** One tick with the ground fact given; returns the timeline frame it evaluated */
		int32 Tick(const FGSTN_CombatTimelineData& Data, EGSTN_TrackHookGround Ground)
		{
			++Frame;
			Environment.Ground = Ground;
			Core.Requests.PruneBefore(Frame);
			FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 0, true, Output, false, nullptr, &Environment);
			for (const FGSTN_TimelineTrack_Event* Key : Output.Events)
			{
				KeysFired.Add(Key ? Key->StartFrame : INDEX_NONE);
			}
			return Output.Frame;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HoldClockFallTest, "GSTNCombat.NP.HoldUntilLanded.Fall", GSTN_NP_TEST_FLAGS)
bool FGSTN_HoldClockFallTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHoldClockTests;
	const FGSTN_CombatTimelineData Data = MakeData();

	// A long fall: 50 frames in the air, the timeline plays to 26 and stands there
	FRun Run(Data, 100);
	int32 Last = 0;
	for (int32 Index = 0; Index < 50; ++Index)
	{
		Last = Run.Tick(Data, EGSTN_TrackHookGround::Airborne);
	}
	TestEqual(TEXT("the fall holds the hold's last frame"), Last, LoopEnd - 1);
	TestEqual(TEXT("standing for every frame past it"), Run.Core.TimelinePausedFrames, 50 - (LoopEnd - 1));
	TestTrue(TEXT("still running"), Run.Core.ActionState == EGSTN_ActionState::InTimeline);

	// Touchdown: the land frame on this very tick, the skipped key silent, the land part's key on its frame
	TestEqual(TEXT("the first grounded frame evaluates the land frame"), Run.Tick(Data, EGSTN_TrackHookGround::OnGround), Land);
	TestEqual(TEXT("then the frame after it"), Run.Tick(Data, EGSTN_TrackHookGround::OnGround), Land + 1);
	TestFalse(TEXT("the key inside the skipped frames never fires"), Run.KeysFired.Contains(SkippedKey));
	TestEqual(TEXT("the land part's key fires once"), Run.KeysFired.FilterByPredicate([](int32 Key) { return Key == LandKey; }).Num(), 1);

	// The move then ends on its own total
	int32 Guard = 0;
	while (Run.Core.ActionState == EGSTN_ActionState::InTimeline && Guard++ < 100)
	{
		Run.Tick(Data, EGSTN_TrackHookGround::OnGround);
	}
	TestTrue(TEXT("the move ends after its land part"), Run.Output.bFinished);
	TestEqual(TEXT("on its last frame"), Run.Output.Frame, Data.TotalFrames);

	// No Mover (unknown ground): straight through, the key inside the loop's later copies fires as before
	FRun Plain(Data, 300);
	for (int32 Index = 0; Index < 45; ++Index)
	{
		Plain.Tick(Data, EGSTN_TrackHookGround::Unknown);
	}
	TestEqual(TEXT("unknown ground plays straight through"), Plain.Output.Frame, 45);
	TestTrue(TEXT("and fires every key it crosses"), Plain.KeysFired.Contains(SkippedKey));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HoldClockEarlyLandTest, "GSTNCombat.NP.HoldUntilLanded.EarlyLand", GSTN_NP_TEST_FLAGS)
bool FGSTN_HoldClockEarlyLandTest::RunTest(const FString& Parameters)
{
	using namespace GSTNHoldClockTests;
	const FGSTN_CombatTimelineData Data = MakeData();

	// Started low: the ground comes in the startup, before the timeline ever stood still
	FRun Run(Data, 500);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Run.Tick(Data, EGSTN_TrackHookGround::Airborne);
	}
	TestEqual(TEXT("landing out of the startup goes to the land frame"), Run.Tick(Data, EGSTN_TrackHookGround::OnGround), Land);
	TestEqual(TEXT("the paused count is the skipped frames, negative"), Run.Core.TimelinePausedFrames, 5 - Land);

	// The count goes over the wire with its sign (zigzag) and the timeline frame reads back the same
	FGSTN_CombatSyncState State;
	State.Core = Run.Core;
	FBitWriter Writer(1024, true);
	State.NetSerialize(FNetSerializeParams(Writer));
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	FGSTN_CombatSyncState Loaded;
	Loaded.NetSerialize(FNetSerializeParams(Reader));
	TestFalse(TEXT("reads back to the end"), Reader.IsError() || !Reader.AtEnd());
	TestEqual(TEXT("negative paused frames survive the wire"), Loaded.Core.TimelinePausedFrames, Run.Core.TimelinePausedFrames);
	TestFalse(TEXT("and do not ask for a reconcile"), Loaded.Core.ShouldReconcile(Run.Core));

	// A rollback replays the same path: the same frames and the same count
	FRun Again(Data, 500);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Again.Tick(Data, EGSTN_TrackHookGround::Airborne);
	}
	Again.Tick(Data, EGSTN_TrackHookGround::OnGround);
	TestEqual(TEXT("deterministic"), Again.Core.TimelinePausedFrames, Run.Core.TimelinePausedFrames);
	TestEqual(TEXT("next frame continues the land part"), Run.Tick(Data, EGSTN_TrackHookGround::OnGround), Land + 1);

	// A hitstop frame on the touchdown: the timeline stands, the landing is taken on the next frame
	FRun Hit(Data, 700);
	for (int32 Index = 0; Index < 30; ++Index)
	{
		Hit.Tick(Data, EGSTN_TrackHookGround::Airborne);
	}
	++Hit.Frame;
	Hit.Environment.Ground = EGSTN_TrackHookGround::OnGround;
	FGSTN_CombatTimelineLogic::AdvanceClock(Hit.Core, Hit.Frame, &Data, 0, true, Hit.Output, true, nullptr, &Hit.Environment);
	TestEqual(TEXT("hitstop on the touchdown: still the held frame"), Hit.Output.Frame, LoopEnd - 1);
	TestEqual(TEXT("then the land frame"), Hit.Tick(Data, EGSTN_TrackHookGround::OnGround), Land);
	return true;
}

#endif
