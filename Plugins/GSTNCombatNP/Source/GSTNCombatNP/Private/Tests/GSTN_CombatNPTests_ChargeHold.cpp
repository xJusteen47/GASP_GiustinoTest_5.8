// Charge Hold in the combat clock (aerial charge, 2026-11-25): the timeline stands on the hold frame while the button is held since the start, the
// release plays on, and a landing during the hold (Hold Until Landed's jump) also ends the hold, so the paused montage resumes (desktop network test
// 2026-11-27: the land montage stayed paused for the whole landing). Pure: sync state in, sync state out, the button map and the ground fact lent
// through the hook environment as the combat tick lends them.

#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_InputSample.h"
#include "Misc/AutomationTest.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "Simulation/GSTN_TimelineTrackHook.h"
#include "Timeline/GSTN_ChargeHold.h"
#include "Timeline/GSTN_HoldUntilLanded.h"
#include "Timeline/GSTN_TimelineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNChargeClockTests
{
	constexpr int32 HoldFrame = 5;
	constexpr int32 LoopStart = 16;
	constexpr int32 LoopEnd = 27;
	constexpr int32 Land = 60;

	/** The Overlord dropkick's shape with its charge: hold on frame 5, held loop [16, 27), land from 60 */
	FGSTN_CombatTimelineData MakeData(const FGameplayTag& Button)
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 92;
		Data.FirstActionableFrame = 91;
		Data.OverrideFrames = 0;
		Data = UGSTN_TimelineUtils::SetHoldUntilLandedTrack(Data, LoopStart, LoopEnd, Land);
		return UGSTN_TimelineUtils::SetChargeHoldTrack(Data, HoldFrame, Button, NAME_None, 60);
	}

	struct FRun
	{
		FGSTN_CombatSyncCore Core;
		FGSTN_TimelineOutput Output;
		FGSTN_TrackHookEnvironment Environment;
		int32 Frame = 0;

		FRun(const FGSTN_CombatTimelineData& Data, const FGSTN_ButtonMap& Buttons, int32 StartFrame)
		{
			Output.Reserve(Data);
			Environment.Buttons = &Buttons;
			Frame = StartFrame;
			AppendInput(StartFrame, true, true);
			FGSTN_CombatTimelineLogic::Start(Core, StartFrame, 3, EGSTN_ActivationSource::Command);
			FGSTN_CombatTimelineLogic::EvaluateStart(Core, StartFrame, Data, 0, Output, &Environment);
		}

		void AppendInput(int32 InputFrame, bool bHeld, bool bPressed = false, bool bReleased = false)
		{
			FGSTN_InputSample Sample;
			Sample.Frame = InputFrame;
			Sample.Held = bHeld ? 1u : 0u;
			Sample.Pressed = bPressed ? 1u : 0u;
			Sample.Released = bReleased ? 1u : 0u;
			Core.Input.History.Append(Sample);
		}

		/** One tick: the input of the frame before is already in the history (as in the combat tick), then the clock with the ground fact given */
		int32 Tick(const FGSTN_CombatTimelineData& Data, EGSTN_TrackHookGround Ground, bool bHeld, bool bReleasedNow = false)
		{
			++Frame;
			Environment.Ground = Ground;
			Core.Requests.PruneBefore(Frame);
			FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, &Data, 0, true, Output, false, nullptr, &Environment);
			AppendInput(Frame, bHeld, false, bReleasedNow);
			return Output.Frame;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ChargeClockReleaseTest, "GSTNCombat.NP.ChargeHold.Release", GSTN_NP_TEST_FLAGS)
bool FGSTN_ChargeClockReleaseTest::RunTest(const FString& Parameters)
{
	using namespace GSTNChargeClockTests;
	const FGameplayTag Button = FGameplayTag::RequestGameplayTag(TEXT("Input.Button.Attack"), false);
	if (!Button.IsValid())
	{
		AddWarning(TEXT("Input.Button.Attack is not registered: skipped"));
		return true;
	}
	const FGSTN_ButtonMap Buttons(TArray<FGameplayTag>{ Button });
	const FGSTN_CombatTimelineData Data = MakeData(Button);
	FRun Run(Data, Buttons, 100);
	for (int32 Tick = 1; Tick <= HoldFrame; ++Tick)
	{
		Run.Tick(Data, EGSTN_TrackHookGround::InAir, true);
	}
	TestEqual(TEXT("on the hold frame"), Run.Output.Frame, HoldFrame);
	for (int32 Tick = 0; Tick < 10; ++Tick)
	{
		TestEqual(TEXT("stands on the hold frame while held"), Run.Tick(Data, EGSTN_TrackHookGround::InAir, Tick < 9, Tick == 9), HoldFrame);
		TestTrue(TEXT("stalled"), Run.Output.bChargeHoldStall);
	}
	TestEqual(TEXT("ten charge ticks"), static_cast<int32>(Run.Core.ChargeFrames), 10);
	TestEqual(TEXT("released: the startup plays on"), Run.Tick(Data, EGSTN_TrackHookGround::InAir, false), HoldFrame + 1);
	TestTrue(TEXT("release"), Run.Output.bChargeHoldRelease);
	TestTrue(TEXT("resume"), Run.Output.bChargeHoldResume);
	Run.Tick(Data, EGSTN_TrackHookGround::InAir, false);
	TestFalse(TEXT("one tick only"), Run.Output.bChargeHoldResume);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ChargeClockLandTest, "GSTNCombat.NP.ChargeHold.LandDuringHold", GSTN_NP_TEST_FLAGS)
bool FGSTN_ChargeClockLandTest::RunTest(const FString& Parameters)
{
	using namespace GSTNChargeClockTests;
	const FGameplayTag Button = FGameplayTag::RequestGameplayTag(TEXT("Input.Button.Attack"), false);
	if (!Button.IsValid())
	{
		AddWarning(TEXT("Input.Button.Attack is not registered: skipped"));
		return true;
	}
	const FGSTN_ButtonMap Buttons(TArray<FGameplayTag>{ Button });
	const FGSTN_CombatTimelineData Data = MakeData(Button);
	FRun Run(Data, Buttons, 100);
	for (int32 Tick = 1; Tick <= HoldFrame + 5; ++Tick)
	{
		Run.Tick(Data, EGSTN_TrackHookGround::InAir, true);
	}
	TestTrue(TEXT("charging in the air"), Run.Output.bChargeHoldStall);
	// Touchdown with the button still held: Hold Until Landed jumps to the land part, the hold is over
	TestEqual(TEXT("jumps to the land frame"), Run.Tick(Data, EGSTN_TrackHookGround::OnGround, true), Land);
	TestFalse(TEXT("no stall on the landing"), Run.Output.bChargeHoldStall);
	TestTrue(TEXT("the paused montage resumes"), Run.Output.bChargeHoldResume);
	TestFalse(TEXT("not a release: the montage is not put back on the hold pose"), Run.Output.bChargeHoldRelease);
	TestEqual(TEXT("the land part plays on"), Run.Tick(Data, EGSTN_TrackHookGround::OnGround, true), Land + 1);
	return true;
}

#endif
