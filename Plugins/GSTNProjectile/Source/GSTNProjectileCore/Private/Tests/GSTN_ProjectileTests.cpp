// Automation tests of the projectile plugin, pure part: the ballistic mode and the per-frame sweep the host runs against hurtboxes.
// Goldens are hand values; each test says how it was derived.

#include "Movement/GSTN_BallisticMode.h"
#include "Movement/GSTN_ShotAim.h"
#include "Misc/AutomationTest.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGSTN_HitShape ProjectileTestSphere(double Radius)
	{
		FGSTN_HitShape Shape;
		Shape.Type = EGSTN_ShapeType::Sphere;
		Shape.Extent = FVector(Radius);
		return Shape;
	}

	/** 1000 cm/s along +X from the origin at 62.5 Hz: 16 cm per frame, starting at StartFrame */
	FGSTN_BallisticSegment ProjectileTestStraightSegment(int32 StartFrame)
	{
		FGSTN_ProjectileParams Params;
		Params.Origin = FVector::ZeroVector;
		Params.Direction = FVector(1.0, 0.0, 0.0);
		Params.Speed = 1000.f;
		Params.GravityAccel = FVector::ZeroVector;
		return FGSTN_BallisticMode::MakeSegment(Params, StartFrame, 0.016);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNProjectileStraightHitTest, "GSTNProjectile.Core.StraightPathHitsHurtboxAtExpectedFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTNProjectileStraightHitTest::RunTest(const FString& Parameters)
{
	// Projectile radius 10, target hurtbox radius 20 centred at x = 500: contact once the centre passes x = 470.
	// Frame k after the start is at 16 k: frame 29 sweeps 448 -> 464 (gap 6 cm), frame 30 sweeps 464 -> 480 (touches). Expected: start + 30
	const FGSTN_HitShape Projectile = ProjectileTestSphere(10.0);
	const FGSTN_HitShape Hurtbox = ProjectileTestSphere(20.0);
	const FTransform Target(FVector(500.0, 0.0, 0.0));

	for (const int32 StartFrame : {0, 1000})
	{
		const FGSTN_BallisticSegment Segment = ProjectileTestStraightSegment(StartFrame);
		TestEqual(TEXT("position at the start frame is the origin"), FGSTN_BallisticMode::PositionAt(Segment, StartFrame), FVector::ZeroVector);
		TestEqual(TEXT("zero gravity: frame 30 at x = 480 on the line"), FGSTN_BallisticMode::PositionAt(Segment, StartFrame + 30), FVector(480.0, 0.0, 0.0));
		TestFalse(TEXT("frame 29 does not touch"), FGSTN_BallisticMode::SweepStep(Segment, Projectile, StartFrame + 29, Hurtbox, Target));
		TestEqual(TEXT("first contact at start + 30"), FGSTN_BallisticMode::FindFirstContactFrame(Segment, Projectile, Hurtbox, Target, StartFrame + 120), StartFrame + 30);
	}

	// The same target 100 cm off the line is never touched (gap 70 cm)
	const FGSTN_BallisticSegment Segment = ProjectileTestStraightSegment(0);
	TestEqual(TEXT("a target off the line is missed"),
		FGSTN_BallisticMode::FindFirstContactFrame(Segment, Projectile, Hurtbox, FTransform(FVector(500.0, 100.0, 0.0)), 120), static_cast<int32>(INDEX_NONE));

	// A lifetime that ends before the contact frame never hits
	TestEqual(TEXT("lifetime ends before contact"), FGSTN_BallisticMode::FindFirstContactFrame(Segment, Projectile, Hurtbox, Target, 29), static_cast<int32>(INDEX_NONE));

	// A thin hurtbox (radius 2) at x = 504 is crossed inside one frame (16 cm step, 24 cm contact band): the sweep still finds frame 31
	// (frame 30 ends at 480, 12 cm short of the 492 band; frame 31 sweeps 480 -> 496)
	TestEqual(TEXT("swept: thin hurtbox not jumped over"),
		FGSTN_BallisticMode::FindFirstContactFrame(Segment, Projectile, ProjectileTestSphere(2.0), FTransform(FVector(504.0, 0.0, 0.0)), 120), 31);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNProjectileGravityTest, "GSTNProjectile.Core.GravityClosedForm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTNProjectileGravityTest::RunTest(const FString& Parameters)
{
	// 1000 cm/s along +X, gravity -1000 cm/s^2 on Z, 0.016 s steps, start frame 10. Frame 60: t = 0.8 s, x = 800, z = -0.5 * 1000 * 0.64 = -320; vz = -800
	FGSTN_ProjectileParams Params;
	Params.Direction = FVector(2.0, 0.0, 0.0); // normalized on use
	Params.Speed = 1000.f;
	Params.GravityAccel = FVector(0.0, 0.0, -1000.0);
	const FGSTN_BallisticSegment Segment = FGSTN_BallisticMode::MakeSegment(Params, 10, 0.016);
	TestTrue(TEXT("position at frame 60"), FGSTN_BallisticMode::PositionAt(Segment, 60).Equals(FVector(800.0, 0.0, -320.0), 1e-9));
	TestTrue(TEXT("velocity at frame 60"), FGSTN_BallisticMode::VelocityAt(Segment, 60).Equals(FVector(1000.0, 0.0, -800.0), 1e-9));
	TestEqual(TEXT("frames before the start stay at the start"), FGSTN_BallisticMode::PositionAt(Segment, 3), FVector::ZeroVector);

	// Same inputs, same bits: the host, a client and a rewind call the same closed form
	const FVector A = FGSTN_BallisticMode::PositionAt(Segment, 77);
	const FVector B = FGSTN_BallisticMode::PositionAt(FGSTN_BallisticMode::MakeSegment(Params, 10, 0.016), 77);
	TestTrue(TEXT("bit-identical recomputation"), FMemory::Memcmp(&A, &B, sizeof(FVector)) == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNProjectileArcAimTest, "GSTNProjectile.Core.ArcAimReachesTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTNProjectileArcAimTest::RunTest(const FString& Parameters)
{
	// v = 1000 cm/s, g = 1000 cm/s^2, target 800 cm away at the same height. tan(theta) = (v^2 -+ sqrt(v^4 - g^2 x^2)) / (g x)
	// = (1e6 -+ 6e5) / 8e5: 0.5 for the flat shot, 2 for the lob. Flight time x / (v cos theta): 0.894 s (55.9 frames), 1.789 s (111.8 frames)
	const FVector Gravity(0.0, 0.0, -1000.0);
	const FVector TargetPoint(800.0, 0.0, 0.0);
	const FGSTN_HitShape Projectile = ProjectileTestSphere(10.0);
	const FGSTN_HitShape Hurtbox = ProjectileTestSphere(20.0);

	struct FCase { bool bHigh; double Tan; int32 MinFrame; int32 MaxFrame; };
	for (const FCase Case : {FCase{false, 0.5, 50, 56}, FCase{true, 2.0, 104, 112}})
	{
		FVector Direction;
		TestTrue(TEXT("reachable target solved"), FGSTN_BallisticMode::SolveLaunchDirection(FVector::ZeroVector, TargetPoint, 1000.0, Gravity, Case.bHigh, Direction));
		TestTrue(TEXT("unit direction"), FMath::IsNearlyEqual(Direction.Size(), 1.0, 1e-9));
		TestTrue(TEXT("launch tangent matches the closed form"), FMath::IsNearlyEqual(Direction.Z / Direction.X, Case.Tan, 1e-9));

		FGSTN_ProjectileParams Params;
		Params.Direction = Direction;
		Params.Speed = 1000.f;
		Params.GravityAccel = Gravity;
		const FGSTN_BallisticSegment Segment = FGSTN_BallisticMode::MakeSegment(Params, 0, 0.016);
		const int32 Contact = FGSTN_BallisticMode::FindFirstContactFrame(Segment, Projectile, Hurtbox, FTransform(TargetPoint), 200);
		TestTrue(FString::Printf(TEXT("%s arc hits the target hurtbox near its flight time (frame %d)"), Case.bHigh ? TEXT("high") : TEXT("low"), Contact),
			Contact >= Case.MinFrame && Contact <= Case.MaxFrame);
		double Closest = TNumericLimits<double>::Max();
		for (int32 Frame = 1; Frame <= 200; ++Frame)
		{
			Closest = FMath::Min(Closest, FVector::Dist(FGSTN_BallisticMode::PositionAt(Segment, Frame), TargetPoint));
		}
		TestTrue(FString::Printf(TEXT("the path passes through the target (closest frame %.2f cm, 16 cm steps)"), Closest), Closest < 10.0);
	}

	// Gravity along -Y (a wall or a tilted arena): the same solution rotated
	FVector Sideways;
	TestTrue(TEXT("sideways gravity solved"), FGSTN_BallisticMode::SolveLaunchDirection(FVector::ZeroVector, FVector(800.0, 0.0, 0.0), 1000.0, FVector(0.0, -1000.0, 0.0), false, Sideways));
	TestTrue(TEXT("sideways: tangent 0.5 against gravity"), FMath::IsNearlyEqual(Sideways.Y / Sideways.X, 0.5, 1e-9));

	FVector Unused = FVector::ZeroVector;
	TestFalse(TEXT("out of range: v^2 / g = 1000 cm is the farthest flat reach"), FGSTN_BallisticMode::SolveLaunchDirection(FVector::ZeroVector, FVector(1001.0, 0.0, 0.0), 1000.0, Gravity, false, Unused));
	TestTrue(TEXT("out of range leaves the direction untouched"), Unused.IsZero());
	FVector Straight;
	TestTrue(TEXT("zero gravity aims straight"), FGSTN_BallisticMode::SolveLaunchDirection(FVector::ZeroVector, FVector(3.0, 4.0, 0.0), 1000.0, FVector::ZeroVector, true, Straight)
		&& Straight.Equals(FVector(0.6, 0.8, 0.0), 1e-12));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNProjectileHitscanTest, "GSTNProjectile.Core.HitscanAndReticleAim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTNProjectileHitscanTest::RunTest(const FString& Parameters)
{
	// M11 L9. A hitscan along +Y (yaw 90) from (0, 0, 100), range 3000, radius 5: the capsule must cover the line in its first frame
	FGSTN_ProjectileParams Params;
	Params.Origin = FVector(0.0, 0.0, 100.0);
	Params.Direction = FVector(0.0, 1.0, 0.0);
	Params.MaxHits = 2;
	FGSTN_HitscanSpec Spec;
	Spec.RangeCm = 3000.f;
	Spec.RadiusCm = 5.f;
	FGSTN_ShotAim::MakeHitscan(Params, Spec);
	TestEqual(TEXT("one frame of life"), Params.LifetimeFrames, 1);
	TestTrue(TEXT("no gravity"), Params.GravityAccel.IsZero());
	TestTrue(TEXT("nearest first"), Params.bHitNearestFirst);
	TestEqual(TEXT("MaxHits kept"), static_cast<int32>(Params.MaxHits), 2);

	const FGSTN_BallisticSegment Segment = FGSTN_BallisticMode::Quantize(FGSTN_BallisticMode::MakeSegment(Params, 0, 0.016));
	const FGSTN_HitShape Target = ProjectileTestSphere(20.0);
	TestTrue(TEXT("near target on the line"), FGSTN_BallisticMode::SweepStep(Segment, Params.Shape, 1, Target, FTransform(FVector(0.0, 300.0, 100.0))));
	TestTrue(TEXT("far target on the line"), FGSTN_BallisticMode::SweepStep(Segment, Params.Shape, 1, Target, FTransform(FVector(0.0, 2950.0, 100.0))));
	TestFalse(TEXT("past the range"), FGSTN_BallisticMode::SweepStep(Segment, Params.Shape, 1, Target, FTransform(FVector(0.0, 3200.0, 100.0))));
	TestFalse(TEXT("behind the muzzle"), FGSTN_BallisticMode::SweepStep(Segment, Params.Shape, 1, Target, FTransform(FVector(0.0, -200.0, 100.0))));
	TestFalse(TEXT("off the line (30 cm aside, 20 + 5 reach)"), FGSTN_BallisticMode::SweepStep(Segment, Params.Shape, 1, Target, FTransform(FVector(30.0, 1500.0, 100.0))));
	TestTrue(TEXT("grazing (24 cm aside)"), FGSTN_BallisticMode::SweepStep(Segment, Params.Shape, 1, Target, FTransform(FVector(24.0, 1500.0, 100.0))));

	// Reticle aim: on = the camera aim with its pitch clamped; off = the flat direction plus the launch pitch, never the camera's pitch
	const FVector Flat(1.0, 0.0, 0.0);
	const FVector Up30 = FGSTN_ShotAim::ReticleDirection(true, FRotator(30.0, 90.0, 0.0), Flat, -60.f, 60.f, 0.f);
	TestEqual(TEXT("on: reticle pitch"), Up30.Rotation().Pitch, 30.0, 0.01);
	TestEqual(TEXT("on: reticle yaw"), Up30.Rotation().Yaw, 90.0, 0.01);
	TestEqual(TEXT("on: pitch clamped"), FGSTN_ShotAim::ReticleDirection(true, FRotator(80.0, 0.0, 0.0), Flat, -60.f, 60.f, 0.f).Rotation().Pitch, 60.0, 0.01);
	TestEqual(TEXT("on: no aim in the command = flat"), FGSTN_ShotAim::ReticleDirection(true, FRotator::ZeroRotator, Flat, -60.f, 60.f, 10.f).Rotation().Pitch, 0.0, 0.01);
	TestEqual(TEXT("off: launch pitch only"), FGSTN_ShotAim::ReticleDirection(false, FRotator(30.0, 90.0, 0.0), Flat, -60.f, 60.f, 10.f).Rotation().Pitch, 10.0, 0.01);
	TestEqual(TEXT("off: flat yaw"), FGSTN_ShotAim::ReticleDirection(false, FRotator(30.0, 90.0, 0.0), Flat, -60.f, 60.f, 0.f).Rotation().Yaw, 0.0, 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNProjectileGhostReconcileTest, "GSTNProjectile.Core.GhostReconcile",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTNProjectileGhostReconcileTest::RunTest(const FString& Parameters)
{
	// A shot of 1500 cm/s along a skewed direction with gravity, from an off-grid origin: every component needs rounding
	FGSTN_ProjectileParams Params;
	Params.Origin = FVector(1234.5678, -987.6543, 210.0101);
	Params.Direction = FVector(0.8, 0.5, 0.33);
	Params.Speed = 1500.f;
	Params.GravityAccel = FVector(0.0, 0.0, -980.0);
	const FGSTN_BallisticSegment Raw = FGSTN_BallisticMode::MakeSegment(Params, 1000, 0.016);
	const FGSTN_BallisticSegment Host = FGSTN_BallisticMode::Quantize(Raw);

	// Wire: what a client decodes is the host's quantized segment, bit for bit, in a few dozen bytes; quantizing again changes nothing
	FBitWriter Writer(0, true);
	FGSTN_BallisticSegment Sent = Host;
	bool bOk = false;
	Sent.NetSerialize(Writer, nullptr, bOk);
	TestTrue(TEXT("serialize ok"), bOk);
	TestTrue(TEXT("spawn payload under 40 bytes"), Writer.GetNumBytes() < 40);
	AddInfo(FString::Printf(TEXT("segment payload %lld bytes (raw doubles 84)"), Writer.GetNumBytes()));
	FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
	FGSTN_BallisticSegment Received;
	Received.NetSerialize(Reader, nullptr, bOk);
	TestEqual(TEXT("frame"), Received.StartFrame, Host.StartFrame);
	TestTrue(TEXT("same bits on the client"), Received.Start == Host.Start && Received.Velocity == Host.Velocity
		&& Received.GravityAccel == Host.GravityAccel && Received.StepSeconds == Host.StepSeconds);
	const FGSTN_BallisticSegment Again = FGSTN_BallisticMode::Quantize(Host);
	TestTrue(TEXT("quantize is idempotent"), Again.Start == Host.Start && Again.Velocity == Host.Velocity && Again.GravityAccel == Host.GravityAccel);
	TestTrue(TEXT("position at frame 1060 identical host/client"), FGSTN_BallisticMode::PositionAt(Received, 1060) == FGSTN_BallisticMode::PositionAt(Host, 1060));
	TestTrue(TEXT("rounding error under 0.01 cm"), FVector::Dist(Host.Start, Raw.Start) < 0.01);

	// The ghost is the client's prediction of the same shot; reconcile at frame 1015, where the host's projectile arrives
	double Error = 0.0;
	TestTrue(TEXT("exact prediction adopts with zero error"),
		FGSTN_ProjectileGhostReconcile::Decide(Host, Received, 1015, 3, 50.0, Error) == FGSTN_ProjectileGhostReconcile::EResult::Adopt && Error == 0.0);

	FGSTN_ProjectileParams Off = Params;
	Off.Origin.X += 3.0;
	const FGSTN_BallisticSegment Near = FGSTN_BallisticMode::Quantize(FGSTN_BallisticMode::MakeSegment(Off, 1000, 0.016));
	TestTrue(TEXT("3 cm off adopts"), FGSTN_ProjectileGhostReconcile::Decide(Near, Host, 1015, 3, 50.0, Error) == FGSTN_ProjectileGhostReconcile::EResult::Adopt);
	TestTrue(TEXT("3 cm off: error 3 cm"), FMath::IsNearlyEqual(Error, 3.0, 0.02));

	// One frame late at 1500 cm/s: 24 cm along the path (plus the gravity term), still the same shot within tolerance
	const FGSTN_BallisticSegment Late = FGSTN_BallisticMode::Quantize(FGSTN_BallisticMode::MakeSegment(Params, 1001, 0.016));
	TestTrue(TEXT("one frame late adopts"), FGSTN_ProjectileGhostReconcile::Decide(Host, Late, 1015, 3, 50.0, Error) == FGSTN_ProjectileGhostReconcile::EResult::Adopt);
	TestTrue(TEXT("one frame late: ~24 cm"), Error > 20.0 && Error < 30.0);

	Off.Origin.X += 117.0;
	const FGSTN_BallisticSegment Far = FGSTN_BallisticMode::Quantize(FGSTN_BallisticMode::MakeSegment(Off, 1000, 0.016));
	TestTrue(TEXT("120 cm off snaps"), FGSTN_ProjectileGhostReconcile::Decide(Far, Host, 1015, 3, 50.0, Error) == FGSTN_ProjectileGhostReconcile::EResult::Snap);
	const FGSTN_BallisticSegment Other = FGSTN_BallisticMode::Quantize(FGSTN_BallisticMode::MakeSegment(Params, 1010, 0.016));
	TestTrue(TEXT("10 frames apart is another shot"), FGSTN_ProjectileGhostReconcile::Decide(Host, Other, 1015, 3, 50.0, Error) == FGSTN_ProjectileGhostReconcile::EResult::NoMatch);

	// Blend: the full offset at adoption, half at 3 of 6 frames, none from frame 6 on
	const FVector Offset(10.0, -4.0, 2.0);
	TestTrue(TEXT("blend start"), FGSTN_ProjectileGhostReconcile::BlendOffset(Offset, 1015, 6, 1015) == Offset);
	TestTrue(TEXT("blend half"), FGSTN_ProjectileGhostReconcile::BlendOffset(Offset, 1015, 6, 1018).Equals(Offset * 0.5, 1e-9));
	TestTrue(TEXT("blend done"), FGSTN_ProjectileGhostReconcile::BlendOffset(Offset, 1015, 6, 1021).IsZero());
	TestTrue(TEXT("no blend frames"), FGSTN_ProjectileGhostReconcile::BlendOffset(Offset, 1015, 0, 1015).IsZero());
	return true;
}

#endif
