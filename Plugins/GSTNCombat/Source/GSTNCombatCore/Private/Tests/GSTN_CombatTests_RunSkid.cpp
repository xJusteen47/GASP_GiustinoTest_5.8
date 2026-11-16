// Headless tests of the Mario-style run P2/P3 (docs/notes/corsa-mario-piano.md): skid start rule, phases, jump window, the run state's skid
// carry and takeoff latch, the air control scale, the jump cut ramp, the attack slide's retreat window and carried speed.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Jump/GSTN_JumpLogic.h"
#include "Jump/GSTN_JumpTypes.h"
#include "Movement/GSTN_MomentumSlide.h"
#include "Movement/GSTN_RunSkidTypes.h"
#include "Movement/GSTN_RunStageTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr float Dt = 0.016f;

	FGSTN_RunFrameInput Frame(int32 Index, bool bGrounded, float Speed)
	{
		FGSTN_RunFrameInput Input;
		Input.Frame = Index;
		Input.bGrounded = bGrounded;
		Input.GroundVelocity = FVector(Speed, 0.0, 0.0);
		Input.StickWorld = FVector(1.0, 0.0, 0.0);
		return Input;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunSkidStartTest, "GSTNCombat.Core.RunSkid.Start", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RunSkidStartTest::RunTest(const FString& Parameters)
{
	const FGSTN_RunSkidTuning Tuning;
	const FVector Run(600.0, 0.0, 0.0);
	TestTrue(TEXT("stage 2, fast, stick reversed: skid"), FGSTN_RunSkidMath::ShouldStart(Tuning, 1, Run, FVector(-1.0, 0.0, 0.0), 1.f));
	TestFalse(TEXT("stage 1 pivots, no skid"), FGSTN_RunSkidMath::ShouldStart(Tuning, 0, Run, FVector(-1.0, 0.0, 0.0), 1.f));
	TestFalse(TEXT("too slow"), FGSTN_RunSkidMath::ShouldStart(Tuning, 1, FVector(200.0, 0.0, 0.0), FVector(-1.0, 0.0, 0.0), 1.f));
	TestFalse(TEXT("a 90 degree turn is no reversal"), FGSTN_RunSkidMath::ShouldStart(Tuning, 1, Run, FVector(0.0, 1.0, 0.0), 1.f));
	TestFalse(TEXT("a light stick does not skid"), FGSTN_RunSkidMath::ShouldStart(Tuning, 1, Run, FVector(-0.3, 0.0, 0.0), 1.f));
	TestFalse(TEXT("a movement lock (factor 0) never skids"), FGSTN_RunSkidMath::ShouldStart(Tuning, 1, Run, FVector(-1.0, 0.0, 0.0), 0.f));
	FGSTN_RunSkidTuning Off;
	Off.bEnabled = false;
	TestFalse(TEXT("disabled"), FGSTN_RunSkidMath::ShouldStart(Off, 2, Run, FVector(-1.0, 0.0, 0.0), 1.f));
	TestTrue(TEXT("the threshold scales with the stats"), FGSTN_RunSkidMath::ShouldStart(Tuning, 1, FVector(200.0, 0.0, 0.0), FVector(-1.0, 0.0, 0.0), 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunSkidPhasesTest, "GSTNCombat.Core.RunSkid.Phases", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RunSkidPhasesTest::RunTest(const FString& Parameters)
{
	FGSTN_RunSkidTuning Tuning;
	Tuning.Brake = 2500.f;  // 40 cm/s a frame at 62.5 Hz
	Tuning.EnterFrames = 4;
	Tuning.ExitFrames = 8;

	const int32 Rest = FGSTN_RunSkidMath::FramesToRest(600.f, Tuning.Brake, Dt);
	TestEqual(TEXT("600 cm/s rests after 15 frames"), Rest, 15);
	TestEqual(TEXT("frame 0 moves at the start speed"), FGSTN_RunSkidMath::SpeedAt(600.f, Tuning.Brake, 0, Dt), 600.f);
	TestEqual(TEXT("closed form"), FGSTN_RunSkidMath::SpeedAt(600.f, Tuning.Brake, 5, Dt), 400.f, 1e-3f);
	TestEqual(TEXT("never negative"), FGSTN_RunSkidMath::SpeedAt(600.f, Tuning.Brake, 40, Dt), 0.f);
	TestEqual(TEXT("stop distance v^2/2a"), FGSTN_RunSkidMath::StopDistance(600.f, Tuning.Brake), 72.f, 1e-3f);

	TestEqual(TEXT("enter"), FGSTN_RunSkidMath::PhaseAt(Tuning, 0, Rest), EGSTN_RunSkidPhase::Enter);
	TestEqual(TEXT("active"), FGSTN_RunSkidMath::PhaseAt(Tuning, 4, Rest), EGSTN_RunSkidPhase::Active);
	TestEqual(TEXT("exit at rest"), FGSTN_RunSkidMath::PhaseAt(Tuning, Rest, Rest), EGSTN_RunSkidPhase::Exit);
	TestEqual(TEXT("last exit frame"), FGSTN_RunSkidMath::PhaseAt(Tuning, Rest + 7, Rest), EGSTN_RunSkidPhase::Exit);
	TestEqual(TEXT("over"), FGSTN_RunSkidMath::PhaseAt(Tuning, Rest + 8, Rest), EGSTN_RunSkidPhase::None);
	TestEqual(TEXT("a ledge stop at frame 2 goes straight to the exit"), FGSTN_RunSkidMath::PhaseAt(Tuning, 2, 2), EGSTN_RunSkidPhase::Exit);

	// The window as the abilities see it: they read frame N's tag on frame N+1, so the tag of frame Start-1 opens it
	Tuning.JumpWindowStart = 3;
	Tuning.JumpWindowEnd = 6;
	TestFalse(TEXT("window closed before"), FGSTN_RunSkidMath::InJumpWindow(Tuning, 1));
	TestTrue(TEXT("window opens one frame early"), FGSTN_RunSkidMath::InJumpWindow(Tuning, 2));
	TestTrue(TEXT("window last frame"), FGSTN_RunSkidMath::InJumpWindow(Tuning, 5));
	TestFalse(TEXT("window closed after"), FGSTN_RunSkidMath::InJumpWindow(Tuning, 6));
	Tuning.JumpWindowEnd = -1;
	TestTrue(TEXT("-1 = until the skid ends"), FGSTN_RunSkidMath::InJumpWindow(Tuning, 500));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunSkidRunStateTest, "GSTNCombat.Core.RunSkid.RunState", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RunSkidRunStateTest::RunTest(const FString& Parameters)
{
	FGSTN_RunStagesTuning Tuning = FGSTN_RunStagesTuning::MakeDefault();
	Tuning.Skid.CarryFrames = 6;
	FGSTN_RunStageState State;
	State.Stage = 2;

	// A skid frame: the stage drops, the speed of before the skid is carried for the skid's window (not the new stage's)
	FGSTN_RunFrameInput Skid = Frame(0, true, 700.f);
	Skid.bSkidStarted = true;
	Skid.SkidStartSpeed = 820.f;
	FGSTN_RunStageLogic::Step(Tuning, Skid, State);
	TestEqual(TEXT("skid resets the stage"), static_cast<int32>(State.Stage), 0);
	TestEqual(TEXT("carry source"), State.CarrySource, EGSTN_RunCarrySource::Skid);
	TestEqual(TEXT("carried = before the skid"), FGSTN_RunStageLogic::GetCarriedSpeed(Tuning, State), 820.f);
	for (int32 F = 1; F <= 6; ++F)
	{
		FGSTN_RunStageLogic::Step(Tuning, Frame(F, true, 300.f), State);
	}
	TestEqual(TEXT("still carried on the window's last frame"), FGSTN_RunStageLogic::GetCarriedSpeed(Tuning, State), 820.f);
	FGSTN_RunStageLogic::Step(Tuning, Frame(7, true, 300.f), State);
	TestEqual(TEXT("gone after the window"), FGSTN_RunStageLogic::GetCarriedSpeed(Tuning, State), 0.f);

	Tuning.Skid.bResetsStage = false;
	FGSTN_RunStageState Kept;
	Kept.Stage = 2;
	Skid.Frame = 0;
	FGSTN_RunStageLogic::Step(Tuning, Skid, Kept);
	TestEqual(TEXT("bResetsStage off keeps the stage"), static_cast<int32>(Kept.Stage), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunSkidAirTest, "GSTNCombat.Core.RunSkid.Air", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RunSkidAirTest::RunTest(const FString& Parameters)
{
	FGSTN_RunStagesTuning Tuning = FGSTN_RunStagesTuning::MakeDefault();
	FGSTN_RunStageState State;
	State.Stage = 2;
	FGSTN_RunStageLogic::Step(Tuning, Frame(0, true, 840.f), State);
	FGSTN_RunStageLogic::Step(Tuning, Frame(1, false, 830.f), State);
	TestTrue(TEXT("airborne"), State.bAirborne);
	TestEqual(TEXT("takeoff speed = the last ground frame's"), State.TakeoffSpeed, 840.f, 0.01f);
	TestEqual(TEXT("takeoff stage"), static_cast<int32>(State.TakeoffStage), 2);
	FGSTN_RunStageLogic::Step(Tuning, Frame(2, false, 100.f), State);
	TestEqual(TEXT("latched once per air time"), State.TakeoffSpeed, 840.f, 0.01f);
	TestEqual(TEXT("air top speed = takeoff"), FGSTN_RunStageLogic::GetAirTopSpeed(Tuning, State, 1.f), 840.f, 0.01f);
	TestEqual(TEXT("stage 3 steering"), FGSTN_RunStageLogic::GetAirControlScale(Tuning, State), Tuning.Stages[2].AirControlScale);

	FGSTN_RunStageState Standing;
	FGSTN_RunStageLogic::Step(Tuning, Frame(0, true, 0.f), Standing);
	FGSTN_RunStageLogic::Step(Tuning, Frame(1, false, 0.f), Standing);
	TestEqual(TEXT("a standing jump still drifts (floor)"), FGSTN_RunStageLogic::GetAirTopSpeed(Tuning, Standing, 1.f), Tuning.Air.MinAirSpeed);
	FGSTN_RunStageLogic::Step(Tuning, Frame(2, true, 0.f), Standing);
	TestFalse(TEXT("landed"), Standing.bAirborne);

	Tuning.Air.Model = EGSTN_AirControlModel::Fixed;
	TestEqual(TEXT("Fixed model: no stage scale"), FGSTN_RunStageLogic::GetAirControlScale(Tuning, State), 1.f);

	// The new fields travel and compare
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	State.SerializeCompact(Writer);
	FGSTN_RunStageState Copy;
	FMemoryReader Reader(Bytes);
	Copy.SerializeCompact(Reader);
	TestTrue(TEXT("airborne travels"), Copy.bAirborne);
	TestEqual(TEXT("takeoff speed travels"), Copy.TakeoffSpeed, 840.f, 0.1f);
	TestEqual(TEXT("takeoff stage travels"), static_cast<int32>(Copy.TakeoffStage), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_JumpCutRampTest, "GSTNCombat.Core.Jump.CutRamp", GSTN_CORE_TEST_FLAGS)
bool FGSTN_JumpCutRampTest::RunTest(const FString& Parameters)
{
	FGSTN_JumpParams Params;
	Params.RiseGravityScale = 1.f;
	Params.CutGravityScale = 3.f;
	Params.MinHoldFrames = 2;
	Params.MaxHoldFrames = 20;
	Params.CutRampFrames = 4;
	const FGSTN_JumpResetRules Rules;

	FGSTN_JumpState State = FGSTN_JumpLogic::ApplyTakeoff(FGSTN_JumpState(), 0, 0, EGSTN_JumpKind::Ground);
	FGSTN_JumpMoverSample Rising;
	Rising.bGrounded = false;
	Rising.UpSpeed = 500.0;
	State = FGSTN_JumpLogic::Step(State, 1, Rising, true, Params, Rules);
	TestEqual(TEXT("held: rise scale"), FGSTN_JumpLogic::GravityScale(State, Params), 1.f);
	State = FGSTN_JumpLogic::Step(State, 2, Rising, false, Params, Rules);
	TestTrue(TEXT("released: cut"), State.bJumpCut);
	TestEqual(TEXT("ramp step 1 of 4"), FGSTN_JumpLogic::GravityScale(State, Params), 1.5f, 1e-4f);
	State = FGSTN_JumpLogic::Step(State, 3, Rising, false, Params, Rules);
	TestEqual(TEXT("ramp step 2 of 4"), FGSTN_JumpLogic::GravityScale(State, Params), 2.f, 1e-4f);
	for (int32 F = 4; F < 10; ++F)
	{
		State = FGSTN_JumpLogic::Step(State, F, Rising, false, Params, Rules);
	}
	TestEqual(TEXT("ramp done: cut scale"), FGSTN_JumpLogic::GravityScale(State, Params), 3.f);
	FGSTN_JumpMoverSample Falling = Rising;
	Falling.UpSpeed = -10.0;
	State = FGSTN_JumpLogic::Step(State, 10, Falling, false, Params, Rules);
	TestEqual(TEXT("apex: the fall scale takes over"), FGSTN_JumpLogic::GravityScale(State, Params), Params.FallGravityScale);

	Params.CutRampFrames = 0;
	FGSTN_JumpState Step = FGSTN_JumpLogic::ApplyTakeoff(FGSTN_JumpState(), 0, 0, EGSTN_JumpKind::Ground);
	Step = FGSTN_JumpLogic::Step(Step, 1, Rising, true, Params, Rules);
	Step = FGSTN_JumpLogic::Step(Step, 2, Rising, false, Params, Rules);
	TestEqual(TEXT("ramp 0: the cut scale at once (as before)"), FGSTN_JumpLogic::GravityScale(Step, Params), 3.f);

	// The counter travels
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	State.NetSerialize(Writer);
	FGSTN_JumpState Copy;
	FMemoryReader Reader(Bytes);
	Copy.NetSerialize(Reader);
	TestTrue(TEXT("jump state round trip"), Copy == State);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MomentumSlideStickTest, "GSTNCombat.Core.MomentumSlide.StickRetreat", GSTN_CORE_TEST_FLAGS)
bool FGSTN_MomentumSlideStickTest::RunTest(const FString& Parameters)
{
	FGSTN_MomentumSlide Slide;
	Slide.RetreatWindowFrames = 6;
	Slide.StickBrake = 3000.f;
	Slide.StickAlongScale = 0.5f;
	FGSTN_MomentumSlideBraking Base;
	Base.Deceleration = 1500.f;

	const FGSTN_MomentumSlideBraking InWindow = FGSTN_MomentumSlideMath::ApplyStick(Slide, Base, 3, -1.f, 1.f);
	TestEqual(TEXT("retreat window: the stick back does not brake"), InWindow.Deceleration, 1500.f);
	const FGSTN_MomentumSlideBraking After = FGSTN_MomentumSlideMath::ApplyStick(Slide, Base, 6, -1.f, 1.f);
	TestEqual(TEXT("after the window the stick back brakes"), After.Deceleration, 4500.f);
	const FGSTN_MomentumSlideBraking Half = FGSTN_MomentumSlideMath::ApplyStick(Slide, Base, 6, -1.f, 0.5f);
	TestEqual(TEXT("brake scales with the stick"), Half.Deceleration, 3000.f);
	const FGSTN_MomentumSlideBraking Along = FGSTN_MomentumSlideMath::ApplyStick(Slide, Base, 8, 1.f, 1.f);
	TestEqual(TEXT("stick along: less braking"), Along.Deceleration, 750.f);
	const FGSTN_MomentumSlideBraking Released = FGSTN_MomentumSlideMath::ApplyStick(Slide, Base, 8, -1.f, 0.1f);
	TestEqual(TEXT("released: nothing"), Released.Deceleration, 1500.f);
	const FGSTN_MomentumSlideBraking Side = FGSTN_MomentumSlideMath::ApplyStick(Slide, Base, 8, 0.f, 1.f);
	TestEqual(TEXT("sideways: nothing"), Side.Deceleration, 1500.f);

	TestEqual(TEXT("carried speed wins when faster"), FGSTN_MomentumSlideMath::PickRunningSpeed(Slide, 120.f, 650.f), 650.f);
	TestEqual(TEXT("measured wins when faster"), FGSTN_MomentumSlideMath::PickRunningSpeed(Slide, 700.f, 650.f), 700.f);
	Slide.bUseCarriedSpeed = false;
	TestEqual(TEXT("off: measured only"), FGSTN_MomentumSlideMath::PickRunningSpeed(Slide, 120.f, 650.f), 120.f);
	return true;
}

#endif
