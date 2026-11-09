#include "Tests/GSTN_CombatTestUtils.h"
#include "Timeline/GSTN_StartupSteer.h"
#include "Timeline/GSTN_TimelineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

// Startup steering with the aim stick (plan precisione-weakpoint 5): goldens of the pure clamp the sim calls every startup frame.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_StartupSteerTest, "GSTNCombat.Core.StartupSteer.Goldens", GSTN_CORE_TEST_FLAGS)
bool FGSTN_StartupSteerTest::RunTest(const FString& Parameters)
{
	constexpr float Tol = 1.e-3f;
	using S = FGSTN_StartupSteer;

	TestEqual(TEXT("default max steer is 20 deg"), FGSTN_CombatTimelineData().MaxSteerDegrees, 20.f);

	// Centred stick, dead zone and a move that does not steer keep the start yaw
	TestEqual(TEXT("centred stick"), S::SteerYaw(30.f, FVector2D::ZeroVector, 0.f, 20.f), 30.f, Tol);
	TestEqual(TEXT("inside the dead zone"), S::SteerYaw(30.f, FVector2D(0.2, 0.0), 0.f, 20.f), 30.f, Tol);
	TestEqual(TEXT("MaxSteerDegrees 0"), S::SteerYaw(30.f, FVector2D(1.0, 0.0), 0.f, 0.f), 30.f, Tol);

	// Stick right with the camera at 0 aims at yaw 90: clamped to start + 20
	TestEqual(TEXT("right, clamped"), S::SteerYaw(0.f, FVector2D(1.0, 0.0), 0.f, 20.f), 20.f, Tol);
	TestEqual(TEXT("left, clamped"), S::SteerYaw(0.f, FVector2D(-1.0, 0.0), 0.f, 20.f), -20.f, Tol);

	// Inside the cone the move faces the stick exactly (stick 10 deg right of camera forward)
	const FVector2D TenRight(FMath::Sin(FMath::DegreesToRadians(10.0)), FMath::Cos(FMath::DegreesToRadians(10.0)));
	TestEqual(TEXT("inside the cone"), S::SteerYaw(0.f, TenRight, 0.f, 20.f), 10.f, Tol);

	// The camera yaw turns the stick into world space: stick forward with the camera at 15 aims at 15
	TestEqual(TEXT("camera relative"), S::SteerYaw(0.f, FVector2D(0.0, 1.0), 15.f, 20.f), 15.f, Tol);

	// Across the +-180 seam: start 170, stick aiming at -170 (20 deg to the right through 180) -> 190 = -170
	TestEqual(TEXT("across the seam"), S::SteerYaw(170.f, FVector2D(0.0, 1.0), -170.f, 20.f), -170.f, Tol);
	TestEqual(TEXT("across the seam, clamped"), S::SteerYaw(170.f, FVector2D(0.0, 1.0), -150.f, 20.f), -170.f, Tol);

	// Clamp against the start, not accumulated: the same command on any startup frame gives the same yaw
	TestEqual(TEXT("idempotent"), S::SteerYaw(0.f, FVector2D(1.0, 0.0), 0.f, 20.f), S::SteerYaw(0.f, FVector2D(1.0, 0.0), 0.f, 20.f));
	return true;
}

#endif
