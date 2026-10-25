// Pose of the A/B map's moving wall (GSTN.Wall.TestMover.*). Pure: no world.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WallMovement/Debug/GSTN_WallTestMover.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallTestMoverPoseTest, "GSTN.Wall.TestMover.Pose",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_WallTestMoverPoseTest::RunTest(const FString& Parameters)
{
	// 300 cm/s over +-300 cm: 0 -> 300 in 1 s, back through 0 at 2 s, -300 at 3 s, 0 at 4 s
	TestTrue(TEXT("t=0"), FMath::IsNearlyZero(AGSTN_WallTestMover::TriangleWave(0.0, 300.0, 300.0)));
	TestTrue(TEXT("Peak at 1 s"), FMath::IsNearlyEqual(AGSTN_WallTestMover::TriangleWave(1.0, 300.0, 300.0), 300.0, 1e-6));
	TestTrue(TEXT("Zero at 2 s"), FMath::IsNearlyZero(AGSTN_WallTestMover::TriangleWave(2.0, 300.0, 300.0), 1e-6));
	TestTrue(TEXT("Trough at 3 s"), FMath::IsNearlyEqual(AGSTN_WallTestMover::TriangleWave(3.0, 300.0, 300.0), -300.0, 1e-6));
	TestTrue(TEXT("Periodic"), FMath::IsNearlyEqual(AGSTN_WallTestMover::TriangleWave(4.5, 300.0, 300.0), AGSTN_WallTestMover::TriangleWave(0.5, 300.0, 300.0), 1e-6));
	TestTrue(TEXT("Slope is the speed"), FMath::IsNearlyEqual(AGSTN_WallTestMover::TriangleWave(0.5, 150.0, 300.0), 75.0, 1e-6));
	TestEqual(TEXT("No speed, no motion"), AGSTN_WallTestMover::TriangleWave(1.0, 0.0, 300.0), 0.0);

	FVector Loc;
	FRotator Rot;
	AGSTN_WallTestMover::ComputePose(1.0, FVector(10, 20, 30), FRotator(0, 90, 0), FVector(0, 5, 0), 300.f, 300.f, 20.f, 45.f, Loc, Rot);
	TestTrue(TEXT("Moves along the (normalised) axis"), Loc.Equals(FVector(10, 320, 30), 1e-3));
	TestTrue(TEXT("Yaw 20 deg/s"), FMath::IsNearlyEqual(Rot.Yaw, 110.0, 1e-3));
	return true;
}

#endif
