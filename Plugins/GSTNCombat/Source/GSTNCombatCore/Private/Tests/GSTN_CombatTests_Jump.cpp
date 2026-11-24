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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_JumpGlideTest, "GSTNCombat.Core.Jump.Glide", GSTN_CORE_TEST_FLAGS)
bool FGSTN_JumpGlideTest::RunTest(const FString& Parameters)
{
	// M11 L7b: the jet-pack's slow descent. Falling with the last jump's button held glides; release, fast fall or no stat do not
	FGSTN_JumpParams Params;
	Params.MaxFallSpeed = 2000.f;
	FGSTN_JumpResetRules Rules;
	FGSTN_JumpMoverSample Air;
	Air.bGrounded = false;
	Air.UpSpeed = -100.0;

	FGSTN_JumpState Falling;
	Falling.Phase = EGSTN_JumpPhase::Falling;
	Falling.StartFrame = 10;
	Falling.Button = 2;
	Falling.bJumpCut = true;

	FGSTN_JumpState Out = FGSTN_JumpLogic::Step(Falling, 40, Air, true, Params, Rules);
	TestFalse(TEXT("no glide stat: no glide"), Out.bGliding);

	Params.GlideGravityScale = 0.25f;
	Params.GlideMaxFallSpeed = 300.f;
	Out = FGSTN_JumpLogic::Step(Falling, 40, Air, true, Params, Rules);
	TestTrue(TEXT("held while falling glides"), Out.bGliding);
	TestEqual(TEXT("the glide scale applies"), FGSTN_JumpLogic::GravityScale(Out, Params), 0.25f);
	TestEqual(TEXT("past the glide cap the scale is 0 (no brake)"), FGSTN_JumpLogic::LimitedGravityScale(Out, Params, 400.0, 980.0, 1.0 / 62.5), 0.f);
	TestTrue(TEXT("under the cap the glide scale holds"), FMath::IsNearlyEqual(FGSTN_JumpLogic::LimitedGravityScale(Out, Params, 0.0, 980.0, 1.0 / 62.5), 0.25f));

	TestFalse(TEXT("released: no glide"), FGSTN_JumpLogic::Step(Falling, 40, Air, false, Params, Rules).bGliding);

	FGSTN_JumpState Fast = Falling;
	Fast.bFastFalling = true;
	TestFalse(TEXT("fast fall wins"), FGSTN_JumpLogic::Step(Fast, 40, Air, true, Params, Rules).bGliding);

	FGSTN_JumpState Ledge;
	Ledge.Phase = EGSTN_JumpPhase::Falling;
	TestFalse(TEXT("no jump of its own (walked off a ledge): no button, no glide"), FGSTN_JumpLogic::Step(Ledge, 40, Air, true, Params, Rules).bGliding);

	FGSTN_JumpState Rising = Falling;
	Rising.Phase = EGSTN_JumpPhase::Rising;
	FGSTN_JumpMoverSample Up = Air;
	Up.UpSpeed = 200.0;
	TestFalse(TEXT("rising: no glide"), FGSTN_JumpLogic::Step(Rising, 40, Up, true, Params, Rules).bGliding);

	FGSTN_JumpMoverSample Ground;
	TestFalse(TEXT("landing clears it"), FGSTN_JumpLogic::Step(Out, 41, Ground, true, Params, Rules).bGliding);
	TestFalse(TEXT("an air jump clears it"), FGSTN_JumpLogic::ApplyTakeoff(Out, 41, 2, EGSTN_JumpKind::Air).bGliding);

	// The flag rides the wire
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FGSTN_JumpState Sent = Out;
	Sent.NetSerialize(Writer);
	FMemoryReader Reader(Bytes);
	FGSTN_JumpState Received;
	Received.NetSerialize(Reader);
	TestTrue(TEXT("the glide flag survives the wire"), Received == Sent && Received.bGliding);
	return true;
}

/**
 * Owner's hold-gravity jump (2026-11-17, early 2D Mario), EGSTN_JumpGravityModel::HoldWindow, against the behaviour before (CutUntilApex).
 * Cut and fall scales differ on purpose so that every phase is told apart.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_JumpHoldWindowModelTest, "GSTNCombat.Core.Jump.HoldWindowModel", GSTN_CORE_TEST_FLAGS)
bool FGSTN_JumpHoldWindowModelTest::RunTest(const FString& Parameters)
{
	FGSTN_JumpParams Params;
	Params.RiseGravityScale = 1.f;
	Params.CutGravityScale = 5.f;
	Params.FallGravityScale = 3.f;
	Params.MinHoldFrames = 0;
	Params.MaxHoldFrames = 10;
	Params.CutRampFrames = 0;
	const FGSTN_JumpResetRules Rules;
	TestTrue(TEXT("the owner's model is the default"), Params.GravityModel == EGSTN_JumpGravityModel::HoldWindow);

	FGSTN_JumpMoverSample Rising;
	Rising.bGrounded = false;
	Rising.UpSpeed = 500.0;
	FGSTN_JumpMoverSample Descending = Rising;
	Descending.UpSpeed = -10.0;

	// Runs a jump from frame 0 with the button held on frames 1..HeldUntil, rising on every frame; returns the scale of each frame 1..Frames
	auto Scales = [&](int32 HeldUntil, int32 Frames, FGSTN_JumpState* OutLast = nullptr)
	{
		TArray<float> Out;
		FGSTN_JumpState State = FGSTN_JumpLogic::ApplyTakeoff(FGSTN_JumpState(), 0, 0, EGSTN_JumpKind::Ground);
		for (int32 Frame = 1; Frame <= Frames; ++Frame)
		{
			State = FGSTN_JumpLogic::Step(State, Frame, Rising, Frame <= HeldUntil, Params, Rules);
			Out.Add(FGSTN_JumpLogic::GravityScale(State, Params));
		}
		if (OutLast)
		{
			*OutLast = State;
		}
		return Out;
	};

	// Held through the window: the reduced gravity inside it, the descent gravity as soon as it ends, still rising and still held
	FGSTN_JumpState Last;
	TArray<float> Held = Scales(100, 14, &Last);
	TestEqual(TEXT("held, inside the window: hold gravity"), Held[8], 1.f);
	TestEqual(TEXT("held, the window ends (frame 10): descent gravity while still rising"), Held[9], 3.f);
	TestEqual(TEXT("held, after the window: descent gravity"), Held[13], 3.f);
	TestTrue(TEXT("the window flag is set, the phase still rising"), Last.bHoldWindowOver && Last.Phase == EGSTN_JumpPhase::Rising);

	// Released inside the window: the boosted gravity at once, then the descent gravity when the window ends
	TArray<float> Released = Scales(4, 14);
	TestEqual(TEXT("held on frame 4: hold gravity"), Released[3], 1.f);
	TestEqual(TEXT("released on frame 5: boosted gravity"), Released[4], 5.f);
	TestEqual(TEXT("released, frame 9: still boosted"), Released[8], 5.f);
	TestEqual(TEXT("released, the window ends: descent gravity"), Released[9], 3.f);

	// Released, then the apex before the window ends: the descent gravity from the apex
	FGSTN_JumpState State = FGSTN_JumpLogic::ApplyTakeoff(FGSTN_JumpState(), 0, 0, EGSTN_JumpKind::Ground);
	State = FGSTN_JumpLogic::Step(State, 1, Rising, false, Params, Rules);
	TestEqual(TEXT("a tap: boosted gravity from the first frame (no minimum hold)"), FGSTN_JumpLogic::GravityScale(State, Params), 5.f);
	State = FGSTN_JumpLogic::Step(State, 2, Descending, false, Params, Rules);
	TestEqual(TEXT("descending before the window ends: descent gravity"), FGSTN_JumpLogic::GravityScale(State, Params), 3.f);

	// The ramp: a jump held to the end of the window ramps from the hold to the descent gravity; one released long before switches at once
	Params.CutRampFrames = 4;
	Held = Scales(100, 14);
	TestEqual(TEXT("ramp: the window end is step 1 of 4 toward the descent gravity"), Held[9], 1.5f, 1e-4f);
	TestEqual(TEXT("ramp: step 4 reaches the descent gravity"), Held[12], 3.f, 1e-4f);
	Released = Scales(2, 14);
	TestEqual(TEXT("ramp: released on frame 3, step 1 toward the boosted gravity"), Released[2], 2.f, 1e-4f);
	TestEqual(TEXT("ramp: a cut 7 frames old switches at once to the descent gravity at the window end"), Released[9], 3.f, 1e-4f);
	Params.CutRampFrames = 0;

	// The behaviour before, kept as an option: the window end counts as a release, the boosted gravity lasts until the apex
	Params.GravityModel = EGSTN_JumpGravityModel::CutUntilApex;
	Held = Scales(100, 14, &Last);
	TestEqual(TEXT("CutUntilApex, held: hold gravity inside the window"), Held[8], 1.f);
	TestEqual(TEXT("CutUntilApex, held past the window: boosted gravity until the apex"), Held[13], 5.f);
	TestFalse(TEXT("CutUntilApex never sets the window flag"), Last.bHoldWindowOver);
	Released = Scales(4, 14);
	TestEqual(TEXT("CutUntilApex, released: boosted gravity past the window"), Released[13], 5.f);
	Params.GravityModel = EGSTN_JumpGravityModel::HoldWindow;

	// The same jump lands higher with the descent gravity after the window than with the boosted one (fall 3 < cut 5)
	FGSTN_JumpParams Before = Params;
	Before.GravityModel = EGSTN_JumpGravityModel::CutUntilApex;
	const double WindowApex = FGSTN_JumpLogic::SimulateApex(FVector(0.0, 0.0, -1.0), 980.0, 900.0, 100, Params).Height;
	const double BeforeApex = FGSTN_JumpLogic::SimulateApex(FVector(0.0, 0.0, -1.0), 980.0, 900.0, 100, Before).Height;
	TestTrue(*FString::Printf(TEXT("held through: HoldWindow apex %.1f > CutUntilApex apex %.1f"), WindowApex, BeforeApex), WindowApex > BeforeApex);
	TestTrue(TEXT("a tap is the same in both (the window never ends before the apex)"), FMath::IsNearlyEqual(
		FGSTN_JumpLogic::SimulateApex(FVector(0.0, 0.0, -1.0), 980.0, 300.0, 0, Params).Height, FGSTN_JumpLogic::SimulateApex(FVector(0.0, 0.0, -1.0), 980.0, 300.0, 0, Before).Height));

	// A new take-off and an external launch clear the flag; the flag travels
	FGSTN_JumpState Over;
	Scales(100, 12, &Over);
	TestFalse(TEXT("an air jump starts a new window"), FGSTN_JumpLogic::ApplyTakeoff(Over, 20, 0, EGSTN_JumpKind::Air).bHoldWindowOver);
	FGSTN_JumpMoverSample Launch = Rising;
	Launch.bExternalLaunch = true;
	TestFalse(TEXT("an external launch ends the jump's window"), FGSTN_JumpLogic::Step(Over, 13, Launch, true, Params, Rules).bHoldWindowOver);
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	Over.NetSerialize(Writer);
	FGSTN_JumpState Received;
	FMemoryReader Reader(Bytes);
	Received.NetSerialize(Reader);
	TestTrue(TEXT("the window flag survives the wire"), Received == Over && Received.bHoldWindowOver);
	TestFalse(TEXT("the flag alone makes the state non-default"), [] { FGSTN_JumpState S; S.bHoldWindowOver = true; return S.IsDefault(); }());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_JumpRunBoostTest, "GSTNCombat.Core.Jump.RunBoost", GSTN_CORE_TEST_FLAGS)
bool FGSTN_JumpRunBoostTest::RunTest(const FString& Parameters)
{
	FGSTN_JumpRunBoost Boost;
	TestFalse(TEXT("off by default"), Boost.bEnabled);
	TestEqual(TEXT("off: no boost at any speed"), Boost.VelocityScale(2000.0), 1.f);

	Boost.bEnabled = true;
	TestEqual(TEXT("standing: plain jump"), Boost.VelocityScale(0.0), 1.f);
	TestEqual(TEXT("at MinSpeed: plain jump"), Boost.VelocityScale(470.0), 1.f);
	TestEqual(TEXT("halfway: half the boost"), Boost.VelocityScale(660.0), 1.06f, 1e-4f);
	TestEqual(TEXT("at FullSpeed: full boost"), Boost.VelocityScale(850.0), 1.12f, 1e-4f);
	TestEqual(TEXT("past FullSpeed: capped"), Boost.VelocityScale(1500.0), 1.12f, 1e-4f);

	// About +25 % height at the full boost, as SMB1's fastest jump (height grows with the square of the take-off speed)
	const FGSTN_JumpParams Params;
	const double Plain = FGSTN_JumpLogic::SimulateApex(FVector(0.0, 0.0, -1.0), 980.0, 1000.0, 100, Params).Height;
	const double Running = FGSTN_JumpLogic::SimulateApex(FVector(0.0, 0.0, -1.0), 980.0, 1000.0 * Boost.VelocityScale(900.0), 100, Params).Height;
	TestTrue(*FString::Printf(TEXT("running jump is higher (%.1f vs %.1f)"), Running, Plain), Running > Plain * 1.1);

	// A step instead of a ramp when the two speeds meet
	Boost.FullSpeed = Boost.MinSpeed;
	TestEqual(TEXT("step: below"), Boost.VelocityScale(400.0), 1.f);
	TestEqual(TEXT("step: at or above"), Boost.VelocityScale(470.0), 1.12f, 1e-4f);
	return true;
}


// Owner 2026-11-24, 1A: a hit's launch flies on one gravity, up and down, whatever the victim was doing; a victim hit while falling used to keep
// the kit's fall gravity on the way up and barely rose
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_JumpKnockedFlightTest, "GSTNCombat.Core.Jump.KnockedFlight", GSTN_CORE_TEST_FLAGS)
bool FGSTN_JumpKnockedFlightTest::RunTest(const FString& Parameters)
{
	FGSTN_JumpParams Params;
	Params.RiseGravityScale = 2.6f;
	Params.CutGravityScale = 9.f;
	Params.FallGravityScale = 9.f;
	Params.FastFallGravityScale = 12.f;
	Params.KnockbackGravityScale = 4.f;
	const FGSTN_JumpResetRules Rules;

	FGSTN_JumpMoverSample Hit;
	Hit.bGrounded = false;
	Hit.UpSpeed = 900.0;
	Hit.bKnockbackLaunch = true;

	// Hit while falling: the falling victim is launched up on the knocked gravity, not the fall gravity
	FGSTN_JumpState Falling;
	Falling.Phase = EGSTN_JumpPhase::Falling;
	Falling.AirJumpsUsed = 1;
	const FGSTN_JumpState Knocked = FGSTN_JumpLogic::Step(Falling, 100, Hit, false, Params, Rules);
	TestTrue(TEXT("knocked"), Knocked.bKnocked);
	TestTrue(TEXT("rising -> Launched"), Knocked.Phase == EGSTN_JumpPhase::Launched);
	TestEqual(TEXT("knocked gravity on the way up"), FGSTN_JumpLogic::GravityScale(Knocked, Params), 4.f);
	TestEqual(TEXT("air jumps kept"), static_cast<int32>(Knocked.AirJumpsUsed), 1);

	// Past the apex: still the knocked gravity (it was the fall gravity before)
	FGSTN_JumpMoverSample Down;
	Down.bGrounded = false;
	Down.UpSpeed = -200.0;
	const FGSTN_JumpState Descending = FGSTN_JumpLogic::Step(Knocked, 140, Down, false, Params, Rules);
	TestTrue(TEXT("descending -> Falling"), Descending.Phase == EGSTN_JumpPhase::Falling);
	TestEqual(TEXT("knocked gravity on the way down"), FGSTN_JumpLogic::GravityScale(Descending, Params), 4.f);

	// A fast fall wins; a jump of its own ends the knocked flight; the landing clears it
	FGSTN_JumpState Fast = FGSTN_JumpLogic::ApplyFastFall(Descending);
	TestEqual(TEXT("fast fall wins"), FGSTN_JumpLogic::GravityScale(Fast, Params), 12.f);
	const FGSTN_JumpState Jumped = FGSTN_JumpLogic::ApplyTakeoff(Descending, 150, 5, EGSTN_JumpKind::Air);
	TestFalse(TEXT("own jump ends it"), Jumped.bKnocked);
	FGSTN_JumpMoverSample Ground;
	const FGSTN_JumpState Landed = FGSTN_JumpLogic::Step(Descending, 160, Ground, false, Params, Rules);
	TestFalse(TEXT("landing ends it"), Landed.bKnocked);

	// Hit in the hold phase of its own jump: the jump is over (no hold gravity, no glide button)
	const FGSTN_JumpState Own = FGSTN_JumpLogic::ApplyTakeoff(FGSTN_JumpState(), 10, 5, EGSTN_JumpKind::Ground);
	const FGSTN_JumpState OwnHit = FGSTN_JumpLogic::Step(Own, 12, Hit, true, Params, Rules);
	TestTrue(TEXT("own jump hit: knocked"), OwnHit.bKnocked);
	TestEqual(TEXT("own jump hit: button released"), static_cast<int32>(OwnHit.Button), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("own jump hit: knocked gravity"), FGSTN_JumpLogic::GravityScale(OwnHit, Params), 4.f);

	// Off (0): the old phase gravities
	FGSTN_JumpParams Old = Params;
	Old.KnockbackGravityScale = 0.f;
	TestEqual(TEXT("off: launch rise x1"), FGSTN_JumpLogic::GravityScale(Knocked, Old), 1.f);
	TestEqual(TEXT("off: fall gravity"), FGSTN_JumpLogic::GravityScale(Descending, Old), 9.f);

	// The flag survives the wire and counts against the default
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	FGSTN_JumpState Sent = Descending;
	Sent.NetSerialize(Writer);
	FMemoryReader Reader(Bytes);
	FGSTN_JumpState Received;
	Received.NetSerialize(Reader);
	TestTrue(TEXT("round trip"), Received == Sent);
	FGSTN_JumpState OnlyKnocked;
	OnlyKnocked.bKnocked = true;
	TestFalse(TEXT("not default"), OnlyKnocked.IsDefault());
	return true;
}

#endif
