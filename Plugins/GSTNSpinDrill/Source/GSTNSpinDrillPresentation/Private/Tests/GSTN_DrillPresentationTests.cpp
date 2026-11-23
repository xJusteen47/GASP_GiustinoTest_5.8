// Automation tests of the spin and drill presentation (plan D11): the pose is a function of the fragment and the frame.

#include "GSTN_DrillPresentationComponent.h"
#include "Misc/AutomationTest.h"
#include "Sim/GSTN_DrillFragment.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillPresentationPoseTest, "GSTNSpinDrill.Presentation.PoseFromFragment",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)
bool FGSTN_DrillPresentationPoseTest::RunTest(const FString& Parameters)
{
	const FGSTN_DrillWobbleTuning Drill;
	const TArray<float> NoSamples;
	TestFalse(TEXT("no fragment: inactive"), UGSTN_DrillPresentationComponent::ComputePose(nullptr, 10, 2.5f, Drill, 60.f, NoSamples).bActive);

	FGSTN_DrillFragment Fragment;
	Fragment.Phase = EGSTN_DrillPhase::Spinning;
	Fragment.PhaseStartFrame = 100;
	// 2.5 turns a second at 60 Hz: 15 degrees a frame, clockwise = negative yaw
	const FGSTN_DrillPresentationPose Cw = UGSTN_DrillPresentationComponent::ComputePose(&Fragment, 102, 2.5f, Drill, 60.f, NoSamples);
	TestTrue(TEXT("clockwise spin: -30 degrees after two frames"), Cw.bActive && FMath::IsNearlyEqual(Cw.SpinYawDegrees, -30.f, 0.01f));
	Fragment.Flags = GSTNDrillFlags::CounterClockwise;
	const FGSTN_DrillPresentationPose Ccw = UGSTN_DrillPresentationComponent::ComputePose(&Fragment, 102, 2.5f, Drill, 60.f, NoSamples);
	TestTrue(TEXT("counter-clockwise: +30"), FMath::IsNearlyEqual(Ccw.SpinYawDegrees, 30.f, 0.01f));
	TestTrue(TEXT("the same frame gives the same pose (every machine)"), FMath::IsNearlyEqual(Ccw.SpinYawDegrees,
		UGSTN_DrillPresentationComponent::ComputePose(&Fragment, 102, 2.5f, Drill, 60.f, NoSamples).SpinYawDegrees));

	Fragment.Phase = EGSTN_DrillPhase::Seating;
	Fragment.LatchFrame = 200;
	Fragment.SeatFrames = 12;
	const FGSTN_DrillPresentationPose Seat = UGSTN_DrillPresentationComponent::ComputePose(&Fragment, 203, 2.5f, Drill, 60.f, NoSamples);
	TestTrue(TEXT("seating: a wobble that is decaying"), Seat.bActive && Seat.WobbleDegrees > 0.f && Seat.WobbleDegrees < Drill.MaxWobbleDegrees);
	const FGSTN_DrillPresentationPose Seated = UGSTN_DrillPresentationComponent::ComputePose(&Fragment, 212, 2.5f, Drill, 60.f, NoSamples);
	TestTrue(TEXT("at the end of the seat: no wobble"), FMath::IsNearlyZero(Seated.WobbleDegrees));

	Fragment.Phase = EGSTN_DrillPhase::Lag;
	TestFalse(TEXT("the lag shows nothing"), UGSTN_DrillPresentationComponent::ComputePose(&Fragment, 230, 2.5f, Drill, 60.f, NoSamples).bActive);
	TestTrue(TEXT("inactive pose: no rotation"), UGSTN_DrillPresentationComponent::PoseRotation(FGSTN_DrillPresentationPose()).Equals(FQuat::Identity));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
