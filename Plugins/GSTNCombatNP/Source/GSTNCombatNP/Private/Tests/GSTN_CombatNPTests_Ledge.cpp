// Ledge Check in the combat clock (owner 2026-11-16): inside the window, on the ground, a probe that finds no floor stops the move in place (one
// tick at a time), ends it, recoils it into the track's response or names the track for the continuation. Outside the window, in the air or with
// floor ahead nothing happens. Pure: sync state in, sync state out, the probe lent through the hook environment as the combat tick lends its trace.

#include "Misc/AutomationTest.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "Simulation/GSTN_TimelineTrackHook.h"
#include "Timeline/GSTN_Ledge.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNLedgeClockTests
{
	constexpr int32 WindowStart = 5;
	constexpr int32 WindowEnd = 20;

	/** A 40-frame dash with one Ledge Check window [5, 20) */
	FGSTN_CombatTimelineData MakeData(EGSTN_LedgeAction Action, int32 RecoilFrames = 0)
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 40;
		Data.FirstActionableFrame = 39;
		Data.OverrideFrames = 0;
		FGSTN_TimelineTrack_LedgeCheck Ledge;
		Ledge.StartFrame = WindowStart;
		Ledge.EndFrame = WindowEnd;
		Ledge.Action = Action;
		Ledge.ProbeAhead = 35.f;
		Ledge.ProbeDepth = 50.f;
		Ledge.Response.RecoilFrames = RecoilFrames;
		Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_LedgeCheck>(Ledge));
		return Data;
	}

	struct FRun
	{
		FGSTN_CombatSyncCore Core;
		FGSTN_TimelineOutput Output;
		FGSTN_TrackHookEnvironment Environment;
		int32 Frame = 0;
		/** What the fake probe answers, and what it was asked */
		bool bFloor = true;
		int32 Probes = 0;
		float LastAhead = 0.f;
		float LastDepth = 0.f;

		FRun(const FGSTN_CombatTimelineData& Data, int32 StartFrame)
		{
			Output.Reserve(Data);
			Frame = StartFrame;
			Environment.Ground = EGSTN_TrackHookGround::OnGround;
			Environment.HasFloorAhead = [this](float Ahead, float Depth)
			{
				++Probes;
				LastAhead = Ahead;
				LastDepth = Depth;
				return bFloor;
			};
			FGSTN_CombatTimelineLogic::Start(Core, StartFrame, 3, EGSTN_ActivationSource::Command);
			FGSTN_CombatTimelineLogic::EvaluateStart(Core, StartFrame, Data, 0, Output, &Environment);
		}

		/** Ticks until the timeline frame evaluated reaches Target (or the move ends); returns the last frame evaluated */
		int32 TickTo(const FGSTN_CombatTimelineData& Data, int32 Target)
		{
			while (Core.ActionState == EGSTN_ActionState::InTimeline && Output.Frame < Target)
			{
				Tick(Data);
			}
			return Output.Frame;
		}

		bool Tick(const FGSTN_CombatTimelineData& Data)
		{
			++Frame;
			Core.Requests.PruneBefore(Frame);
			return FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 0, true, Output, false, nullptr, &Environment);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LedgeClockWindowTest, "GSTNCombat.NP.Ledge.ProbesOnlyInWindowOnGround", GSTN_NP_TEST_FLAGS)
bool FGSTN_LedgeClockWindowTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLedgeClockTests;
	const FGSTN_CombatTimelineData Data = MakeData(EGSTN_LedgeAction::EndMove);

	// The void is there from the start, but the window opens on frame 5: frames 1-4 play
	FRun Run(Data, 100);
	Run.bFloor = false;
	Run.TickTo(Data, WindowStart - 1);
	TestEqual(TEXT("no probe before the window"), Run.Probes, 0);
	TestTrue(TEXT("still running before the window"), Run.Core.ActionState == EGSTN_ActionState::InTimeline);

	// In the air inside the window: no probe, the move plays on (the Ground Rule handles walking off)
	Run.Environment.Ground = EGSTN_TrackHookGround::Airborne;
	Run.Tick(Data);
	TestEqual(TEXT("no probe in the air"), Run.Probes, 0);
	TestTrue(TEXT("still running in the air"), Run.Core.ActionState == EGSTN_ActionState::InTimeline);

	// Floor ahead on the ground inside the window: probed with the track's numbers, nothing happens
	Run.Environment.Ground = EGSTN_TrackHookGround::OnGround;
	Run.bFloor = true;
	Run.Tick(Data);
	TestEqual(TEXT("probed once"), Run.Probes, 1);
	TestEqual(TEXT("probe ahead"), Run.LastAhead, 35.f);
	TestEqual(TEXT("probe depth"), Run.LastDepth, 50.f);
	TestTrue(TEXT("floor ahead: running"), Run.Core.ActionState == EGSTN_ActionState::InTimeline);

	// No probe lent (a test, an entity without a body): the rule does nothing
	Run.bFloor = false;
	const int32 ProbesBefore = Run.Probes;
	Run.Environment.HasFloorAhead = nullptr;
	Run.Tick(Data);
	TestTrue(TEXT("no probe function: the rule does nothing"), Run.Core.ActionState == EGSTN_ActionState::InTimeline);
	TestEqual(TEXT("no probe counted"), Run.Probes, ProbesBefore);

	// After the window: no probe whatever the floor
	FRun After(Data, 200);
	After.TickTo(Data, WindowEnd);
	After.bFloor = false;
	After.Tick(Data);
	TestEqual(TEXT("no probe after the window"), After.Probes, WindowEnd - WindowStart);
	TestTrue(TEXT("running after the window"), After.Core.ActionState == EGSTN_ActionState::InTimeline);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LedgeClockStopTest, "GSTNCombat.NP.Ledge.Stop", GSTN_NP_TEST_FLAGS)
bool FGSTN_LedgeClockStopTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLedgeClockTests;
	const FGSTN_CombatTimelineData Data = MakeData(EGSTN_LedgeAction::Stop);

	FRun Run(Data, 100);
	Run.TickTo(Data, WindowStart);
	TestFalse(TEXT("floor: no stop"), Run.Core.bLedgeStop);

	// The void: the move keeps playing (its frames advance), the stop bit is set for each tick that sees it
	Run.bFloor = false;
	const int32 Before = Run.Output.Frame;
	Run.Tick(Data);
	TestTrue(TEXT("void: stop"), Run.Core.bLedgeStop);
	TestTrue(TEXT("the move plays on"), Run.Core.ActionState == EGSTN_ActionState::InTimeline);
	TestEqual(TEXT("its frames advance"), Run.Output.Frame, Before + 1);

	// The bit goes over the wire inside the action part
	{
		FBitWriter Writer(0, true);
		Run.Core.NetSerialize(Writer);
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		FGSTN_CombatSyncCore Copy;
		Copy.NetSerialize(Reader);
		TestTrue(TEXT("bLedgeStop survives the wire"), Copy.bLedgeStop);
	}

	// Floor again: the bit clears on the next tick
	Run.bFloor = true;
	Run.Tick(Data);
	TestFalse(TEXT("floor again: no stop"), Run.Core.bLedgeStop);

	// The void after the window: no stop
	Run.bFloor = false;
	Run.TickTo(Data, WindowEnd);
	TestFalse(TEXT("after the window: no stop"), Run.Core.bLedgeStop);
	TestTrue(TEXT("the move plays on"), Run.Core.ActionState == EGSTN_ActionState::InTimeline);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LedgeClockEndTest, "GSTNCombat.NP.Ledge.EndRecoilContinue", GSTN_NP_TEST_FLAGS)
bool FGSTN_LedgeClockEndTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLedgeClockTests;

	// EndMove: the move ends on the tick that sees the void, no continuation
	{
		const FGSTN_CombatTimelineData Data = MakeData(EGSTN_LedgeAction::EndMove);
		FRun Run(Data, 100);
		Run.TickTo(Data, WindowStart - 1);
		Run.bFloor = false;
		TestFalse(TEXT("EndMove: the tick reports the end"), Run.Tick(Data));
		TestTrue(TEXT("EndMove: actionable"), Run.Core.ActionState != EGSTN_ActionState::InTimeline);
		TestEqual(TEXT("EndMove: no continuation"), Run.Output.LandingContinueTrack, static_cast<int32>(INDEX_NONE));
	}
	// ContinueWithMove: ends and names the track for the combat model
	{
		const FGSTN_CombatTimelineData Data = MakeData(EGSTN_LedgeAction::ContinueWithMove);
		FRun Run(Data, 100);
		Run.TickTo(Data, WindowStart - 1);
		Run.bFloor = false;
		Run.Tick(Data);
		TestTrue(TEXT("Continue: the move ended"), Run.Core.ActionState != EGSTN_ActionState::InTimeline);
		TestEqual(TEXT("Continue: the track is named"), Run.Output.LandingContinueTrack, 0);
	}
	// Recoil with a response: the response replaces the rest of the move from this frame
	{
		const FGSTN_CombatTimelineData Data = MakeData(EGSTN_LedgeAction::Recoil, 12);
		FRun Run(Data, 100);
		Run.TickTo(Data, WindowStart - 1);
		Run.bFloor = false;
		TestTrue(TEXT("Recoil: still busy"), Run.Tick(Data));
		TestEqual(TEXT("Recoil: started this frame"), Run.Core.RecoilStartFrame, Run.Frame);
		TestEqual(TEXT("Recoil: the track's response"), static_cast<int32>(Run.Core.StrikeTrack), 0);
		TestTrue(TEXT("Recoil: the recoil player plays"), Run.Output.bRecoil);
		TestTrue(TEXT("Recoil: the response is the track's"), Data.GetRicochetResponse(0) == &Data.Tracks[0].Get<FGSTN_TimelineTrack_LedgeCheck>().Response);
	}
	// Recoil without a response: like EndMove
	{
		const FGSTN_CombatTimelineData Data = MakeData(EGSTN_LedgeAction::Recoil, 0);
		FRun Run(Data, 100);
		Run.TickTo(Data, WindowStart - 1);
		Run.bFloor = false;
		Run.Tick(Data);
		TestTrue(TEXT("unauthored Recoil: ended"), Run.Core.ActionState != EGSTN_ActionState::InTimeline);
	}
	return true;
}

#endif
