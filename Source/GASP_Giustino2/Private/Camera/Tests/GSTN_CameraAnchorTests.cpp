// Camera anchor (GSTN_CameraAnchor.h): GSTN.Camera.Anchor.*
//
// Pure: no world. They pin the two promises of the fix: the anchor does not move when only the capsule moves (the root jumps, the drawn mesh
// stays), and the damped anchor turns frame to frame vertical steps into a smooth, non overshooting curve.

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/GSTN_CameraAnchor.h"
#include "Misc/AutomationTest.h"

namespace GSTNCameraAnchorTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Dt = 1.f / 60.f;

	static FGSTN_CameraAnchorSettings Settings(float HalfLife, float DeadBand = 0.f, float MaxLag = 0.f)
	{
		FGSTN_CameraAnchorSettings S;
		S.UpHalfLife = HalfLife;
		S.DownHalfLife = HalfLife;
		S.DeadBand = DeadBand;
		S.MaxVerticalLag = MaxLag;
		S.HorizontalHalfLife = 0.f;
		S.SnapDistance = 400.f;
		return S;
	}

	static void Near(FAutomationTestBase& Test, const TCHAR* What, double A, double B, double Tolerance)
	{
		Test.TestTrue(FString::Printf(TEXT("%s (%.3f vs %.3f)"), What, A, B), FMath::Abs(A - B) <= Tolerance);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CameraAnchorStepDropTest, "GSTN.Camera.Anchor.StepDropIsSmoothNoOvershoot", GSTNCameraAnchorTest::TestFlags)
bool FGSTN_CameraAnchorStepDropTest::RunTest(const FString&)
{
	using namespace GSTNCameraAnchorTest;
	// A 35 cm one-frame drop (dair tuck capsule, knockdown): the camera goes down monotonically, never below. A critically damped spring from rest
	// has e^-2ln2 (1 + 2ln2) = 0.596 of the step left after one half life
	const FGSTN_CameraAnchorSettings S = Settings(0.07f);
	FGSTN_CameraAnchorDamper D;
	D.Reset(FVector(0, 0, 100));
	const FVector Target(0, 0, 65);
	double Previous = 100.0;
	double AtHalfLife = 0.0;
	for (int32 Frame = 1; Frame <= 60; ++Frame)
	{
		const double Z = D.Step(Target, FVector::UpVector, Dt, S).Z;
		TestTrue(TEXT("monotonic"), Z <= Previous + 1e-6);
		TestTrue(TEXT("no overshoot"), Z >= 65.0 - 1e-3);
		Previous = Z;
		if (FMath::IsNearlyEqual(Frame * Dt, 0.07f, Dt * 0.5f))
		{
			AtHalfLife = Z;
		}
	}
	// 0.07 s is 4.2 frames: the nearest frame (4) has a little more left
	Near(*this, TEXT("about 60% left after one half life"), AtHalfLife, 65.0 + 35.0 * 0.596, 2.0);
	Near(*this, TEXT("settled after a second"), Previous, 65.0, 0.05);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CameraAnchorJitterTest, "GSTN.Camera.Anchor.FrameJitterIsFiltered", GSTNCameraAnchorTest::TestFlags)
bool FGSTN_CameraAnchorJitterTest::RunTest(const FString&)
{
	using namespace GSTNCameraAnchorTest;
	// The root pops 20 cm up and down every sim frame (capsule shrink / GroundedRestore): the camera moves a fraction of it
	const FGSTN_CameraAnchorSettings S = Settings(0.07f);
	FGSTN_CameraAnchorDamper D;
	D.Reset(FVector::ZeroVector);
	double Min = TNumericLimits<double>::Max();
	double Max = TNumericLimits<double>::Lowest();
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		const double Z = D.Step(FVector(0, 0, (Frame % 2) ? 20.0 : 0.0), FVector::UpVector, Dt, S).Z;
		if (Frame >= 60)
		{
			Min = FMath::Min(Min, Z);
			Max = FMath::Max(Max, Z);
		}
	}
	TestTrue(FString::Printf(TEXT("peak to peak %.2f cm under 5 of the 20 cm input"), Max - Min), Max - Min < 5.0);
	Near(*this, TEXT("centred on the mean"), (Max + Min) * 0.5, 10.0, 3.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CameraAnchorZeroHalfLifeTest, "GSTN.Camera.Anchor.ZeroHalfLifeFollowsExactly", GSTNCameraAnchorTest::TestFlags)
bool FGSTN_CameraAnchorZeroHalfLifeTest::RunTest(const FString&)
{
	using namespace GSTNCameraAnchorTest;
	const FGSTN_CameraAnchorSettings S = Settings(0.f);
	FGSTN_CameraAnchorDamper D;
	D.Reset(FVector::ZeroVector);
	const FVector Out = D.Step(FVector(10, -5, 33), FVector::UpVector, Dt, S);
	TestTrue(TEXT("exact"), Out.Equals(FVector(10, -5, 33), 1e-4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CameraAnchorLagClampTest, "GSTN.Camera.Anchor.MaxVerticalLagClamps", GSTNCameraAnchorTest::TestFlags)
bool FGSTN_CameraAnchorLagClampTest::RunTest(const FString&)
{
	using namespace GSTNCameraAnchorTest;
	// A long slow half life and a fast fall: the camera trails by at most MaxVerticalLag
	const FGSTN_CameraAnchorSettings S = Settings(1.f, 0.f, 30.f);
	FGSTN_CameraAnchorDamper D;
	D.Reset(FVector::ZeroVector);
	double Z = 0.0;
	double Target = 0.0;
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		Target -= 15.0;  // 900 cm/s
		Z = D.Step(FVector(0, 0, Target), FVector::UpVector, Dt, S).Z;
		TestTrue(TEXT("within the lag"), Z - Target <= 30.0 + 1e-3);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CameraAnchorSnapTest, "GSTN.Camera.Anchor.TeleportSnaps", GSTNCameraAnchorTest::TestFlags)
bool FGSTN_CameraAnchorSnapTest::RunTest(const FString&)
{
	using namespace GSTNCameraAnchorTest;
	const FGSTN_CameraAnchorSettings S = Settings(0.5f);
	FGSTN_CameraAnchorDamper D;
	D.Reset(FVector::ZeroVector);
	const FVector Out = D.Step(FVector(0, 0, 1000), FVector::UpVector, Dt, S);
	TestTrue(TEXT("snapped"), Out.Equals(FVector(0, 0, 1000), 1e-4));
	TestFalse(TEXT("not valid before the first step"), FGSTN_CameraAnchorDamper().bValid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CameraAnchorDeadBandTest, "GSTN.Camera.Anchor.DeadBandHoldsSmallMoves", GSTNCameraAnchorTest::TestFlags)
bool FGSTN_CameraAnchorDeadBandTest::RunTest(const FString&)
{
	using namespace GSTNCameraAnchorTest;
	const FGSTN_CameraAnchorSettings S = Settings(0.f, 5.f);
	FGSTN_CameraAnchorDamper D;
	D.Reset(FVector::ZeroVector);
	Near(*this, TEXT("3 cm inside the band: still"), D.Step(FVector(0, 0, 3), FVector::UpVector, Dt, S).Z, 0.0, 1e-4);
	Near(*this, TEXT("12 cm: followed minus the band"), D.Step(FVector(0, 0, 12), FVector::UpVector, Dt, S).Z, 7.0, 1e-4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CameraAnchorUpAxisTest, "GSTN.Camera.Anchor.VerticalIsAlongUp", GSTNCameraAnchorTest::TestFlags)
bool FGSTN_CameraAnchorUpAxisTest::RunTest(const FString&)
{
	using namespace GSTNCameraAnchorTest;
	// Sideways gravity (planetoids later): damping acts along Up only, the rest follows at once
	const FGSTN_CameraAnchorSettings S = Settings(0.2f);
	const FVector Up(1, 0, 0);
	FGSTN_CameraAnchorDamper D;
	D.Reset(FVector::ZeroVector);
	const FVector Out = D.Step(FVector(-40, 25, 30), Up, Dt, S);
	Near(*this, TEXT("plane Y followed"), Out.Y, 25.0, 1e-4);
	Near(*this, TEXT("plane Z followed"), Out.Z, 30.0, 1e-4);
	TestTrue(TEXT("along Up damped"), Out.X > -40.0 + 1.0 && Out.X < 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CameraAnchorCapsuleShiftTest, "GSTN.Camera.Anchor.StandingRootIgnoresCapsuleShift", GSTNCameraAnchorTest::TestFlags)
bool FGSTN_CameraAnchorCapsuleShiftTest::RunTest(const FString&)
{
	// GASP mesh: root at the capsule bottom, yaw -90. The body capsule shrinks by 30 cm from the floor: the actor root goes down 30, the mesh
	// base goes up 30 (FGSTN_BodyCapsuleModifier::ApplyShape), the drawn mesh does not move. The standing root read from the mesh must not move
	const FTransform StandingRel(FRotator(0, -90, 0), FVector(0, 0, -90));
	for (const double Yaw : { 0.0, 37.0, -120.0 })
	{
		const FTransform Actor(FRotator(0, Yaw, 0), FVector(100, 50, 90));
		const FVector Before = FGSTN_CameraAnchorDamper::StandingRootFromVisual(StandingRel * Actor, StandingRel);
		TestTrue(TEXT("standing: the anchor is the root"), Before.Equals(Actor.GetLocation(), 1e-3));

		const FTransform ShrunkActor(FRotator(0, Yaw, 0), FVector(100, 50, 60));
		const FTransform ShiftedRel(FRotator(0, -90, 0), FVector(0, 0, -60));
		const FTransform VisualWorld = ShiftedRel * ShrunkActor;
		TestTrue(TEXT("the drawn mesh did not move"), VisualWorld.GetLocation().Equals((StandingRel * Actor).GetLocation(), 1e-3));
		const FVector After = FGSTN_CameraAnchorDamper::StandingRootFromVisual(VisualWorld, StandingRel);
		TestTrue(FString::Printf(TEXT("yaw %.0f: the anchor did not move with the capsule"), Yaw), After.Equals(Before, 1e-3));
	}
	return true;
}

#endif
