// Headless tests of the Mario-style run stages (docs/notes/corsa-mario-piano.md, P1): stage climb, boost window, release, tight
// turn, bump, substeps, network form and the rollback contract (restore a state, replay the same inputs, same result).

#include "Tests/GSTN_CombatTestUtils.h"
#include "Movement/GSTN_RunStageTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Three short stages so the tests stay readable: 3 frames each to climb, thresholds at 90% of the stage speed */
	FGSTN_RunStagesTuning TestTuning(EGSTN_RunBoostMode Mode = EGSTN_RunBoostMode::InitialWindow)
	{
		FGSTN_RunStagesTuning Tuning = FGSTN_RunStagesTuning::MakeDefault();
		for (FGSTN_RunStage& Stage : Tuning.Stages)
		{
			Stage.FramesToNextStage = 3;
		}
		Tuning.BoostMode = Mode;
		Tuning.BoostWindowFrames = 5;
		Tuning.ReleaseDropFrames = 4;
		return Tuning;
	}

	/** A grounded frame running along +X at Speed with the stick at Stick (world) */
	FGSTN_RunFrameInput Run(int32 Frame, float Speed, FVector Stick = FVector(1.0, 0.0, 0.0), bool bBoost = false)
	{
		FGSTN_RunFrameInput Input;
		Input.Frame = Frame;
		Input.bGrounded = true;
		Input.GroundVelocity = FVector(Speed, 0.0, 0.0);
		Input.StickWorld = Stick;
		Input.bBoostCommand = bBoost;
		return Input;
	}

	/** Steps frames [From, To) at the top speed of the stage the state is in (so every frame qualifies) */
	int32 RunAtStageSpeed(const FGSTN_RunStagesTuning& Tuning, FGSTN_RunStageState& State, int32 From, int32 To)
	{
		for (int32 Frame = From; Frame < To; ++Frame)
		{
			FGSTN_RunStageLogic::Step(Tuning, Run(Frame, Tuning.GetStage(State.Stage).MaxSpeed), State);
		}
		return To;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunStagesClimbTest, "GSTNCombat.Core.RunStages.Climb", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RunStagesClimbTest::RunTest(const FString& Parameters)
{
	const FGSTN_RunStagesTuning Tuning = TestTuning();
	FGSTN_RunStageState State;

	// Under the threshold nothing counts
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		FGSTN_RunStageLogic::Step(Tuning, Run(Frame, Tuning.Stages[0].MaxSpeed * 0.5f), State);
	}
	TestEqual(TEXT("slow running stays on stage 1"), (int32)State.Stage, 0);

	RunAtStageSpeed(Tuning, State, 10, 12);
	TestEqual(TEXT("2 qualifying frames: still stage 1"), (int32)State.Stage, 0);
	RunAtStageSpeed(Tuning, State, 12, 13);
	TestEqual(TEXT("3 qualifying frames: stage 2"), (int32)State.Stage, 1);
	RunAtStageSpeed(Tuning, State, 13, 16);
	TestEqual(TEXT("3 more at stage 2 speed: stage 3"), (int32)State.Stage, 2);
	RunAtStageSpeed(Tuning, State, 16, 40);
	TestEqual(TEXT("the last stage is the top"), (int32)State.Stage, 2);

	// The stick sideways (not forward) does not count
	FGSTN_RunStageState Side;
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		FGSTN_RunStageLogic::Step(Tuning, Run(Frame, Tuning.Stages[0].MaxSpeed, FVector(0.5, 0.86, 0.0)), Side);
	}
	TestEqual(TEXT("stick 60 degrees off the velocity: no climb"), (int32)Side.Stage, 0);

	// A stats factor of 0.5 halves the threshold
	FGSTN_RunStageState Slow;
	for (int32 Frame = 0; Frame < 3; ++Frame)
	{
		FGSTN_RunFrameInput Input = Run(Frame, Tuning.Stages[0].MaxSpeed * 0.5f);
		Input.SpeedFactor = 0.5f;
		FGSTN_RunStageLogic::Step(Tuning, Input, Slow);
	}
	TestEqual(TEXT("a slow kit climbs at its own top speed"), (int32)Slow.Stage, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunStagesBoostWindowTest, "GSTNCombat.Core.RunStages.BoostWindow", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RunStagesBoostWindowTest::RunTest(const FString& Parameters)
{
	{
		const FGSTN_RunStagesTuning Tuning = TestTuning();
		FGSTN_RunStageState State;
		FGSTN_RunStageLogic::Step(Tuning, Run(0, 0.f), State);
		FGSTN_RunStageLogic::Step(Tuning, Run(1, 100.f, FVector(1.0, 0.0, 0.0), true), State);
		TestEqual(TEXT("boost inside the initial window: the middle stage (BoostTargetStage 2)"), (int32)State.Stage, 1);
	}
	{
		const FGSTN_RunStagesTuning Tuning = TestTuning();
		FGSTN_RunStageState State;
		for (int32 Frame = 0; Frame < 8; ++Frame)
		{
			FGSTN_RunStageLogic::Step(Tuning, Run(Frame, 100.f), State);
		}
		FGSTN_RunStageLogic::Step(Tuning, Run(8, 100.f, FVector(1.0, 0.0, 0.0), true), State);
		TestEqual(TEXT("boost after the window (frame 8 > 5): ignored"), (int32)State.Stage, 0);

		// Releasing and pushing again opens a new window
		for (int32 Frame = 9; Frame < 11; ++Frame)
		{
			FGSTN_RunStageLogic::Step(Tuning, Run(Frame, 0.f, FVector::ZeroVector), State);
		}
		FGSTN_RunStageLogic::Step(Tuning, Run(11, 0.f), State);
		FGSTN_RunStageLogic::Step(Tuning, Run(12, 50.f, FVector(1.0, 0.0, 0.0), true), State);
		TestEqual(TEXT("a new start opens a new window"), (int32)State.Stage, 1);
	}
	{
		const FGSTN_RunStagesTuning Tuning = TestTuning(EGSTN_RunBoostMode::Anytime);
		FGSTN_RunStageState State;
		for (int32 Frame = 0; Frame < 30; ++Frame)
		{
			FGSTN_RunStageLogic::Step(Tuning, Run(Frame, 100.f), State);
		}
		FGSTN_RunStageLogic::Step(Tuning, Run(30, 100.f, FVector(1.0, 0.0, 0.0), true), State);
		TestEqual(TEXT("Anytime: boost late still works"), (int32)State.Stage, 1);
	}
	{
		const FGSTN_RunStagesTuning Tuning = TestTuning(EGSTN_RunBoostMode::Disabled);
		FGSTN_RunStageState State;
		FGSTN_RunStageLogic::Step(Tuning, Run(0, 100.f, FVector(1.0, 0.0, 0.0), true), State);
		TestEqual(TEXT("Disabled: no boost"), (int32)State.Stage, 0);
	}
	{
		const FGSTN_RunStagesTuning Tuning = TestTuning();
		FGSTN_RunStageState State;
		FGSTN_RunStageLogic::Step(Tuning, Run(0, 0.f, FVector::ZeroVector, true), State);
		TestEqual(TEXT("boost with the stick released: nothing"), (int32)State.Stage, 0);
	}
	{
		// Owner 2026-11-17: the target is data, counted from 1; out of range is clamped; a boost never lowers the stage
		FGSTN_RunStagesTuning Tuning = TestTuning(EGSTN_RunBoostMode::Anytime);
		TestEqual(TEXT("default target = stage 2 (sprint 1), index 1"), Tuning.GetBoostTargetIndex(), 1);
		Tuning.BoostTargetStage = 3;
		FGSTN_RunStageState Last;
		FGSTN_RunStageLogic::Step(Tuning, Run(0, 100.f, FVector(1.0, 0.0, 0.0), true), Last);
		TestEqual(TEXT("BoostTargetStage 3: the last stage (the old rule)"), (int32)Last.Stage, 2);
		Tuning.BoostTargetStage = 99;
		TestEqual(TEXT("a target past the table is clamped to the last stage"), Tuning.GetBoostTargetIndex(), 2);
		Tuning.BoostTargetStage = 2;
		FGSTN_RunStageState High;
		High.Stage = 2;
		FGSTN_RunStageLogic::Step(Tuning, Run(0, 800.f, FVector(1.0, 0.0, 0.0), true), High);
		TestEqual(TEXT("boost at stage 3 does not drop it to stage 2"), (int32)High.Stage, 2);
		Tuning.BoostTargetStage = 1;
		FGSTN_RunStageState First;
		FGSTN_RunStageLogic::Step(Tuning, Run(0, 100.f, FVector(1.0, 0.0, 0.0), true), First);
		TestEqual(TEXT("BoostTargetStage 1: the boost does nothing"), (int32)First.Stage, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunStagesDropTest, "GSTNCombat.Core.RunStages.Drop", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RunStagesDropTest::RunTest(const FString& Parameters)
{
	const FGSTN_RunStagesTuning Tuning = TestTuning();

	// Stick release: the speed of before the release is carried, the stage holds ReleaseDropFrames, then stage 1
	{
		FGSTN_RunStageState State;
		int32 Frame = RunAtStageSpeed(Tuning, State, 0, 7);
		TestEqual(TEXT("at stage 3 before the release"), (int32)State.Stage, 2);
		const float Before = Tuning.Stages[2].MaxSpeed;
		FGSTN_RunStageLogic::Step(Tuning, Run(Frame++, Before * 0.8f, FVector::ZeroVector), State);
		TestEqual(TEXT("carried = speed of the frame before the release"), State.CarriedSpeed, Before);
		TestEqual(TEXT("carried within the stage's CarryFrames"), FGSTN_RunStageLogic::GetCarriedSpeed(Tuning, State), Before);
		for (int32 i = 0; i <= Tuning.Stages[2].CarryFrames; ++i)
		{
			FGSTN_RunStageLogic::Step(Tuning, Run(Frame++, 100.f, FVector::ZeroVector), State);
		}
		TestEqual(TEXT("past CarryFrames: nothing carried"), FGSTN_RunStageLogic::GetCarriedSpeed(Tuning, State), 0.f);
		TestEqual(TEXT("released longer than ReleaseDropFrames: stage 1"), (int32)State.Stage, 0);
	}
	// A short release keeps the stage
	{
		FGSTN_RunStageState State;
		int32 Frame = RunAtStageSpeed(Tuning, State, 0, 7);
		FGSTN_RunStageLogic::Step(Tuning, Run(Frame++, 800.f, FVector::ZeroVector), State);
		FGSTN_RunStageLogic::Step(Tuning, Run(Frame++, 800.f), State);
		TestEqual(TEXT("one frame released: stage kept"), (int32)State.Stage, 2);
	}
	// Tight turn: one stage down
	{
		FGSTN_RunStageState State;
		int32 Frame = RunAtStageSpeed(Tuning, State, 0, 7);
		FGSTN_RunStageLogic::Step(Tuning, Run(Frame++, Tuning.Stages[2].MaxSpeed, FVector(-1.0, 0.0, 0.0)), State);
		TestEqual(TEXT("stick against the velocity: one stage down"), (int32)State.Stage, 1);
	}
	// Bump: most of the speed lost in one frame
	{
		FGSTN_RunStageState State;
		int32 Frame = RunAtStageSpeed(Tuning, State, 0, 7);
		FGSTN_RunStageLogic::Step(Tuning, Run(Frame++, 50.f), State);
		TestEqual(TEXT("a bump: stage 1"), (int32)State.Stage, 0);
	}
	// In the air the stage is kept
	{
		FGSTN_RunStageState State;
		int32 Frame = RunAtStageSpeed(Tuning, State, 0, 7);
		for (int32 i = 0; i < 20; ++i)
		{
			FGSTN_RunFrameInput Air = Run(Frame++, 0.f, FVector::ZeroVector);
			Air.bGrounded = false;
			FGSTN_RunStageLogic::Step(Tuning, Air, State);
		}
		TestEqual(TEXT("airborne with the stick released: stage kept"), (int32)State.Stage, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunStagesRollbackTest, "GSTNCombat.Core.RunStages.Rollback", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RunStagesRollbackTest::RunTest(const FString& Parameters)
{
	const FGSTN_RunStagesTuning Tuning = TestTuning();

	// A fixed input sequence: climb, boost, release, push again
	TArray<FGSTN_RunFrameInput> Inputs;
	for (int32 Frame = 0; Frame < 40; ++Frame)
	{
		const bool bReleased = Frame >= 20 && Frame < 23;
		const float Speed = Frame < 10 ? 450.f : 700.f;
		Inputs.Add(Run(Frame, bReleased ? 600.f : Speed, bReleased ? FVector::ZeroVector : FVector(1.0, 0.0, 0.0), Frame == 25));
	}

	FGSTN_RunStageState State;
	FGSTN_RunStageState Saved;
	TArray<FGSTN_RunStageState> Predicted;
	for (const FGSTN_RunFrameInput& Input : Inputs)
	{
		FGSTN_RunStageLogic::Step(Tuning, Input, State);
		Predicted.Add(State);
		if (Input.Frame == 15)
		{
			Saved = State;
		}
	}

	// Rollback to the end of frame 15 and replay: every frame matches, the stage is restored then stepped again
	FGSTN_RunStageState Replay = Saved;
	TestEqual(TEXT("the restored stage is the saved one"), (int32)Replay.Stage, (int32)Predicted[15].Stage);
	bool bAllEqual = true;
	for (int32 Frame = 16; Frame < Inputs.Num(); ++Frame)
	{
		FGSTN_RunStageLogic::Step(Tuning, Inputs[Frame], Replay);
		bAllEqual &= Replay == Predicted[Frame];
	}
	TestTrue(TEXT("re-simulation gives the same state every frame"), bAllEqual);

	// Substeps of one frame step once
	FGSTN_RunStageState Once = Saved;
	FGSTN_RunStageLogic::Step(Tuning, Inputs[16], Once);
	const FGSTN_RunStageState AfterOne = Once;
	FGSTN_RunStageLogic::Step(Tuning, Inputs[16], Once);
	TestTrue(TEXT("a second substep of the same frame changes nothing"), Once == AfterOne);

	// Network form: what the authority sends is what the client reads (speeds to 0.1 cm/s)
	FGSTN_RunStageState Sent = Predicted[21];
	Sent.CarriedSpeed = 612.34f;
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	Sent.SerializeCompact(Writer);
	FGSTN_RunStageState Received;
	FMemoryReader Reader(Bytes);
	Received.SerializeCompact(Reader);
	TestEqual(TEXT("stage survives the network"), (int32)Received.Stage, (int32)Sent.Stage);
	TestEqual(TEXT("counters survive"), Received.FramesSinceStickRelease, Sent.FramesSinceStickRelease);
	TestEqual(TEXT("carry source survives"), (int32)Received.CarrySource, (int32)Sent.CarrySource);
	TestTrue(TEXT("carried speed to 0.1 cm/s"), FMath::IsNearlyEqual(Received.CarriedSpeed, 612.3f, 0.01f));
	TestEqual(TEXT("frame survives"), Received.LastFrame, Sent.LastFrame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunStagesAccelRampTest, "GSTNCombat.Core.RunStages.AccelRamp", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RunStagesAccelRampTest::RunTest(const FString& Parameters)
{
	FGSTN_RunStagesTuning Tuning = TestTuning();
	FGSTN_RunStageState State;
	TestEqual(TEXT("no ramp authored = full acceleration"), FGSTN_RunStageLogic::GetAccelerationRamp(Tuning, State), 1.f);

	Tuning.Stages[0].AccelRampFrames = 4;
	Tuning.Stages[0].RampStartFraction = 0.2f;
	TestEqual(TEXT("at rest: the first ramp frame"), FGSTN_RunStageLogic::GetAccelerationRamp(Tuning, State), 0.2f, 1e-4f);

	// A light tilt (under the stages' "released" threshold) still ramps, and the ramp grows without a step
	const FVector Light(0.1, 0.0, 0.0);
	float Prev = FGSTN_RunStageLogic::GetAccelerationRamp(Tuning, State);
	for (int32 Frame = 0; Frame < 4; ++Frame)
	{
		FGSTN_RunStageLogic::Step(Tuning, Run(Frame, 20.f * Frame, Light), State);
		const float Ramp = FGSTN_RunStageLogic::GetAccelerationRamp(Tuning, State);
		TestTrue(*FString::Printf(TEXT("ramp grows, frame %d"), Frame), Ramp > Prev && Ramp - Prev <= 0.2f + 1e-4f);
		Prev = Ramp;
	}
	TestEqual(TEXT("ramp done"), Prev, 1.f, 1e-4f);

	// Stick back at rest: the next push ramps again
	FGSTN_RunStageLogic::Step(Tuning, Run(4, 0.f, FVector::ZeroVector), State);
	TestEqual(TEXT("released: back to the start"), FGSTN_RunStageLogic::GetAccelerationRamp(Tuning, State), 0.2f, 1e-4f);

	// The state round-trips through the network form
	FGSTN_RunStageLogic::Step(Tuning, Run(5, 10.f, Light), State);
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	State.SerializeCompact(Writer);
	FGSTN_RunStageState Read;
	FMemoryReader Reader(Bytes);
	Read.SerializeCompact(Reader);
	TestEqual(TEXT("ramp counter survives the network form"), Read.RampFrames, State.RampFrames);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunStagesStartStampTest, "GSTNCombat.Core.RunStages.StartStamp", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RunStagesStartStampTest::RunTest(const FString& Parameters)
{
	// Owner choice B (2026-11-18): a hard push from still stamps almost in place for Frames, then the stage 1 ramp starts from its beginning
	FGSTN_RunStagesTuning Tuning = TestTuning();
	Tuning.StartStamp.Frames = 3;
	Tuning.StartStamp.AccelFraction = 0.05f;
	Tuning.StartStamp.StickMin = 0.5f;
	Tuning.StartStamp.MaxStartSpeed = 60.f;
	Tuning.Stages[0].AccelRampFrames = 4;
	Tuning.Stages[0].RampStartFraction = 0.2f;

	FGSTN_RunStageState State;
	TestEqual(TEXT("still, stick at rest: frame 0 already cut"), FGSTN_RunStageLogic::GetAccelerationRamp(Tuning, State), 0.05f, 1e-4f);
	FGSTN_RunStageLogic::Step(Tuning, Run(0, 1.f), State);
	TestEqual(TEXT("stamp started on the push"), State.StampFrames, 0);
	TestTrue(TEXT("stamp live"), FGSTN_RunStageLogic::IsStartStamp(Tuning, State));
	TestEqual(TEXT("stamp frame 1 cut"), FGSTN_RunStageLogic::GetAccelerationRamp(Tuning, State), 0.05f, 1e-4f);
	FGSTN_RunStageLogic::Step(Tuning, Run(1, 2.f), State);
	TestEqual(TEXT("stamp frame 2 cut"), FGSTN_RunStageLogic::GetAccelerationRamp(Tuning, State), 0.05f, 1e-4f);
	FGSTN_RunStageLogic::Step(Tuning, Run(2, 3.f), State);
	TestTrue(TEXT("last stamp frame stepped: still live in its end state"), FGSTN_RunStageLogic::IsStartStamp(Tuning, State));
	TestEqual(TEXT("after the stamp: the ramp's first frame, no step"), FGSTN_RunStageLogic::GetAccelerationRamp(Tuning, State), 0.2f, 1e-4f);
	FGSTN_RunStageLogic::Step(Tuning, Run(3, 10.f), State);
	TestFalse(TEXT("stamp over"), FGSTN_RunStageLogic::IsStartStamp(Tuning, State));
	TestTrue(TEXT("the ramp grows after the stamp"), FGSTN_RunStageLogic::GetAccelerationRamp(Tuning, State) > 0.2f);
	TestTrue(TEXT("the counter keeps going for the clip time"), State.StampFrames == 3);

	// One push stamps once: a pause in speed under MaxStartSpeed (a wall) does not stamp again
	FGSTN_RunStageLogic::Step(Tuning, Run(4, 0.f), State);
	TestFalse(TEXT("no second stamp on the same push"), FGSTN_RunStageLogic::IsStartStamp(Tuning, State));

	// A light tilt walks without the stamp, and only its first frame lost acceleration
	FGSTN_RunStageLogic::Step(Tuning, Run(5, 0.f, FVector::ZeroVector), State);
	TestEqual(TEXT("stick at rest resets"), State.StampFrames, -1);
	FGSTN_RunStageLogic::Step(Tuning, Run(6, 1.f, FVector(0.3, 0.0, 0.0)), State);
	TestEqual(TEXT("light tilt: no stamp"), State.StampFrames, -1);
	TestEqual(TEXT("light tilt: the ramp as usual"), FGSTN_RunStageLogic::GetAccelerationRamp(Tuning, State), 0.2f + 0.8f * 0.25f, 1e-4f);

	// A re-push while running never stamps
	FGSTN_RunStageState Running;
	Running.PrevSpeed = 400.f;
	FGSTN_RunStageLogic::Step(Tuning, Run(0, 400.f), Running);
	FGSTN_RunStageLogic::Step(Tuning, Run(1, 380.f, FVector::ZeroVector), Running);
	TestEqual(TEXT("running: not cut"), FGSTN_RunStageLogic::GetAccelerationRamp(Tuning, Running), 0.2f, 1e-4f);
	FGSTN_RunStageLogic::Step(Tuning, Run(2, 380.f), Running);
	TestEqual(TEXT("running re-push: no stamp"), Running.StampFrames, -1);

	// Stamp off by default (a profile without the setting keeps today's start)
	FGSTN_RunStageState Plain;
	TestEqual(TEXT("off: no cut"), FGSTN_RunStageLogic::GetAccelerationRamp(TestTuning(), Plain), 1.f);

	// The counter survives the network form
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	State.StampFrames = 2;
	State.SerializeCompact(Writer);
	FGSTN_RunStageState Read;
	FMemoryReader Reader(Bytes);
	Read.SerializeCompact(Reader);
	TestEqual(TEXT("stamp counter survives the network form"), Read.StampFrames, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunStagesBoostDashTest, "GSTNCombat.Core.RunStages.BoostDash", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RunStagesBoostDashTest::RunTest(const FString& Parameters)
{
	// Owner 2026-11-19: L3 skips the stamp and launches at once (Smash's dash)
	FGSTN_RunStagesTuning Tuning = TestTuning();
	Tuning.StartStamp.Frames = 21;
	Tuning.BoostWindowFrames = 2;
	Tuning.BoostDash.Frames = 3;
	Tuning.BoostDash.AccelMultiplier = 8.f;

	FGSTN_RunStageState State;
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		FGSTN_RunStageLogic::Step(Tuning, Run(Frame, 1.f), State);
	}
	TestTrue(TEXT("stamping, past the plain boost window"), FGSTN_RunStageLogic::IsStartStamp(Tuning, State));
	FGSTN_RunStageLogic::Step(Tuning, Run(10, 1.f, FVector(1.0, 0.0, 0.0), true), State);
	TestEqual(TEXT("the boost works during the whole stamp"), (int32)State.Stage, Tuning.GetBoostTargetIndex());
	TestFalse(TEXT("the stamp is over"), FGSTN_RunStageLogic::IsStartStamp(Tuning, State));
	TestEqual(TEXT("no run start time after the boost"), State.StampFrames, -1);
	TestTrue(TEXT("burst live"), FGSTN_RunStageLogic::IsBoostDash(Tuning, State));
	TestEqual(TEXT("burst acceleration"), FGSTN_RunStageLogic::GetAccelerationRamp(Tuning, State), 8.f, 1e-4f);
	for (int32 Frame = 11; Frame < 14; ++Frame)
	{
		FGSTN_RunStageLogic::Step(Tuning, Run(Frame, 300.f), State);
	}
	TestFalse(TEXT("burst over after its frames"), FGSTN_RunStageLogic::IsBoostDash(Tuning, State));
	TestEqual(TEXT("no new stamp on the boosted stage"), State.StampFrames, -1);
	FGSTN_RunStageLogic::Step(Tuning, Run(14, 300.f, FVector::ZeroVector), State);
	TestEqual(TEXT("stick at rest clears the dash"), State.DashFrames, -1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RunStagesReversalTest, "GSTNCombat.Core.RunStages.Reversal", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RunStagesReversalTest::RunTest(const FString& Parameters)
{
	// skid-reversal-1: the stick flipped against a run starts the reversal (multiplier, no ramp), the run the other way ends it; and the
	// ramp restarted by the tight turn's stage drop no longer waits the run start stamp again
	FGSTN_RunStagesTuning Tuning = TestTuning();
	Tuning.StartStamp.Frames = 21;
	for (FGSTN_RunStage& Stage : Tuning.Stages)
	{
		Stage.AccelRampFrames = 6;
		Stage.RampStartFraction = 0.3f;
	}
	const FVector Back(-1.0, 0.0, 0.0);
	const float Top = Tuning.Stages[0].MaxSpeed;
	Tuning.TightTurnDropStages = 3;

	// Off: the stamp-ramp fix alone. A push from rest stamps, the run reaches stage 2, then the stick flips
	FGSTN_RunStageState State;
	FGSTN_RunStageLogic::Step(Tuning, Run(0, 1.f), State);
	int32 Frame = 1;
	for (; Frame < 40; ++Frame)
	{
		FGSTN_RunStageLogic::Step(Tuning, Run(Frame, Tuning.GetStage(State.Stage).MaxSpeed), State);
	}
	TestTrue(TEXT("climbed past stage 1"), State.Stage > 0);
	FGSTN_RunStageLogic::Step(Tuning, Run(Frame++, Top, Back), State);
	TestEqual(TEXT("the flip drops the run to stage 1"), (int32)State.Stage, 0);
	for (int32 K = 0; K < 6; ++K)
	{
		FGSTN_RunStageLogic::Step(Tuning, Run(Frame++, -100.f, Back), State);
	}
	TestEqual(TEXT("the restarted ramp is done after its 6 frames, not 21 + 6"), FGSTN_RunStageLogic::GetAccelerationRamp(Tuning, State), 1.f, 1e-4f);
	TestFalse(TEXT("off: no reversal"), FGSTN_RunStageLogic::IsReversing(Tuning, State));

	// On
	Tuning.Reversal.bEnabled = true;
	Tuning.Reversal.AccelerationMultiplier = 2.5f;
	Tuning.Reversal.MaxFrames = 30;
	FGSTN_RunStageState Rev;
	RunAtStageSpeed(Tuning, Rev, 0, 10);
	FGSTN_RunStageLogic::Step(Tuning, Run(10, Top, FVector(-0.2, 1.0, 0.0).GetSafeNormal()), Rev);
	TestFalse(TEXT("a side push is no reversal"), FGSTN_RunStageLogic::IsReversing(Tuning, Rev));
	FGSTN_RunStageLogic::Step(Tuning, Run(11, Top, Back * 0.3), Rev);
	TestFalse(TEXT("a light tilt back is no reversal"), FGSTN_RunStageLogic::IsReversing(Tuning, Rev));
	FGSTN_RunStageLogic::Step(Tuning, Run(12, Top, Back), Rev);
	TestTrue(TEXT("full stick back: reversal"), FGSTN_RunStageLogic::IsReversing(Tuning, Rev));
	TestEqual(TEXT("reversal acceleration"), FGSTN_RunStageLogic::GetAccelerationRamp(Tuning, Rev), 2.5f, 1e-4f);
	FGSTN_RunStageLogic::Step(Tuning, Run(13, -Top * 0.5f, Back), Rev);
	TestTrue(TEXT("still under the end speed the other way"), FGSTN_RunStageLogic::IsReversing(Tuning, Rev));
	FGSTN_RunStageLogic::Step(Tuning, Run(14, -Top, Back), Rev);
	TestFalse(TEXT("running the other way: over"), FGSTN_RunStageLogic::IsReversing(Tuning, Rev));

	FGSTN_RunStageLogic::Step(Tuning, Run(15, -Top, FVector(1.0, 0.0, 0.0)), Rev);
	TestTrue(TEXT("flip again"), FGSTN_RunStageLogic::IsReversing(Tuning, Rev));
	for (int32 K = 16; K < 16 + 31; ++K)
	{
		FGSTN_RunStageLogic::Step(Tuning, Run(K, -10.f, FVector(1.0, 0.0, 0.0)), Rev);
	}
	TestFalse(TEXT("MaxFrames ends a stuck reversal"), FGSTN_RunStageLogic::IsReversing(Tuning, Rev));

	FGSTN_RunStageState Net;
	Net.ReverseFrames = 7;
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	Net.SerializeCompact(Writer);
	FGSTN_RunStageState Read;
	FMemoryReader Reader(Bytes);
	Read.SerializeCompact(Reader);
	TestEqual(TEXT("reversal counter survives the network form"), Read.ReverseFrames, 7);
	return true;
}

#endif