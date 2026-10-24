// Pure tests of the wall control model (wall-slide-v2.md W1, 6.2): GSTN.Wall.Control.*

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WallMovement/GSTN_WallControlTypes.h"
#include "WallMovement/MoveLibrary/GSTN_WallControlUtils.h"

/** Double-safe tolerance compare (FVector2D components are double, the literals float: TestNearlyEqual is ambiguous) */
static void ControlNearlyEq(FAutomationTestBase& Test, const TCHAR* What, double A, double B, double Tolerance = 1e-4)
{
	Test.TestTrue(What, FMath::Abs(A - B) <= Tolerance);
}

namespace GSTNWallControlTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Dt = 1.f / 60.f;

	static TArray<FVector> UpVectors()
	{
		return { FVector(0, 0, 1), FVector(0, 0, -1), FVector(1, 0, 0), FVector(0, -1, 0), FVector(1, 1, 1).GetSafeNormal(), FVector(-2, 1, 3).GetSafeNormal() };
	}

	/** A wall normal perpendicular to Up, rotated by Tilt degrees away from it so the plane is not always vertical */
	static FVector WallNormalFor(const FVector& Up, float TiltDeg)
	{
		const FVector Seed = FMath::Abs(Up.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
		const FVector Side = FVector::CrossProduct(Up, Seed).GetSafeNormal();
		const FVector Axis = FVector::CrossProduct(Side, Up).GetSafeNormal();
		return FQuat(Axis, FMath::DegreesToRadians(TiltDeg)).RotateVector(Side);
	}

	static FVector2D Run(const FGSTN_WallSlideControlSettings& S, FVector2D V, const FVector2D& Input, bool bBrake, float Seconds)
	{
		for (int32 i = 0, N = FMath::RoundToInt(Seconds / Dt); i < N; ++i)
		{
			V = UGSTN_WallControlUtils::StepSlideVelocity(S, V, Input, bBrake, Dt);
		}
		return V;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallControlEntryTest, "GSTN.Wall.Control.EntryKeepsMomentum", GSTNWallControlTest::TestFlags)
bool FGSTN_WallControlEntryTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallControlTest;
	FGSTN_WallSlideControlSettings S;
	for (const FVector& Up : UpVectors())
	{
		const FVector N = WallNormalFor(Up, 0.f);
		FVector H, V;
		UGSTN_WallControlUtils::MakePlaneAxes(N, Up, H, V);
		// A run along the wall, a fall, and a bit into the wall
		const FVector In = H * 400.f - Up * 500.f - N * 150.f;

		S.EntrySpeedKeep = 1.f;
		const FVector Out = UGSTN_WallControlUtils::ProjectEntryVelocity(S, In, N);
		ControlNearlyEq(*this, TEXT("Length kept"), Out.Size(), In.Size(), 0.1f);
		ControlNearlyEq(*this, TEXT("On the plane"), FVector::DotProduct(Out, N), 0.f, 0.01f);
		const FVector2D P = UGSTN_WallControlUtils::ToPlane(Out, H, V);
		TestTrue(TEXT("Keeps the run direction"), P.X > 0.f);
		TestTrue(TEXT("Keeps the fall direction"), P.Y < 0.f);

		S.EntrySpeedKeep = 0.5f;
		ControlNearlyEq(*this, TEXT("Keep factor"), UGSTN_WallControlUtils::ProjectEntryVelocity(S, In, N).Size(), In.Size() * 0.5f, 0.1f);

		S.EntrySpeedKeep = 1.f;
		S.MaxEntrySpeed = 300.f;
		ControlNearlyEq(*this, TEXT("Clamped to the max"), UGSTN_WallControlUtils::ProjectEntryVelocity(S, In, N).Size(), 300.f, 0.1f);
		S.MaxEntrySpeed = 1500.f;

		TestTrue(TEXT("Head-on gives nothing to carry"), UGSTN_WallControlUtils::ProjectEntryVelocity(S, -N * 300.f, N).IsNearlyZero());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallControlHorizontalTest, "GSTN.Wall.Control.HorizontalLowControl", GSTNWallControlTest::TestFlags)
bool FGSTN_WallControlHorizontalTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallControlTest;
	const FGSTN_WallSlideControlSettings S;

	// Momentum decays slowly: after one second without input most of it is still there
	const FVector2D Coast = Run(S, FVector2D(500.f, 0.f), FVector2D::ZeroVector, false, 1.f);
	TestTrue(TEXT("Momentum mostly kept after 1 s"), Coast.X > 500.f * 0.6f);
	TestTrue(TEXT("But it does decay"), Coast.X < 500.f);

	// Input from rest accelerates only a little: after 0.5 s, accel * 0.5
	const FVector2D Push = Run(S, FVector2D::ZeroVector, FVector2D(1.f, 0.f), false, 0.5f);
	ControlNearlyEq(*this, TEXT("Low control accel"), Push.X, S.HorizontalControlAccel * 0.5f, S.HorizontalControlAccel * 0.05f);
	TestTrue(TEXT("Capped by the control max"), Run(S, FVector2D::ZeroVector, FVector2D(1.f, 0.f), false, 10.f).X <= S.HorizontalControlMaxSpeed + 0.01f);

	// Input along entry momentum above the max adds nothing
	const FVector2D Fast = UGSTN_WallControlUtils::StepSlideVelocity(S, FVector2D(800.f, 0.f), FVector2D(1.f, 0.f), false, Dt);
	TestTrue(TEXT("Input does not accelerate past the control max"), Fast.X <= 800.f);

	// Works to the left too
	TestTrue(TEXT("Mirrored"), Run(S, FVector2D::ZeroVector, FVector2D(-1.f, 0.f), false, 0.5f).X < -S.HorizontalControlAccel * 0.4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallControlBrakeAgainstTest, "GSTN.Wall.Control.StickAgainstMotionBrakesHard", GSTNWallControlTest::TestFlags)
bool FGSTN_WallControlBrakeAgainstTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallControlTest;
	const FGSTN_WallSlideControlSettings S;
	const FVector2D Start(500.f, 0.f);

	const FVector2D Coast = Run(S, Start, FVector2D::ZeroVector, false, 0.2f);
	const FVector2D Brake = Run(S, Start, FVector2D(-1.f, 0.f), false, 0.2f);
	TestTrue(TEXT("Braking sheds far more than drag"), Brake.X < Coast.X - 200.f);
	ControlNearlyEq(*this, TEXT("Brake decel rate"), Brake.X, 500.f * FMath::Exp(-S.HorizontalDrag * 0.2f) - S.HorizontalBrakeDecel * 0.2f, 30.f);

	// The brake itself never overshoots zero in the step that crosses it. Holding the stick afterwards does accelerate the
	// other way (the little control along the motion), which is intended, so a long run is not asserted as "stays >= 0"
	const FVector2D Crossing = UGSTN_WallControlUtils::StepSlideVelocity(S, FVector2D(10.f, 0.f), FVector2D(-1.f, 0.f), false, Dt);
	TestTrue(TEXT("Stops without reversing in one go"), Crossing.X >= 0.f);

	// The vertical brake input resolution
	FGSTN_WallBrakeInput B;
	TestTrue(TEXT("Stick up against a fall brakes"), UGSTN_WallControlUtils::ResolveBrake(B, FVector2D(0.f, -300.f), FVector2D(0.f, 1.f), false));
	TestFalse(TEXT("Stick down with the fall does not"), UGSTN_WallControlUtils::ResolveBrake(B, FVector2D(0.f, -300.f), FVector2D(0.f, -1.f), false));
	TestFalse(TEXT("Below the threshold does not"), UGSTN_WallControlUtils::ResolveBrake(B, FVector2D(0.f, -300.f), FVector2D(0.f, 0.1f), false));
	TestTrue(TEXT("The tag brakes"), UGSTN_WallControlUtils::ResolveBrake(B, FVector2D(0.f, -300.f), FVector2D::ZeroVector, true));
	B.bStickAgainstMotion = false;
	TestFalse(TEXT("Disabled rules do nothing"), UGSTN_WallControlUtils::ResolveBrake(B, FVector2D(0.f, -300.f), FVector2D(0.f, 1.f), false));
	B.bStickUp = true;
	TestTrue(TEXT("Stick up rule"), UGSTN_WallControlUtils::ResolveBrake(B, FVector2D(0.f, 300.f), FVector2D(0.f, 1.f), false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallControlTerminalTest, "GSTN.Wall.Control.VerticalTerminalApproach", GSTNWallControlTest::TestFlags)
bool FGSTN_WallControlTerminalTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallControlTest;
	const FGSTN_WallSlideControlSettings S;

	// From rest: speeds up, monotonic, never exceeds terminal, ends within 1%
	FVector2D V = FVector2D::ZeroVector;
	float PreviousDown = 0.f;
	for (int32 i = 0; i < 60 * 12; ++i)
	{
		V = UGSTN_WallControlUtils::StepSlideVelocity(S, V, FVector2D::ZeroVector, false, Dt);
		TestTrue(TEXT("Monotonic approach"), -V.Y >= PreviousDown - UE_KINDA_SMALL_NUMBER);
		TestTrue(TEXT("Never beyond terminal"), -V.Y <= S.TerminalDownSpeed + 0.01f);
		PreviousDown = -V.Y;
	}
	TestTrue(TEXT("Close to terminal after 12 s"), -V.Y > S.TerminalDownSpeed * 0.99f);

	// Entered faster than terminal: decays toward it gradually, not in one tick
	const FVector2D Fast = UGSTN_WallControlUtils::StepSlideVelocity(S, FVector2D(0.f, -1200.f), FVector2D::ZeroVector, false, Dt);
	TestTrue(TEXT("Not clamped instantly"), -Fast.Y > S.TerminalDownSpeed + 100.f && -Fast.Y < 1200.f);
	ControlNearlyEq(*this, TEXT("Settles to terminal"), -Run(S, FVector2D(0.f, -1200.f), FVector2D::ZeroVector, false, 3.f).Y, S.TerminalDownSpeed, 0.5f);

	// Upward entry speed decays under gravity, no instant stop
	const FVector2D Up = UGSTN_WallControlUtils::StepSlideVelocity(S, FVector2D(0.f, 400.f), FVector2D::ZeroVector, false, Dt);
	TestTrue(TEXT("Upward speed decays but stays"), Up.Y > 300.f && Up.Y < 400.f);
	TestTrue(TEXT("Eventually falls"), Run(S, FVector2D(0.f, 400.f), FVector2D::ZeroVector, false, 2.f).Y < 0.f);

	// Stick down lets go faster, only when DownBoost is set
	FGSTN_WallSlideControlSettings Boost = S;
	Boost.DownBoost = 200.f;
	ControlNearlyEq(*this, TEXT("Boosted terminal"), -Run(Boost, FVector2D(0.f, -600.f), FVector2D(0.f, -1.f), false, 12.f).Y, S.TerminalDownSpeed + 200.f, 2.f);
	ControlNearlyEq(*this, TEXT("No boost by default"), -Run(S, FVector2D(0.f, -600.f), FVector2D(0.f, -1.f), false, 12.f).Y, S.TerminalDownSpeed, 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallControlBrakeDownTest, "GSTN.Wall.Control.BrakeDownToMinSpeed", GSTNWallControlTest::TestFlags)
bool FGSTN_WallControlBrakeDownTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallControlTest;
	const FGSTN_WallSlideControlSettings S;
	FVector2D V(0.f, -600.f);
	int32 Ticks = 0;
	while (-V.Y > S.BrakeMinSpeed + UE_KINDA_SMALL_NUMBER && Ticks < 600)
	{
		const float Before = -V.Y;
		V = UGSTN_WallControlUtils::StepSlideVelocity(S, V, FVector2D::ZeroVector, true, Dt);
		TestTrue(TEXT("Brake never speeds up"), -V.Y <= Before);
		TestTrue(TEXT("Never below the min speed"), -V.Y >= S.BrakeMinSpeed - UE_KINDA_SMALL_NUMBER);
		++Ticks;
	}
	const int32 Expected = FMath::CeilToInt((600.f - S.BrakeMinSpeed) / (S.VerticalBrakeDecel * Dt));
	TestEqual(TEXT("Tick count follows the decel"), Ticks, Expected);
	ControlNearlyEq(*this, TEXT("Reaches the min speed exactly"), -V.Y, S.BrakeMinSpeed, 0.001f);

	// It then holds: no gravity gain while braking
	V = Run(S, V, FVector2D::ZeroVector, true, 2.f);
	ControlNearlyEq(*this, TEXT("Holds the min speed"), -V.Y, S.BrakeMinSpeed, 0.001f);

	// Releasing resumes the soft fall
	V = Run(S, V, FVector2D::ZeroVector, false, 0.5f);
	TestTrue(TEXT("Falls again after release"), -V.Y > S.BrakeMinSpeed + 50.f);

	// Braking an upward drift brings it to rest
	TestTrue(TEXT("Upward drift stopped"), FMath::IsNearlyZero(Run(S, FVector2D(0.f, 300.f), FVector2D::ZeroVector, true, 1.f).Y));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallControlClimbTest, "GSTN.Wall.Control.ClimbFullControl", GSTNWallControlTest::TestFlags)
bool FGSTN_WallControlClimbTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallControlTest;
	const FGSTN_WallClimbControlSettings C;
	auto Climb = [&](FVector2D V, const FVector2D& Input, float Seconds)
	{
		for (int32 i = 0, N = FMath::RoundToInt(Seconds / Dt); i < N; ++i)
		{
			V = UGSTN_WallControlUtils::StepClimbVelocity(C, V, Input, Dt);
		}
		return V;
	};

	for (const FVector2D& Dir : { FVector2D(0, 1), FVector2D(0, -1), FVector2D(1, 0), FVector2D(-1, 0), FVector2D(1, 1).GetSafeNormal(), FVector2D(-1, 2).GetSafeNormal() })
	{
		const FVector2D V = Climb(FVector2D::ZeroVector, Dir, 2.f);
		TestTrue(TEXT("Reaches climb speed in any direction"), V.Equals(Dir * C.ClimbSpeed, 0.01f));
	}
	ControlNearlyEq(*this, TEXT("Half stick, half speed"), Climb(FVector2D::ZeroVector, FVector2D(0.f, 0.5f), 2.f).Y, C.ClimbSpeed * 0.5f, 0.01f);
	ControlNearlyEq(*this, TEXT("Oversize stick is clamped"), Climb(FVector2D::ZeroVector, FVector2D(0.f, 3.f), 2.f).Y, C.ClimbSpeed, 0.01f);

	// No gravity: released, it stops and stays stopped (no slide)
	TestTrue(TEXT("Release holds position"), Climb(FVector2D(100.f, 200.f), FVector2D::ZeroVector, 2.f).IsNearlyZero());
	// Reversal passes through a controlled brake
	const FVector2D Step = UGSTN_WallControlUtils::StepClimbVelocity(C, FVector2D(0.f, C.ClimbSpeed), FVector2D(0.f, -1.f), Dt);
	TestTrue(TEXT("Reversing decelerates first"), Step.Y < C.ClimbSpeed && Step.Y > 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallControlGravityIndependentTest, "GSTN.Wall.Control.GravityIndependent", GSTNWallControlTest::TestFlags)
bool FGSTN_WallControlGravityIndependentTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallControlTest;
	const FGSTN_WallSlideControlSettings S;
	const FVector2D Start(300.f, -200.f);
	const FVector2D Input(-0.7f, 0.4f);

	// Reference in plane coordinates; the same state expressed through each gravity-up must evolve identically
	FVector2D Reference = Start;
	for (int32 i = 0; i < 90; ++i)
	{
		Reference = UGSTN_WallControlUtils::StepSlideVelocity(S, Reference, Input, i > 45, Dt);
	}

	for (const FVector& Up : UpVectors())
	{
		for (float Tilt : { 0.f, 25.f, -60.f })
		{
			const FVector N = WallNormalFor(Up, Tilt);
			FVector H, V;
			UGSTN_WallControlUtils::MakePlaneAxes(N, Up, H, V);
			ControlNearlyEq(*this, TEXT("Axes orthogonal"), FVector::DotProduct(H, V), 0.f, 0.001f);
			ControlNearlyEq(*this, TEXT("H on the plane"), FVector::DotProduct(H, N), 0.f, 0.001f);
			TestTrue(TEXT("V points up along the wall"), FVector::DotProduct(V, Up) > 0.f);

			FVector World = UGSTN_WallControlUtils::FromPlane(Start, H, V);
			for (int32 i = 0; i < 90; ++i)
			{
				const FVector2D Plane = UGSTN_WallControlUtils::ToPlane(World, H, V);
				World = UGSTN_WallControlUtils::FromPlane(UGSTN_WallControlUtils::StepSlideVelocity(S, Plane, Input, i > 45, Dt), H, V);
			}
			TestTrue(TEXT("Same result in every frame"), UGSTN_WallControlUtils::ToPlane(World, H, V).Equals(Reference, 0.05f));
			ControlNearlyEq(*this, TEXT("Stays on the plane"), FVector::DotProduct(World, N), 0.f, 0.01f);
		}
	}

	// Ceiling: no gravity tangent, axes still well defined
	FVector H, V;
	UGSTN_WallControlUtils::MakePlaneAxes(FVector(0, 0, -1), FVector(0, 0, 1), H, V);
	ControlNearlyEq(*this, TEXT("Ceiling axes orthogonal"), FVector::DotProduct(H, V), 0.f, 0.001f);
	TestTrue(TEXT("Ceiling axes unit"), H.IsNormalized() && V.IsNormalized());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
