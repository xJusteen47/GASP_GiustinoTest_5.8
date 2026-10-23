#include "Tests/GSTN_CombatTestUtils.h"
#include "Jump/GSTN_GravityPhases.h"
#include "Jump/GSTN_JumpLogic.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Gravity directions the jump must work under: world down, tilted 45 degrees, sideways, inverted */
	TArray<FVector> TestGravityDirections()
	{
		return { FVector(0.0, 0.0, -1.0), FVector(0.70710678, 0.0, -0.70710678).GetSafeNormal(), FVector(1.0, 0.0, 0.0), FVector(0.0, 0.0, 1.0), FVector(0.3, -0.5, -0.8).GetSafeNormal() };
	}

	FGSTN_JumpState AirborneRisingState(int32 AirJumpsUsed, int32 Coyote)
	{
		FGSTN_JumpState State = FGSTN_JumpLogic::ApplyTakeoff(FGSTN_JumpState(), 10, 5, EGSTN_JumpKind::Ground);
		State.AirJumpsUsed = static_cast<uint8>(AirJumpsUsed);
		State.CoyoteFramesLeft = static_cast<uint8>(Coyote);
		return State;
	}

	FGSTN_JumpMoverSample LaunchSample(double UpSpeed)
	{
		FGSTN_JumpMoverSample Sample;
		Sample.bGrounded = false;
		Sample.UpSpeed = UpSpeed;
		Sample.bExternalLaunch = true;
		return Sample;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_JumpTiltedGravityApexTest, "GSTNCombat.Core.Jump.TiltedGravityApex", GSTN_CORE_TEST_FLAGS)
bool FGSTN_JumpTiltedGravityApexTest::RunTest(const FString& Parameters)
{
	const FGSTN_JumpParams Params;
	const FGSTN_JumpApex Reference = FGSTN_JumpLogic::SimulateApex(FVector(0.0, 0.0, -1.0), 980.0, 700.0, 12, Params);
	TestTrue(TEXT("the reference jump rises"), Reference.Height > 50.0);

	for (const FVector& Gravity : TestGravityDirections())
	{
		const FString Label = Gravity.ToString();
		const FGSTN_JumpApex Apex = FGSTN_JumpLogic::SimulateApex(Gravity, 980.0, 700.0, 12, Params);
		TestTrue(*FString::Printf(TEXT("the apex height does not depend on the gravity direction (%s)"), *Label), FMath::IsNearlyEqual(Apex.Height, Reference.Height, 1e-6 * Reference.Height));
		TestEqual(*FString::Printf(TEXT("same frames to the apex (%s)"), *Label), Apex.Frames, Reference.Frames);

		// The whole displacement is along up: nothing sideways from a jump straight up
		const FVector Up = -Gravity;
		const FVector Lateral = Apex.Position - Up * Apex.Position.Dot(Up);
		TestTrue(*FString::Printf(TEXT("the jump goes along local up, not world Z (%s)"), *Label), Lateral.Size() < 1e-6 * Apex.Height);
	}

	// Continuous variable height: the longer the hold, the higher; the cap of MaxJumpHoldFrames ends it
	const FVector Tilted = TestGravityDirections()[1];
	double Previous = -1.0;
	for (const int32 Hold : { 0, 4, 6, 8, 10, 11 })
	{
		const double Height = FGSTN_JumpLogic::SimulateApex(Tilted, 980.0, 700.0, Hold, Params).Height;
		TestTrue(*FString::Printf(TEXT("a hold of %d frames is higher than the shorter ones (%.1f > %.1f)"), Hold, Height, Previous), Height > Previous);
		Previous = Height;
	}
	TestTrue(TEXT("holding past the cap of MaxJumpHoldFrames adds nothing"),
		FMath::IsNearlyEqual(FGSTN_JumpLogic::SimulateApex(Tilted, 980.0, 700.0, 12, Params).Height, FGSTN_JumpLogic::SimulateApex(Tilted, 980.0, 700.0, 40, Params).Height));

	// Inverted gravity: up is world down, and the speed along up of a velocity toward the ceiling is positive
	TestTrue(TEXT("under inverted gravity moving toward world +Z is moving down"), FGSTN_JumpLogic::UpSpeed(FVector(0.0, 0.0, 300.0), FVector(0.0, 0.0, 1.0)) < 0.0);
	TestTrue(TEXT("under inverted gravity moving toward world -Z is moving up"), FGSTN_JumpLogic::UpSpeed(FVector(0.0, 0.0, -300.0), FVector(0.0, 0.0, 1.0)) > 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_JumpExternalLaunchResetsTest, "GSTNCombat.Core.Jump.ExternalLaunchResets", GSTN_CORE_TEST_FLAGS)
bool FGSTN_JumpExternalLaunchResetsTest::RunTest(const FString& Parameters)
{
	FGSTN_JumpParams Params;
	Params.AirJumpCount = 2;
	const FGSTN_JumpResetRules Defaults;

	// Default rules: refill the air jumps, clear the coyote window, end the hold phase of the jump in progress
	{
		const FGSTN_JumpState Out = FGSTN_JumpLogic::Step(AirborneRisingState(2, 3), 14, LaunchSample(900.0), true, Params, Defaults);
		TestEqual(TEXT("default: the air jumps are available again"), static_cast<int32>(Out.AirJumpsUsed), 0);
		TestEqual(TEXT("default: the coyote window is cleared"), static_cast<int32>(Out.CoyoteFramesLeft), 0);
		TestTrue(TEXT("default: the jump hold phase ends, plain gravity for a launch"), Out.Phase == EGSTN_JumpPhase::Launched && !Out.bJumpCut && Out.StartFrame == INDEX_NONE);
		TestEqual(TEXT("default: gravity scale of a launch is 1"), FGSTN_JumpLogic::GravityScale(Out, Params), 1.f);
		TestTrue(TEXT("after the launch the next jump is an air jump, not a ground jump"), FGSTN_JumpLogic::ClassifyJump(Out, false, Params) == EGSTN_JumpKind::Air);
	}

	// Keep / Set N
	{
		FGSTN_JumpResetRules Rules;
		Rules.AirJumpsOnExternalLaunch = EGSTN_AirJumpsOnLaunch::Keep;
		Rules.CoyoteOnExternalLaunch = EGSTN_CoyoteOnLaunch::Keep;
		const FGSTN_JumpState Kept = FGSTN_JumpLogic::Step(AirborneRisingState(2, 3), 14, LaunchSample(900.0), true, Params, Rules);
		TestEqual(TEXT("Keep: the air jumps used stay"), static_cast<int32>(Kept.AirJumpsUsed), 2);
		TestEqual(TEXT("Keep: the coyote window runs on (one frame passed)"), static_cast<int32>(Kept.CoyoteFramesLeft), 2);
		TestTrue(TEXT("Keep: a ground jump is still allowed inside the window"), FGSTN_JumpLogic::ClassifyJump(Kept, false, Params) == EGSTN_JumpKind::Ground);

		Rules.AirJumpsOnExternalLaunch = EGSTN_AirJumpsOnLaunch::SetCount;
		Rules.AirJumpsSetCount = 1;
		TestEqual(TEXT("Set 1 of 2: one air jump used"), static_cast<int32>(FGSTN_JumpLogic::Step(AirborneRisingState(2, 0), 14, LaunchSample(900.0), true, Params, Rules).AirJumpsUsed), 1);
		Rules.AirJumpsSetCount = 5;
		TestEqual(TEXT("Set 5 of 2: capped, none used"), static_cast<int32>(FGSTN_JumpLogic::Step(AirborneRisingState(2, 0), 14, LaunchSample(900.0), true, Params, Rules).AirJumpsUsed), 0);
	}

	// The hold phase can be left running
	{
		FGSTN_JumpResetRules Rules;
		Rules.bCancelHoldPhaseOnExternalLaunch = false;
		const FGSTN_JumpState Out = FGSTN_JumpLogic::Step(AirborneRisingState(0, 0), 14, LaunchSample(900.0), true, Params, Rules);
		TestTrue(TEXT("hold phase kept: still rising from the jump's own start frame"), Out.Phase == EGSTN_JumpPhase::Rising && Out.StartFrame == 10);
	}

	// A launch downward is a fall; it ends a fast fall
	{
		FGSTN_JumpState Fast = FGSTN_JumpLogic::ApplyFastFall(AirborneRisingState(0, 0));
		Fast.Phase = EGSTN_JumpPhase::Falling;
		const FGSTN_JumpState Out = FGSTN_JumpLogic::Step(Fast, 14, LaunchSample(-400.0), false, Params, Defaults);
		TestTrue(TEXT("a launch toward the ground is falling and not fast falling"), Out.Phase == EGSTN_JumpPhase::Falling && !Out.bFastFalling);
	}

	// Without the launch flag nothing resets: the same velocity is just a jump going on
	{
		FGSTN_JumpMoverSample Plain = LaunchSample(900.0);
		Plain.bExternalLaunch = false;
		const FGSTN_JumpState Out = FGSTN_JumpLogic::Step(AirborneRisingState(2, 0), 14, Plain, true, Params, Defaults);
		TestEqual(TEXT("no launch: the air jumps used are not refilled"), static_cast<int32>(Out.AirJumpsUsed), 2);
		TestTrue(TEXT("no launch: the jump still rises"), Out.Phase == EGSTN_JumpPhase::Rising);
	}

	// Landing resets everything and opens the coyote window
	{
		FGSTN_JumpMoverSample Ground;
		Ground.bGrounded = true;
		const FGSTN_JumpState Out = FGSTN_JumpLogic::Step(AirborneRisingState(2, 0), 20, Ground, false, Params, Defaults);
		TestTrue(TEXT("on the ground: no jump, no air jumps used"), Out.Phase == EGSTN_JumpPhase::None && Out.AirJumpsUsed == 0 && !Out.bJumpCut);
		TestEqual(TEXT("the coyote window opens for the next ledge"), static_cast<int32>(Out.CoyoteFramesLeft), Params.CoyoteFrames);
	}

	// The state's wire form round trips
	{
		FGSTN_JumpState Source = FGSTN_JumpLogic::ApplyTakeoff(AirborneRisingState(1, 2), 33, 5, EGSTN_JumpKind::Air);
		Source.bFastFalling = true;
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes);
		Source.NetSerialize(Writer);
		FGSTN_JumpState Loaded;
		FMemoryReader Reader(Bytes);
		Loaded.NetSerialize(Reader);
		TestTrue(TEXT("the jump state survives the wire"), Loaded == Source);

		TArray<uint8> DefaultBytes;
		FMemoryWriter DefaultWriter(DefaultBytes);
		FGSTN_JumpState().NetSerialize(DefaultWriter);
		DefaultWriter.Flush();
		TestEqual(TEXT("a default jump state is one byte at most (one bit)"), DefaultBytes.Num(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GravityPhasesFollowLocalGravityTest, "GSTNCombat.Core.Gravity.PhasesFollowLocalGravity", GSTN_CORE_TEST_FLAGS)
bool FGSTN_GravityPhasesFollowLocalGravityTest::RunTest(const FString& Parameters)
{
	const FGSTN_JumpParams Params;
	for (const FVector& Direction : TestGravityDirections())
	{
		const FVector Base = Direction * 980.0;
		FGSTN_GravityBase Tracker;
		bool bWrite = false;

		FVector Out = FGSTN_GravityPhases::Resolve(Tracker, Base, 1.f, bWrite);
		TestFalse(TEXT("scale 1 over the authored gravity writes nothing"), bWrite);
		TestTrue(TEXT("and keeps the gravity it has"), Out.Equals(Base, 1e-6));

		Out = FGSTN_GravityPhases::Resolve(Tracker, Base, Params.FallGravityScale, bWrite);
		TestTrue(TEXT("the fall scale is written"), bWrite);
		TestTrue(TEXT("same direction as the local gravity, size scaled"), Out.Equals(Base * Params.FallGravityScale, 1e-6) && Out.GetSafeNormal().Equals(Direction, 1e-9));

		// The next frame Mover reports what was written: the base is not scaled again
		Out = FGSTN_GravityPhases::Resolve(Tracker, Out, Params.CutGravityScale, bWrite);
		TestTrue(TEXT("a written value is not read back as the authored gravity (no compounding)"), bWrite && Out.Equals(Base * Params.CutGravityScale, 1e-6));

		Out = FGSTN_GravityPhases::Resolve(Tracker, Out, 1.f, bWrite);
		TestTrue(TEXT("back to scale 1 writes the authored gravity back once"), bWrite && Out.Equals(Base, 1e-6));
		Out = FGSTN_GravityPhases::Resolve(Tracker, Out, 1.f, bWrite);
		TestFalse(TEXT("and then nothing"), bWrite);

		// Something else changes the gravity (a level object tilts it): the new one is the base and the phases follow it
		const FVector Flipped = FVector(-Direction.Y, Direction.X, -Direction.Z) * 700.0;
		Out = FGSTN_GravityPhases::Resolve(Tracker, Flipped, Params.FastFallGravityScale, bWrite);
		TestTrue(TEXT("a gravity set by something else becomes the base, the phase scales it in its own direction"),
			bWrite && Out.Equals(Flipped * Params.FastFallGravityScale, 1e-6) && Out.GetSafeNormal().Equals(Flipped.GetSafeNormal(), 1e-9));
	}

	// The phase scales: hold, cut, fall, fast fall, launched
	FGSTN_JumpState State = FGSTN_JumpLogic::ApplyTakeoff(FGSTN_JumpState(), 0, 0, EGSTN_JumpKind::Ground);
	TestEqual(TEXT("rising with the button held"), FGSTN_JumpLogic::GravityScale(State, Params), Params.RiseGravityScale);
	State.bJumpCut = true;
	TestEqual(TEXT("rising after the cut"), FGSTN_JumpLogic::GravityScale(State, Params), Params.CutGravityScale);
	State.Phase = EGSTN_JumpPhase::Falling;
	TestEqual(TEXT("falling"), FGSTN_JumpLogic::GravityScale(State, Params), Params.FallGravityScale);
	State = FGSTN_JumpLogic::ApplyFastFall(State);
	TestEqual(TEXT("fast falling"), FGSTN_JumpLogic::GravityScale(State, Params), Params.FastFallGravityScale);

	// The phases are decided from the speed along local up: a character moving toward the sideways ground is falling
	FGSTN_JumpState Standing;
	FGSTN_JumpMoverSample Sample;
	Sample.bGrounded = false;
	Sample.UpSpeed = FGSTN_JumpLogic::UpSpeed(FVector(500.0, 0.0, 0.0), FVector(1.0, 0.0, 0.0));
	TestTrue(TEXT("toward the sideways ground is down"), Sample.UpSpeed < 0.0);
	Standing = FGSTN_JumpLogic::Step(Standing, 1, Sample, false, Params, FGSTN_JumpResetRules());
	TestTrue(TEXT("airborne and moving down is falling under any gravity"), Standing.Phase == EGSTN_JumpPhase::Falling && FGSTN_JumpLogic::CanFastFall(Standing, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_JumpTerminalFallSpeedTest, "GSTNCombat.Core.Jump.TerminalFallSpeed", GSTN_CORE_TEST_FLAGS)
bool FGSTN_JumpTerminalFallSpeedTest::RunTest(const FString& Parameters)
{
	const double G = 980.0, Dt = 1.0 / 60.0;
	FGSTN_JumpParams Params;
	Params.MaxFallSpeed = 1500.f;
	Params.FastFallSpeed = 2200.f;

	// Falls under the scale of the state until the terminal speed, and stays on it: reached exactly, never passed
	auto Fall = [&](const FGSTN_JumpState& State, int32 Frames)
	{
		double Down = 0.0, Peak = 0.0;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Down += G * FGSTN_JumpLogic::LimitedGravityScale(State, Params, Down, G, Dt) * Dt;
			Peak = FMath::Max(Peak, Down);
		}
		return TPair<double, double>(Down, Peak);
	};

	FGSTN_JumpState Falling;
	Falling.Phase = EGSTN_JumpPhase::Falling;
	const TPair<double, double> Plain = Fall(Falling, 600);
	TestTrue(TEXT("a plain fall ends on MaxFallSpeed and never passes it"), FMath::IsNearlyEqual(Plain.Key, 1500.0, 0.01) && Plain.Value <= 1500.0 + 0.01);

	FGSTN_JumpState Fast = Falling;
	Fast.bFastFalling = true;
	const TPair<double, double> FastFall = Fall(Fast, 600);
	TestTrue(TEXT("a fast fall ends on FastFallSpeed and never passes it"), FMath::IsNearlyEqual(FastFall.Key, 2200.0, 0.01) && FastFall.Value <= 2200.0 + 0.01);

	TestEqual(TEXT("far from the cap the scale is the plain gravity scale"), FGSTN_JumpLogic::LimitedGravityScale(Falling, Params, 100.0, G, Dt), FGSTN_JumpLogic::GravityScale(Falling, Params));
	TestEqual(TEXT("a speed already past the cap is kept: no gravity"), FGSTN_JumpLogic::LimitedGravityScale(Falling, Params, 1800.0, G, Dt), 0.f);

	Params.MaxFallSpeed = 0.f;
	TestEqual(TEXT("0 = no limit"), FGSTN_JumpLogic::LimitedGravityScale(Falling, Params, 5000.0, G, Dt), FGSTN_JumpLogic::GravityScale(Falling, Params));

	// The same under another gravity magnitude and step: the cap is a speed, not a frame count
	Params.MaxFallSpeed = 900.f;
	double Down = 0.0;
	for (int32 Frame = 0; Frame < 600; ++Frame)
	{
		Down += 2000.0 * FGSTN_JumpLogic::LimitedGravityScale(Falling, Params, Down, 2000.0, 1.0 / 30.0) * (1.0 / 30.0);
	}
	TestTrue(TEXT("with another gravity and step it ends on the cap"), FMath::IsNearlyEqual(Down, 900.0, 0.01));
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_JumpSquatDelaysImpulseTest, "GSTNCombat.Core.Jump.SquatDelaysImpulse", GSTN_CORE_TEST_FLAGS)
bool FGSTN_JumpSquatDelaysImpulseTest::RunTest(const FString& Parameters)
{
	FGSTN_JumpMoverSample Ground;
	const FGSTN_JumpResetRules Rules;
	FGSTN_JumpParams Params;

	// 0 = today's behaviour: the request takes off at once
	FGSTN_JumpState State = FGSTN_JumpLogic::Step(FGSTN_JumpState(), 9, Ground, true, Params, Rules);
	State = FGSTN_JumpLogic::ApplyRequest(State, 10, 3, FGSTN_JumpLogic::ClassifyJump(State, true, Params), true, Params);
	TestTrue(TEXT("no squat: rising on the frame of the request"), State.Phase == EGSTN_JumpPhase::Rising && State.StartFrame == 10 && State.SquatFramesLeft == 0);

	for (const int32 Squat : { 1, 3, 7 })
	{
		Params.SquatFrames = Squat;
		State = FGSTN_JumpLogic::Step(FGSTN_JumpState(), 9, Ground, true, Params, Rules);
		State = FGSTN_JumpLogic::ApplyRequest(State, 10, 3, FGSTN_JumpLogic::ClassifyJump(State, true, Params), true, Params);
		TestTrue(TEXT("the squat starts on the request, not yet rising"), State.Phase == EGSTN_JumpPhase::None && State.SquatFramesLeft == Squat && State.Button == 3);
		TestEqual(TEXT("a second request in the squat is no jump"), static_cast<int32>(FGSTN_JumpLogic::ClassifyJump(State, true, Params)), static_cast<int32>(EGSTN_JumpKind::None));

		int32 TakeoffFrame = INDEX_NONE;
		for (int32 Frame = 11; Frame < 11 + Squat + 2 && TakeoffFrame == INDEX_NONE; ++Frame)
		{
			State = FGSTN_JumpLogic::Step(State, Frame, Ground, true, Params, Rules);
			if (State.Phase == EGSTN_JumpPhase::Rising)
			{
				TakeoffFrame = Frame;
			}
		}
		TestEqual(*FString::Printf(TEXT("squat %d takes off exactly that many frames after the request"), Squat), TakeoffFrame, 10 + Squat);
		TestTrue(TEXT("the take-off carries the button and ends the squat"), State.StartFrame == TakeoffFrame && State.Button == 3 && State.SquatFramesLeft == 0 && !State.bTakeoffWasAir);
	}

	// Knocked off the ground in the squat: no jump
	Params.SquatFrames = 5;
	State = FGSTN_JumpLogic::ApplyRequest(FGSTN_JumpState(), 10, 3, EGSTN_JumpKind::Ground, true, Params);
	FGSTN_JumpMoverSample Air;
	Air.bGrounded = false;
	Air.UpSpeed = 300.0;
	State = FGSTN_JumpLogic::Step(State, 11, Air, true, Params, Rules);
	TestTrue(TEXT("leaving the ground cancels the squat"), State.SquatFramesLeft == 0 && State.Phase != EGSTN_JumpPhase::Rising);

	// A coyote or air jump never squats
	State = FGSTN_JumpLogic::ApplyRequest(FGSTN_JumpState(), 20, 3, EGSTN_JumpKind::Ground, false, Params);
	TestTrue(TEXT("a coyote jump in the air takes off at once"), State.Phase == EGSTN_JumpPhase::Rising && State.SquatFramesLeft == 0);
	State = FGSTN_JumpLogic::ApplyRequest(FGSTN_JumpState(), 20, 3, EGSTN_JumpKind::Air, false, Params);
	TestTrue(TEXT("an air jump takes off at once"), State.Phase == EGSTN_JumpPhase::Rising && State.bTakeoffWasAir);

	// The state round trips through the wire with the squat in it
	FGSTN_JumpState Squatting = FGSTN_JumpLogic::ApplyRequest(FGSTN_JumpState(), 10, 3, EGSTN_JumpKind::Ground, true, Params);
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	Squatting.NetSerialize(Writer);
	FMemoryReader Reader(Bytes);
	FGSTN_JumpState Loaded;
	Loaded.NetSerialize(Reader);
	TestTrue(TEXT("a squatting state is not default and survives NetSerialize"), !Squatting.IsDefault() && Loaded == Squatting);
	return true;
}
#endif
