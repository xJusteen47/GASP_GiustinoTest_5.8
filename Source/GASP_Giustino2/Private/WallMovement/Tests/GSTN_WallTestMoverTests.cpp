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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallTestMoverClockTest, "GSTN.Wall.TestMover.ClockOffsetSlews",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_WallTestMoverClockTest::RunTest(const FString& Parameters)
{
	// The pose is read at the server's clock: the offset to it is corrected slowly (a jump would be a spike in the velocity Chaos derives from the pose)
	constexpr double Dt = 1.0 / 60.0;
	const double Slew = 0.02;
	const double Snap = 0.25;

	double Offset = 0.0;
	Offset = AGSTN_WallTestMover::StepTimeOffset(Offset, 0.05, Dt, Slew, Snap);
	TestTrue(*FString::Printf(TEXT("A small error is corrected at the slew rate (%.6f)"), Offset), FMath::IsNearlyEqual(Offset, Slew * Dt, 1e-9));

	for (int32 i = 0; i < 60 * 5; ++i)
	{
		Offset = AGSTN_WallTestMover::StepTimeOffset(Offset, 0.05, Dt, Slew, Snap);
	}
	TestTrue(*FString::Printf(TEXT("It converges and stays (%.6f)"), Offset), FMath::IsNearlyEqual(Offset, 0.05, 1e-9));

	Offset = AGSTN_WallTestMover::StepTimeOffset(Offset, 0.05 - 0.01, Dt, Slew, Snap);
	TestTrue(TEXT("Corrects downwards too"), Offset < 0.05 && Offset > 0.05 - 0.01);

	TestEqual(TEXT("A large error is taken at once"), AGSTN_WallTestMover::StepTimeOffset(0.0, 3.0, Dt, Slew, Snap), 3.0);
	TestEqual(TEXT("A large negative error too"), AGSTN_WallTestMover::StepTimeOffset(1.0, -4.0, Dt, Slew, Snap), -4.0);
	TestEqual(TEXT("No slew rate holds the offset"), AGSTN_WallTestMover::StepTimeOffset(0.1, 0.12, Dt, 0.0, Snap), 0.1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallTestMoverSimTimeTest, "GSTN.Wall.TestMover.PoseOfASimulationTickIsAFunctionOfItsSimTime",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_WallTestMoverSimTimeTest::RunTest(const FString& Parameters)
{
	// The same frame meets the same wall wherever it is simulated: the pose depends on the step's simulation time only (no clock, no previous pose), so a client
	// that predicts a frame half a second before the server simulates it, and re-simulates it after a correction, gets the server's wall
	TestTrue(TEXT("The end of the step"), FMath::IsNearlyEqual(AGSTN_WallTestMover::SimPoseTimeSeconds(1000.0, 16.667), 1.016667, 1e-9));

	const FVector Base(0, 500, 0);
	const FRotator Yaw(0, 0, 0);
	auto PoseAt = [&](double T)
	{
		FVector L;
		FRotator R;
		AGSTN_WallTestMover::ComputePose(T, Base, Yaw, FVector(0, 1, 0), 300.f, 250.f, 20.f, 30.f, L, R);
		return TPair<FVector, FRotator>(L, R);
	};
	const double Frame = AGSTN_WallTestMover::SimPoseTimeSeconds(7316.667, 16.667);
	const TPair<FVector, FRotator> Server = PoseAt(Frame);
	// "Predicted", then "re-simulated" after other frames were evaluated in between
	const TPair<FVector, FRotator> Predicted = PoseAt(Frame);
	for (double T = 5.0; T < 9.0; T += 0.123)
	{
		PoseAt(T);
	}
	const TPair<FVector, FRotator> Replayed = PoseAt(Frame);
	TestTrue(TEXT("Predicted = server"), Server.Key.Equals(Predicted.Key, 1e-9) && Server.Value.Equals(Predicted.Value, 1e-9));
	TestTrue(TEXT("Replayed = server"), Server.Key.Equals(Replayed.Key, 1e-9) && Server.Value.Equals(Replayed.Value, 1e-9));
	TestFalse(TEXT("A frame later is another pose"), Server.Key.Equals(PoseAt(Frame + 0.0167).Key, 0.01));
	return true;
}

#endif
