// Camera anchor (GSTN_CameraAnchor.h): GSTN.Camera.Anchor.*
//
// Pure: no world. They pin the two promises of the fix: the anchor does not move when only the capsule moves (the root jumps, the drawn mesh
// stays), and the damped anchor turns frame to frame vertical steps into a smooth, non overshooting curve.

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/GSTN_CameraAnchor.h"
#include "GASNPPTest/GSTN_PlaytestCamera.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CameraAnchorAirHoldTest, "GSTN.Camera.Anchor.AirHoldKeepsGroundHeight", GSTNCameraAnchorTest::TestFlags)
bool FGSTN_CameraAnchorAirHoldTest::RunTest(const FString&)
{
	using namespace GSTNCameraAnchorTest;
	// Mario / Astro rule: a normal jump (here 200 cm, under the 250 cm rise band) does not move the camera; a 400 cm climb moves it by 150
	FGSTN_CameraAnchorSettings S = Settings(0.f);
	S.bHoldGroundHeightInAir = true;
	S.AirRiseBand = 250.f;
	S.AirFallBand = 0.f;
	FGSTN_CameraAnchorDamper D;
	D.Reset(FVector::ZeroVector);
	D.Step(FVector::ZeroVector, FVector::UpVector, Dt, S, false);
	Near(*this, TEXT("jump 200: camera stays on the ground"), D.Step(FVector(0, 0, 200), FVector::UpVector, Dt, S, true).Z, 0.0, 0.01);
	Near(*this, TEXT("climb 400: follows minus the band"), D.Step(FVector(0, 0, 390), FVector::UpVector, Dt, S, true).Z, 140.0, 0.01);
	// Falling below the take-off height is followed right away (fall band 0)
	Near(*this, TEXT("drop below ground: followed"), D.Step(FVector(0, 0, -100), FVector::UpVector, Dt, S, true).Z, -100.0, 0.01);
	// Off: the camera follows the jump
	S.bHoldGroundHeightInAir = false;
	FGSTN_CameraAnchorDamper Off;
	Off.Reset(FVector::ZeroVector);
	Off.Step(FVector::ZeroVector, FVector::UpVector, Dt, S, false);
	Near(*this, TEXT("hold off: follows the jump"), Off.Step(FVector(0, 0, 200), FVector::UpVector, Dt, S, true).Z, 200.0, 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CameraAnchorLandingCatchUpTest, "GSTN.Camera.Anchor.LandingOnHigherGroundCatchesUpSoftly", GSTNCameraAnchorTest::TestFlags)
bool FGSTN_CameraAnchorLandingCatchUpTest::RunTest(const FString&)
{
	using namespace GSTNCameraAnchorTest;
	// Jump onto a 150 cm platform: in the air the camera stays at 0; on landing it rises with LandingHalfLife (0.2 s), not the 0.07 s
	// capsule half life, and the 80 cm lag clamp does not snap it up meanwhile
	FGSTN_CameraAnchorSettings S = Settings(0.07f, 0.f, 80.f);
	S.bHoldGroundHeightInAir = true;
	S.AirRiseBand = 250.f;
	S.LandingHalfLife = 0.2f;
	FGSTN_CameraAnchorDamper D;
	D.Reset(FVector::ZeroVector);
	D.Step(FVector::ZeroVector, FVector::UpVector, Dt, S, false);
	Near(*this, TEXT("apex: held"), D.Step(FVector(0, 0, 150), FVector::UpVector, Dt, S, true).Z, 0.0, 0.01);
	double Z = 0.0;
	const int32 Frames = FMath::RoundToInt(0.2f / Dt);
	for (int32 Frame = 0; Frame < Frames; ++Frame)
	{
		Z = D.Step(FVector(0, 0, 150), FVector::UpVector, Dt, S, false).Z;
	}
	// Critically damped from rest: 0.596 of the step left after one half life -> about 60.6 cm covered of 150
	Near(*this, TEXT("one landing half life: ~40% covered"), Z, 150.0 * (1.0 - 0.596), 6.0);
	for (int32 Frame = 0; Frame < 4 * Frames; ++Frame)
	{
		Z = D.Step(FVector(0, 0, 150), FVector::UpVector, Dt, S, false).Z;
	}
	Near(*this, TEXT("settles on the platform"), Z, 150.0, 3.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CameraTiltShiftMathTest, "GSTN.Camera.TiltShift.Math", GSTNCameraAnchorTest::TestFlags)
bool FGSTN_CameraTiltShiftMathTest::RunTest(const FString&)
{
	using namespace GSTNCameraAnchorTest;
	FMinimalViewInfo View;
	View.Location = FVector::ZeroVector;
	View.Rotation = FRotator::ZeroRotator;
	View.FOV = 90.f;
	View.AspectRatio = 1.f;
	Near(*this, TEXT("point ahead: screen centre"), UGSTN_PlaytestCameraModifier::ScreenV(View, FVector(500, 0, 0)), 0.5, 0.001);
	Near(*this, TEXT("45 deg up: top edge"), UGSTN_PlaytestCameraModifier::ScreenV(View, FVector(500, 0, 500)), 0.0, 0.001);
	Near(*this, TEXT("45 deg down: bottom edge"), UGSTN_PlaytestCameraModifier::ScreenV(View, FVector(500, 0, -500)), 1.0, 0.001);
	// Telephoto: 90 -> 45 deg needs tan(45)/tan(22.5) = 2.414 times the distance; the inverse gives the FOV back
	Near(*this, TEXT("telephoto 90->45 distance scale"), UGSTN_PlaytestCameraModifier::TelephotoDistanceScale(90.f, 45.f), 2.41421, 0.001);
	Near(*this, TEXT("FOV for that scale"), UGSTN_PlaytestCameraModifier::FOVForDistanceScale(90.f, 2.41421f), 45.0, 0.01);
	Near(*this, TEXT("scale 1 keeps the FOV"), UGSTN_PlaytestCameraModifier::FOVForDistanceScale(70.f, 1.f), 70.0, 0.01);
	FGSTN_CameraTiltShiftSettings T;
	T.Fstop = 2.f;
	T.Intensity = 2.f;
	Near(*this, TEXT("intensity 2 halves the f-stop"), UGSTN_PlaytestCameraModifier::TiltShiftFstop(T), 1.0, 0.001);
	T.Intensity = 10.f;
	Near(*this, TEXT("f-stop floor 0.7"), UGSTN_PlaytestCameraModifier::TiltShiftFstop(T), 0.7, 0.001);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CameraShiftTest, "GSTN.Camera.Shift.StraightenKeepsCharacterOnScreen", GSTNCameraAnchorTest::TestFlags)
bool FGSTN_CameraShiftTest::RunTest(const FString&)
{
	using namespace GSTNCameraAnchorTest;
	// Camera 400 cm back and 250 cm up looking 26 deg down at the character: levelling it fully with the shift leaves the character exactly
	// where it was on screen, and a vertical edge projects as a vertical line (same x at both ends)
	FMinimalViewInfo View;
	View.Location = FVector(-400, 0, 250);
	View.Rotation = FRotator(-26, 0, 0);
	View.FOV = 90.f;
	View.AspectRatio = 16.f / 9.f;
	const FVector Character(0, 30, 90);
	const FVector2D Before = UGSTN_PlaytestCameraModifier::ProjectNDC(View, Character);
	FMinimalViewInfo Shifted = View;
	const FVector2D Offset = UGSTN_PlaytestCameraModifier::StraightenWithShift(Shifted, Character, 1.f);
	Shifted.OffCenterProjectionOffset = decltype(Shifted.OffCenterProjectionOffset)(Offset.X, Offset.Y);
	const FVector2D After = UGSTN_PlaytestCameraModifier::ProjectNDC(Shifted, Character);
	Near(*this, TEXT("pitch levelled"), Shifted.Rotation.Pitch, 0.0, 0.001);
	Near(*this, TEXT("character x kept"), After.X, Before.X, 0.001);
	Near(*this, TEXT("character y kept"), After.Y, Before.Y, 0.001);
	const FVector2D Top = UGSTN_PlaytestCameraModifier::ProjectNDC(Shifted, FVector(200, 300, 400));
	const FVector2D Bottom = UGSTN_PlaytestCameraModifier::ProjectNDC(Shifted, FVector(200, 300, 0));
	Near(*this, TEXT("verticals stay vertical"), Top.X, Bottom.X, 0.001);
	// Straighten 0: nothing changes
	FMinimalViewInfo Same = View;
	const FVector2D None = UGSTN_PlaytestCameraModifier::StraightenWithShift(Same, Character, 0.f);
	Near(*this, TEXT("straighten 0: no offset"), None.Size(), 0.0, 0.0001);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CameraTiltPlaneTest, "GSTN.Camera.TiltShift.TiltedPlaneNormal", GSTNCameraAnchorTest::TestFlags)
bool FGSTN_CameraTiltPlaneTest::RunTest(const FString&)
{
	using namespace GSTNCameraAnchorTest;
	// Tilt 0 = plane parallel to the sensor (normal = view forward); tilt 90 - pitch = the plane lies on the floor (normal = world up)
	const FRotator Down30(-30, 0, 0);
	const FVector N0 = UGSTN_PlaytestCameraModifier::TiltedPlaneNormal(Down30, 0.f);
	Near(*this, TEXT("tilt 0 = forward"), FVector::DotProduct(N0, Down30.Vector()), 1.0, 0.0001);
	const FVector NFloor = UGSTN_PlaytestCameraModifier::TiltedPlaneNormal(Down30, 60.f);
	Near(*this, TEXT("tilt 60 at pitch -30 = floor"), FMath::Abs(FVector::DotProduct(NFloor, FVector::UpVector)), 1.0, 0.0001);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CameraAnchorHighJumpNoSnapTest, "GSTN.Camera.Anchor.HighJumpInRiseBandDoesNotSnap", GSTNCameraAnchorTest::TestFlags)
bool FGSTN_CameraAnchorHighJumpNoSnapTest::RunTest(const FString&)
{
	using namespace GSTNCameraAnchorTest;
	// Bug pass 4 K2: with a rise band larger than the snap distance, a jump that goes more than SnapDistance above the held camera was taken for
	// a teleport: the camera popped onto the character and the air hold was lost until the landing. The test now measures the held point
	FGSTN_CameraAnchorSettings S = Settings(0.f);
	S.bHoldGroundHeightInAir = true;
	S.AirRiseBand = 600.f;
	S.AirFallBand = 0.f;
	S.SnapDistance = 400.f;
	FGSTN_CameraAnchorDamper D;
	D.Reset(FVector::ZeroVector);
	D.Step(FVector::ZeroVector, FVector::UpVector, Dt, S, false);
	for (int32 Height = 50; Height <= 550; Height += 50)
	{
		Near(*this, *FString::Printf(TEXT("jump at %d cm: camera stays on the ground"), Height), D.Step(FVector(0, 0, Height), FVector::UpVector, Dt, S, true).Z, 0.0, 0.01);
	}
	TestTrue(TEXT("the ground height is still known"), D.bHasGround);
	// A real teleport in the air still snaps
	const FVector Far(2000, 0, 300);
	TestTrue(TEXT("teleport in the air snaps"), D.Step(Far, FVector::UpVector, Dt, S, true).Equals(Far, 1e-4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_CameraScreenVShiftTest, "GSTN.Camera.TiltShift.ScreenVFollowsShift", GSTNCameraAnchorTest::TestFlags)
bool FGSTN_CameraScreenVShiftTest::RunTest(const FString&)
{
	using namespace GSTNCameraAnchorTest;
	// Bug pass 4 K1: the screen band centre (ScreenV) ignored the Shift's off-center offset. It must agree with ProjectNDC (v = 0.5 - 0.5 y)
	FMinimalViewInfo View;
	View.Location = FVector::ZeroVector;
	View.Rotation = FRotator::ZeroRotator;
	View.FOV = 90.f;
	View.AspectRatio = 16.f / 9.f;
	View.OffCenterProjectionOffset = decltype(View.OffCenterProjectionOffset)(0.0, 0.5);
	const FVector Points[] = { FVector(500, 0, 0), FVector(500, 40, 120), FVector(800, -60, -200) };
	for (const FVector& Point : Points)
	{
		Near(*this, TEXT("ScreenV = 0.5 - 0.5 NDC y"), UGSTN_PlaytestCameraModifier::ScreenV(View, Point), 0.5 - 0.5 * UGSTN_PlaytestCameraModifier::ProjectNDC(View, Point).Y, 0.001);
	}
	Near(*this, TEXT("point ahead, offset 0.5: lower on screen"), UGSTN_PlaytestCameraModifier::ScreenV(View, FVector(500, 0, 0)), 0.75, 0.001);
	return true;
}

#endif
