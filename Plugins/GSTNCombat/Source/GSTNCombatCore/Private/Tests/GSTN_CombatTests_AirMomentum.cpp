// Headless tests of the air momentum model (owner 2026-11-18, docs/notes/handovers/air-momentum-1.md): the neutral stick styles, the external
// channel that the stick does not cancel, the brake against it, no speed past the cap along it, the collision clamp, the run state's wire form.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Movement/GSTN_AirMomentumTypes.h"
#include "Movement/GSTN_RunStageTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr float AirMomentumDt = 0.016f;

	/** Runs Frames frames with a constant stick, feeding the output back */
	FGSTN_AirMomentumOutput Run(const FGSTN_AirMomentumTuning& Tuning, FGSTN_AirMomentumInput Input, int32 Frames)
	{
		FGSTN_AirMomentumOutput Out;
		Out.Velocity = Input.Velocity;
		Out.External = Input.External;
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Input.Velocity = Out.Velocity;
			Input.External = Out.External;
			Out = FGSTN_AirMomentumLogic::Step(Tuning, Input);
		}
		return Out;
	}

	FGSTN_AirMomentumInput Drift(float Speed, const FVector& Stick)
	{
		FGSTN_AirMomentumInput Input;
		Input.Velocity = FVector(Speed, 0, 0);
		Input.Stick = Stick;
		Input.TopSpeed = 450.f;
		Input.Control = 0.85f;
		Input.DeltaSeconds = AirMomentumDt;
		return Input;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AirMomentumNeutralTest, "GSTNCombat.Core.AirMomentum.NeutralStyles", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AirMomentumNeutralTest::RunTest(const FString& Parameters)
{
	FGSTN_AirMomentumTuning Astro;
	FGSTN_AirMomentumTuning Mario;
	Mario.Style = EGSTN_AirInertiaStyle::Carry;
	TestTrue(TEXT("the default style is the high friction one (owner: less inertia)"), Astro.Style == EGSTN_AirInertiaStyle::HighFriction);
	FGSTN_AirMomentumTuning Legacy;
	Legacy.Style = EGSTN_AirInertiaStyle::MoverLegacy;
	TestTrue(TEXT("MoverLegacy turns the model off"), !Legacy.IsEnabled());

	// A 450 cm/s drift with the stick released: Astro stops within 8 frames, Mario keeps most of it after 30
	const FGSTN_AirMomentumOutput AstroOut = Run(Astro, Drift(450.f, FVector::ZeroVector), 8);
	TestTrue(TEXT("high friction: stopped in 8 frames"), AstroOut.Velocity.IsNearlyZero());
	const FGSTN_AirMomentumOutput MarioOut = Run(Mario, Drift(450.f, FVector::ZeroVector), 30);
	TestTrue(TEXT("carry: over half the drift after 30 frames"), MarioOut.Velocity.X > 180.f && MarioOut.Velocity.X < 450.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AirMomentumSteerTest, "GSTNCombat.Core.AirMomentum.Steer", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AirMomentumSteerTest::RunTest(const FString& Parameters)
{
	const FGSTN_AirMomentumTuning Tuning;
	// From standing, full stick: at the 450 cap in ~10 frames (3200 x 0.85 = 2720 cm/s2), never past it
	const FGSTN_AirMomentumOutput Up = Run(Tuning, Drift(0.f, FVector(1, 0, 0)), 12);
	TestTrue(TEXT("reaches the top speed"), FMath::IsNearlyEqual(Up.Velocity.X, 450.0, 1.0));
	TestTrue(TEXT("stays at the top speed"), FMath::IsNearlyEqual(Run(Tuning, Drift(0.f, FVector(1, 0, 0)), 60).Velocity.X, 450.0, 1.0));
	// Reversal +450 -> -450 with the stick back: under 20 frames
	const FGSTN_AirMomentumOutput Back = Run(Tuning, Drift(450.f, FVector(-1, 0, 0)), 20);
	TestTrue(TEXT("full reversal within 20 frames"), FMath::IsNearlyEqual(Back.Velocity.X, -450.0, 1.0));
	// Half tilt: half the cap
	TestTrue(TEXT("half tilt = half the cap"), FMath::IsNearlyEqual(Run(Tuning, Drift(0.f, FVector(0.5, 0, 0)), 30).Velocity.X, 225.0, 1.0));
	// Zero air control: a held stick steers nothing
	FGSTN_AirMomentumInput Locked = Drift(300.f, FVector(0, 1, 0));
	Locked.Control = 0.f;
	TestTrue(TEXT("zero control, stick held: no steering"), Run(Tuning, Locked, 5).Velocity.Y == 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AirMomentumExternalTest, "GSTNCombat.Core.AirMomentum.External", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AirMomentumExternalTest::RunTest(const FString& Parameters)
{
	const FGSTN_AirMomentumTuning Tuning;
	// A 1200 cm/s knockback (all external): neutral stick keeps it (only the slow decay: 300 cm/s2 -> ~58 cm/s lost in 12 frames)
	FGSTN_AirMomentumInput Launch = Drift(1200.f, FVector::ZeroVector);
	Launch.External = FVector(1200, 0, 0);
	const FGSTN_AirMomentumOutput Neutral = Run(Tuning, Launch, 12);
	TestTrue(TEXT("neutral stick does not kill the push"), Neutral.Velocity.X > 1130.f);
	TestTrue(TEXT("the push stays external"), FMath::IsNearlyEqual(Neutral.External.X, Neutral.Velocity.X, 0.01));

	// Stick along the push: no speed past it (the cap is filled by the push already)
	FGSTN_AirMomentumInput Along = Launch;
	Along.Stick = FVector(1, 0, 0);
	TestTrue(TEXT("stick along adds nothing"), Run(Tuning, Along, 12).Velocity.X <= Neutral.Velocity.X + 0.01);

	// Stick against: brakes it much faster than neutral
	FGSTN_AirMomentumInput Against = Launch;
	Against.Stick = FVector(-1, 0, 0);
	TestTrue(TEXT("stick against brakes the push"), Run(Tuning, Against, 12).Velocity.X < Neutral.Velocity.X - 300.f);

	// Stick sideways: drift on the player channel, the push kept
	FGSTN_AirMomentumInput Side = Launch;
	Side.Stick = FVector(0, 1, 0);
	const FGSTN_AirMomentumOutput SideOut = Run(Tuning, Side, 12);
	TestTrue(TEXT("sideways drift while pushed"), SideOut.Velocity.Y > 300.f);
	TestTrue(TEXT("sideways stick does not brake the push"), FMath::IsNearlyEqual(SideOut.External.X, Neutral.External.X, 0.01));

	// A knockback's own drag acts on the push
	FGSTN_AirMomentumInput Dragged = Launch;
	Dragged.ExternalDragPerFrame = 0.1f;
	TestTrue(TEXT("knockback drag shrinks the push"), Run(Tuning, Dragged, 5).External.X < 1200.f * 0.6f);

	// Tiny pushes are dropped
	FGSTN_AirMomentumInput Tiny = Drift(10.f, FVector::ZeroVector);
	Tiny.External = FVector(10, 0, 0);
	TestTrue(TEXT("under the min speed the push is dropped"), FGSTN_AirMomentumLogic::Step(Tuning, Tiny).External.IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AirMomentumDriftDITest, "GSTNCombat.Core.AirMomentum.DriftDI", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AirMomentumDriftDITest::RunTest(const FString& Parameters)
{
	const FGSTN_AirMomentumTuning Tuning;
	FGSTN_AirMomentumInput Launch = Drift(1200.f, FVector::ZeroVector);
	Launch.External = FVector(1200, 0, 0);
	const FGSTN_AirMomentumOutput Neutral = Run(Tuning, Launch, 12);

	// Hitstun drift: the stick against the knockback does not brake it, sideways it drifts
	FGSTN_AirMomentumInput Against = Launch;
	Against.bDriftDI = true;
	Against.Stick = FVector(-1, 0, 0);
	const FGSTN_AirMomentumOutput AgainstOut = Run(Tuning, Against, 12);
	TestTrue(TEXT("drift DI never brakes the push"), FMath::IsNearlyEqual(AgainstOut.External.X, Neutral.External.X, 0.01));
	TestTrue(TEXT("drift DI against: the player channel pulls back within the cap"), AgainstOut.Velocity.X < Neutral.Velocity.X && AgainstOut.Velocity.X >= Neutral.Velocity.X - 450.0 - 0.01);
	FGSTN_AirMomentumInput Side = Launch;
	Side.bDriftDI = true;
	Side.Stick = FVector(0, 1, 0);
	TestTrue(TEXT("drift DI sideways moves the path"), Run(Tuning, Side, 12).Velocity.Y > 300.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AirMomentumClampTest, "GSTNCombat.Core.AirMomentum.CollisionClamp", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AirMomentumClampTest::RunTest(const FString& Parameters)
{
	const FVector Push(800, 0, 0);
	const FVector Free(800, 200, 0);
	TestTrue(TEXT("free flight keeps the push"), FGSTN_AirMomentumLogic::ClampExternalToMoved(Push, Free, Free, 20.f).Equals(Push, 0.01));
	TestTrue(TEXT("head-on wall: nothing left"), FGSTN_AirMomentumLogic::ClampExternalToMoved(Push, Push, FVector::ZeroVector, 20.f).IsZero());
	const FVector Slid = FGSTN_AirMomentumLogic::ClampExternalToMoved(Push, FVector(800, 300, 0), FVector(0, 300, 0), 20.f);
	TestTrue(TEXT("wall along Y: the push into it is gone"), Slid.IsZero());
	const FVector Glance = FGSTN_AirMomentumLogic::ClampExternalToMoved(Push, FVector(800, 0, 0), FVector(600, 200, 0), 20.f);
	TestTrue(TEXT("glancing wall: some push left, no longer than the move"), !Glance.IsZero() && Glance.Size() <= FVector(600, 200, 0).Size() + 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_AirMomentumWireTest, "GSTNCombat.Core.AirMomentum.RunStateWire", GSTN_CORE_TEST_FLAGS)
bool FGSTN_AirMomentumWireTest::RunTest(const FString& Parameters)
{
	FGSTN_RunStageState State;
	State.ExternalVelocity = FVector(1234.56, -78.91, 0.0);
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	State.SerializeCompact(Writer);
	FGSTN_RunStageState Read;
	FMemoryReader Reader(Bytes);
	Read.SerializeCompact(Reader);
	TestTrue(TEXT("the external channel travels at 0.1 cm/s"), Read.ExternalVelocity.Equals(State.ExternalVelocity, 0.06));
	return true;
}

#endif
