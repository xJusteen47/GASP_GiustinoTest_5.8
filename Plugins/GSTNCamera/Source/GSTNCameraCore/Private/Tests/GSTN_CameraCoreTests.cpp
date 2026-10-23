// Pure automation tests of the camera maths (camera.md §11, GSTNCamera.Core.*). No world.
// Every frame-dependent test runs for several Up vectors (flipped, sideways, diagonal) so a hidden world-Z assumption fails.

#include "GSTN_CameraMath.h"
#include "GSTN_CameraTypes.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNCameraCoreTest
{
	constexpr EAutomationTestFlags CameraTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	const TArray<FVector>& Ups()
	{
		static const TArray<FVector> Values = {
			FVector(0, 0, 1), FVector(0, 0, -1), FVector(1, 0, 0), FVector(0, 1, 0), FVector(-1, 0, 0),
			FVector(1, 1, 1).GetSafeNormal(), FVector(0.3, -0.5, 0.8).GetSafeNormal(), FVector(-0.7, 0.2, -0.4).GetSafeNormal()
		};
		return Values;
	}

	/** A direction perpendicular to Up that is not the reference forward, to give tests a non-trivial heading */
	FVector SomeFlatDirection(const FVector& Up)
	{
		return FGSTN_CameraMath::HeadingToDirection(FGSTN_CameraMath::MakeFrame(Up, FVector::ZeroVector), 37.f);
	}
}

#define GSTN_CAM_TEST(Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNCameraCoreTest_##Name, "GSTNCamera.Core." #Name, GSTNCameraCoreTest::CameraTestFlags) \
	bool FGSTNCameraCoreTest_##Name::RunTest(const FString& Parameters)

#define GSTN_CAM_FOR_EACH_UP for (const FVector& U : GSTNCameraCoreTest::Ups())

#define CHECK(Cond) TestTrue(*FString::Printf(TEXT("%s [up %s]"), TEXT(#Cond), *U.ToString()), (Cond))

using namespace GSTNCameraCoreTest;

GSTN_CAM_TEST(BlendWeightsMonotonic)
{
	const EGSTN_CameraBlendFunction Functions[] = { EGSTN_CameraBlendFunction::Linear, EGSTN_CameraBlendFunction::EaseIn, EGSTN_CameraBlendFunction::EaseOut, EGSTN_CameraBlendFunction::EaseInOut };
	for (const EGSTN_CameraBlendFunction Function : Functions)
	{
		for (const float Exponent : { 1.f, 2.f, 3.5f })
		{
			float Previous = FGSTN_CameraMath::BlendAlpha(Function, 0.f, Exponent);
			TestEqual(TEXT("starts at 0"), Previous, 0.f);
			for (int32 Step = 1; Step <= 100; ++Step)
			{
				const float Weight = FGSTN_CameraMath::BlendAlpha(Function, Step / 100.f, Exponent);
				TestTrue(TEXT("monotonic"), Weight + 1.0e-6f >= Previous);
				Previous = Weight;
			}
			TestTrue(TEXT("ends at 1"), FMath::IsNearlyEqual(Previous, 1.f, 1.0e-5f));
		}
	}
	return true;
}

GSTN_CAM_TEST(BlendHeadingShortestArc)
{
	FGSTN_CameraView A;
	FGSTN_CameraView B;
	A.HeadingDeg = 170.f;
	B.HeadingDeg = -170.f;
	// Through 180, a 20 degree arc, not through 0
	const float Mid = FGSTN_CameraMath::BlendView(A, B, 0.5f).HeadingDeg;
	TestTrue(TEXT("half way is 180"), FMath::IsNearlyEqual(FMath::Abs(FGSTN_CameraMath::ShortestAngleDelta(Mid, 180.f)), 0.f, 1.0e-3f));
	TestTrue(TEXT("end reaches the target"), FMath::IsNearlyZero(FGSTN_CameraMath::ShortestAngleDelta(FGSTN_CameraMath::BlendView(A, B, 1.f).HeadingDeg, -170.f), 1.0e-3f));

	A.PitchDeg = -10.f;
	B.PitchDeg = -30.f;
	A.ArmCm = 100.f;
	B.ArmCm = 300.f;
	const FGSTN_CameraView Quarter = FGSTN_CameraMath::BlendView(A, B, 0.25f);
	TestTrue(TEXT("pitch linear"), FMath::IsNearlyEqual(Quarter.PitchDeg, -15.f));
	TestTrue(TEXT("arm linear"), FMath::IsNearlyEqual(Quarter.ArmCm, 150.f));
	return true;
}

GSTN_CAM_TEST(OrbitClampedAroundDesired)
{
	FGSTN_CameraOrbitLimits Limits;
	Limits.YawLimitDeg = 30.f;
	Limits.PitchMinOffsetDeg = -10.f;
	Limits.PitchMaxOffsetDeg = 5.f;
	Limits.DistanceScaleLimit = 2.f;
	FGSTN_CameraOrbitState Orbit;
	Orbit.YawDeg = 170.f; // desired is -170: the shortest offset is -20... orbit is at +340 degrees away, i.e. -20 the other way
	Orbit.PitchDeg = 40.f;
	Orbit.DistanceScale = 5.f;
	FGSTN_CameraOrbitState Clamped = FGSTN_CameraMath::ClampOrbitAroundDesired(Orbit, -170.f, -20.f, Limits);
	TestTrue(TEXT("inside the yaw limit keeps the yaw"), FMath::IsNearlyZero(FGSTN_CameraMath::ShortestAngleDelta(Clamped.YawDeg, 170.f), 1.0e-3f));
	TestTrue(TEXT("pitch clamped to the top offset"), FMath::IsNearlyEqual(Clamped.PitchDeg, -15.f));
	TestTrue(TEXT("zoom clamped"), FMath::IsNearlyEqual(Clamped.DistanceScale, 2.f));

	Orbit.YawDeg = 90.f;
	Orbit.PitchDeg = -80.f;
	Orbit.DistanceScale = 0.1f;
	Clamped = FGSTN_CameraMath::ClampOrbitAroundDesired(Orbit, 0.f, -20.f, Limits);
	TestTrue(TEXT("yaw clamped to the limit"), FMath::IsNearlyEqual(Clamped.YawDeg, 30.f));
	TestTrue(TEXT("pitch clamped to the bottom offset"), FMath::IsNearlyEqual(Clamped.PitchDeg, -30.f));
	TestTrue(TEXT("zoom clamped low"), FMath::IsNearlyEqual(Clamped.DistanceScale, 0.5f));
	return true;
}

GSTN_CAM_TEST(RecenterReachesDesired)
{
	FGSTN_CameraOrbitState Orbit;
	Orbit.YawDeg = 80.f;
	Orbit.PitchDeg = 10.f;
	Orbit.DistanceScale = 1.5f;
	// Before the delay nothing moves
	TestFalse(TEXT("not yet"), FGSTN_CameraMath::StepRecenter(Orbit, 0.f, -25.f, 0.5f, 1.f, 90.f, 0.1f));
	TestTrue(TEXT("unchanged"), FMath::IsNearlyEqual(Orbit.YawDeg, 80.f));
	// Never with a delay of 0
	FGSTN_CameraMath::StepRecenter(Orbit, 0.f, -25.f, 100.f, 0.f, 90.f, 0.1f);
	TestTrue(TEXT("delay 0 means never"), FMath::IsNearlyEqual(Orbit.YawDeg, 80.f));

	bool bReached = false;
	float PreviousError = 1.0e9f;
	for (int32 Frame = 0; Frame < 600 && !bReached; ++Frame)
	{
		bReached = FGSTN_CameraMath::StepRecenter(Orbit, 0.f, -25.f, 2.f, 1.f, 90.f, 1.f / 60.f);
		const float Error = FMath::Abs(Orbit.YawDeg) + FMath::Abs(Orbit.PitchDeg + 25.f);
		TestTrue(TEXT("error never grows"), Error <= PreviousError + 1.0e-4f);
		PreviousError = Error;
	}
	TestTrue(TEXT("reaches the desired angle"), bReached);
	TestTrue(TEXT("zoom back to 1"), FMath::IsNearlyEqual(Orbit.DistanceScale, 1.f, 0.002f));
	return true;
}

GSTN_CAM_TEST(UpRateLimited)
{
	GSTN_CAM_FOR_EACH_UP
	{
		const FVector Target = FVector(0.4, 0.5, -0.3).GetSafeNormal();
		FVector Current = U;
		const float Rate = 90.f;
		const float Dt = 1.f / 60.f;
		bool bArrived = false;
		for (int32 Frame = 0; Frame < 600; ++Frame)
		{
			const FVector Next = FGSTN_CameraMath::StepUp(Current, Target, Rate, Dt);
			const float Turned = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Current, Next), -1.0, 1.0)));
			CHECK(Turned <= Rate * Dt + 0.01f);
			CHECK(FMath::IsNearlyEqual(Next.Size(), 1.0, 1.0e-6));
			Current = Next;
			if (Current.Equals(Target, 1.0e-6))
			{
				bArrived = true;
				break;
			}
		}
		CHECK(bArrived);
	}
	return true;
}

GSTN_CAM_TEST(UpFlip180NeverSnaps)
{
	GSTN_CAM_FOR_EACH_UP
	{
		FVector Current = U;
		const FVector Target = -U;
		const float Rate = 90.f;
		const float Dt = 1.f / 30.f;
		float Total = 0.f;
		int32 Frames = 0;
		while (!Current.Equals(Target, 1.0e-5) && Frames < 300)
		{
			const FVector Next = FGSTN_CameraMath::StepUp(Current, Target, Rate, Dt);
			const float Turned = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Current, Next), -1.0, 1.0)));
			CHECK(Turned <= Rate * Dt + 0.01f);
			Total += Turned;
			Current = Next;
			++Frames;
		}
		CHECK(Current.Equals(Target, 1.0e-5));
		CHECK(Frames >= 60); // 180 degrees at 90 deg/s is two seconds, 60 steps at 30 Hz
		CHECK(FMath::IsNearlyEqual(Total, 180.f, 0.5f));
	}
	return true;
}

GSTN_CAM_TEST(TransportedForwardContinuous)
{
	GSTN_CAM_FOR_EACH_UP
	{
		FVector Up = U;
		FVector Forward = SomeFlatDirection(U);
		const FVector Target = FVector(-0.2, 0.9, 0.1).GetSafeNormal();
		for (int32 Frame = 0; Frame < 400; ++Frame)
		{
			const FVector NextUp = FGSTN_CameraMath::StepUp(Up, Target, 120.f, 1.f / 60.f);
			const FVector NextForward = FGSTN_CameraMath::TransportForward(Up, NextUp, Forward);
			CHECK(FMath::IsNearlyEqual(NextForward.Size(), 1.0, 1.0e-6));
			CHECK(FMath::IsNearlyZero(FVector::DotProduct(NextForward, NextUp), 1.0e-6));
			const float Turned = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Forward, NextForward), -1.0, 1.0)));
			CHECK(Turned <= 2.5f);
			Up = NextUp;
			Forward = NextForward;
		}
		// Carrying the forward keeps its heading relative to the frame stable: no hidden world Z
		const FGSTN_CameraFrame Frame = FGSTN_CameraMath::MakeFrame(Up, Forward);
		CHECK(FMath::IsNearlyZero(FGSTN_CameraMath::DirectionToHeading(Frame, Forward), 1.0e-3f));
	}
	return true;
}

GSTN_CAM_TEST(HeadingUnderSidewaysGravity)
{
	GSTN_CAM_FOR_EACH_UP
	{
		const FGSTN_CameraFrame Frame = FGSTN_CameraMath::MakeFrame(U, FVector::ZeroVector);
		CHECK(FMath::IsNearlyZero(FVector::DotProduct(Frame.RefForward, U), 1.0e-6));
		CHECK(FMath::IsNearlyEqual(Frame.RefForward.Size(), 1.0, 1.0e-6));
		// The deterministic reference: world X projected, world Y when degenerate
		CHECK(Frame.RefForward.Equals(FGSTN_CameraMath::ReferenceForward(U), 1.0e-6));
		for (const float Heading : { -170.f, -90.f, -10.f, 0.f, 33.f, 90.f, 179.f })
		{
			const FVector Direction = FGSTN_CameraMath::HeadingToDirection(Frame, Heading);
			CHECK(FMath::IsNearlyZero(FVector::DotProduct(Direction, U), 1.0e-6));
			CHECK(FMath::IsNearlyZero(FGSTN_CameraMath::ShortestAngleDelta(FGSTN_CameraMath::DirectionToHeading(Frame, Direction), Heading), 1.0e-3f));
		}
		// A view pose looks along heading and pitch: the elevation is measured from the plane normal to Up
		FGSTN_CameraView View;
		View.HeadingDeg = 50.f;
		View.PitchDeg = -25.f;
		View.ArmCm = 500.f;
		View.Pivot = FVector(10, 20, 30);
		const FGSTN_CameraPose Pose = FGSTN_CameraMath::ComputePose(Frame, View);
		CHECK(FMath::IsNearlyEqual(FGSTN_CameraMath::DirectionToPitch(U, Pose.Forward), -25.f, 1.0e-3f));
		CHECK(FMath::IsNearlyEqual(FGSTN_CameraMath::DirectionToHeading(Frame, Pose.Forward), 50.f, 1.0e-3f));
		CHECK(FMath::IsNearlyZero(FVector::DotProduct(Pose.Forward, Pose.Up), 1.0e-6));
		CHECK(FVector::DotProduct(Pose.Up, U) > 0.0);
		CHECK((View.Pivot - (Pose.Location + Pose.Forward * 500.f)).IsNearlyZero(1.0e-3));
		// Looking at the pivot from where the camera sits gives the same view back
		const FGSTN_CameraView Back = FGSTN_CameraMath::ViewLookingAt(Frame, Pose.Location, View.Pivot, 65.f);
		CHECK(FMath::IsNearlyEqual(Back.HeadingDeg, 50.f, 1.0e-2f) && FMath::IsNearlyEqual(Back.PitchDeg, -25.f, 1.0e-2f) && FMath::IsNearlyEqual(Back.ArmCm, 500.f, 1.0e-2f));
	}
	return true;
}

GSTN_CAM_TEST(SectionPriorityHighestWins)
{
	FGSTN_CameraSectionCandidate Low;
	Low.Id = 1;
	Low.Priority = 1;
	Low.EnterTime = 10.0;
	FGSTN_CameraSectionCandidate High;
	High.Id = 2;
	High.Priority = 5;
	High.EnterTime = 1.0; // entered first and still wins on priority
	FGSTN_CameraSectionCandidate Mid;
	Mid.Id = 3;
	Mid.Priority = 3;
	TestEqual(TEXT("none"), FGSTN_CameraMath::ChooseSection({}), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("highest priority"), FGSTN_CameraMath::ChooseSection({ Low, High, Mid }), 1);
	TestEqual(TEXT("order independent"), FGSTN_CameraMath::ChooseSection({ High, Mid, Low }), 0);
	return true;
}

GSTN_CAM_TEST(SectionTieEnteredLastThenSmaller)
{
	FGSTN_CameraSectionCandidate A;
	A.Id = 1;
	A.Priority = 2;
	A.EnterTime = 4.0;
	A.VolumeSize = 100.0;
	FGSTN_CameraSectionCandidate B = A;
	B.Id = 2;
	B.EnterTime = 6.0;
	B.VolumeSize = 900.0;
	TestEqual(TEXT("entered last wins"), FGSTN_CameraMath::ChooseSection({ A, B }), 1);
	B.EnterTime = A.EnterTime;
	TestEqual(TEXT("then the smaller volume"), FGSTN_CameraMath::ChooseSection({ A, B }), 0);
	TestEqual(TEXT("then the smaller volume, other order"), FGSTN_CameraMath::ChooseSection({ B, A }), 1);
	B.VolumeSize = A.VolumeSize;
	TestEqual(TEXT("then the lower id"), FGSTN_CameraMath::ChooseSection({ B, A }), 1);
	return true;
}

GSTN_CAM_TEST(RuleHighestPriorityWins)
{
	FGSTN_CameraRuleChoiceState State;
	FGSTN_CameraRuleEval Follow;
	Follow.Priority = 0;
	Follow.bMatches = true;
	FGSTN_CameraRuleEval Section;
	Section.Priority = 50;
	Section.bMatches = true;
	FGSTN_CameraRuleEval Aim;
	Aim.Priority = 100;
	Aim.bMatches = false;
	TArray<FGSTN_CameraRuleEval> Rules = { Aim, Section, Follow };
	TestEqual(TEXT("first frame picks the best match"), FGSTN_CameraMath::StepRuleChoice(State, Rules, 0.016f, 0.3f), 1);
	// A rule that appears with no enter delay wins once the minimum dwell passed
	Rules[0].bMatches = true;
	int32 Chosen = FGSTN_CameraMath::StepRuleChoice(State, Rules, 0.016f, 0.3f);
	TestEqual(TEXT("dwell protects a fresh choice"), Chosen, 1);
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		Chosen = FGSTN_CameraMath::StepRuleChoice(State, Rules, 0.016f, 0.3f);
	}
	TestEqual(TEXT("then the higher priority wins"), Chosen, 0);

	// Ties go to table order
	FGSTN_CameraRuleChoiceState TieState;
	FGSTN_CameraRuleEval First;
	First.bMatches = true;
	FGSTN_CameraRuleEval Second = First;
	TestEqual(TEXT("tie to table order"), FGSTN_CameraMath::StepRuleChoice(TieState, { First, Second }, 0.016f, 0.f), 0);

	// Nothing ever matches
	FGSTN_CameraRuleChoiceState Empty;
	TestEqual(TEXT("no rule"), FGSTN_CameraMath::StepRuleChoice(Empty, { Aim }, 0.016f, 0.f), static_cast<int32>(INDEX_NONE));
	return true;
}

GSTN_CAM_TEST(RuleExitDelayDebounces)
{
	FGSTN_CameraRuleEval Aim;
	Aim.Priority = 100;
	Aim.bMatches = true;
	Aim.ExitDelaySeconds = 0.15f;
	FGSTN_CameraRuleEval Follow;
	Follow.Priority = 0;
	Follow.bMatches = true;
	TArray<FGSTN_CameraRuleEval> Rules = { Aim, Follow };
	FGSTN_CameraRuleChoiceState State;
	const float Dt = 1.f / 60.f;
	int32 Chosen = FGSTN_CameraMath::StepRuleChoice(State, Rules, Dt, 0.3f);
	TestEqual(TEXT("starts on aim"), Chosen, 0);
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Chosen = FGSTN_CameraMath::StepRuleChoice(State, Rules, Dt, 0.3f);
	}
	// A two frame drop (a predicted grant corrected and granted again) does not leave Shoulder
	Rules[0].bMatches = false;
	Chosen = FGSTN_CameraMath::StepRuleChoice(State, Rules, Dt, 0.3f);
	Chosen = FGSTN_CameraMath::StepRuleChoice(State, Rules, Dt, 0.3f);
	TestEqual(TEXT("still aim during the delay"), Chosen, 0);
	Rules[0].bMatches = true;
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		Chosen = FGSTN_CameraMath::StepRuleChoice(State, Rules, Dt, 0.3f);
	}
	TestEqual(TEXT("flicker never left aim"), Chosen, 0);

	// A real end leaves after the exit delay, not before
	Rules[0].bMatches = false;
	int32 FramesToLeave = 0;
	for (; FramesToLeave < 120 && Chosen == 0; ++FramesToLeave)
	{
		Chosen = FGSTN_CameraMath::StepRuleChoice(State, Rules, Dt, 0.3f);
	}
	TestEqual(TEXT("leaves for follow"), Chosen, 1);
	TestTrue(TEXT("not before the delay"), FramesToLeave * Dt >= 0.15f - 1.0e-4f);
	TestTrue(TEXT("and soon after"), FramesToLeave * Dt <= 0.15f + 2.f * Dt);
	return true;
}

GSTN_CAM_TEST(ReversedBlendKeepsWeight)
{
	FGSTN_CameraBlendTuning Blend;
	Blend.BlendSeconds = 1.f;
	Blend.Function = EGSTN_CameraBlendFunction::Linear;
	FGSTN_CameraModeStack Stack;
	Stack.Request(1, Blend, 4);
	TestEqual(TEXT("first entry is complete"), Stack.WeightOf(0), 1.f);
	Stack.Request(2, Blend, 4);
	TestEqual(TEXT("two entries"), Stack.Num(), 2);
	Stack.Advance(0.4f);
	TestTrue(TEXT("new entry at 0.4"), FMath::IsNearlyEqual(Stack.WeightOf(1), 0.4f, 1.0e-4f));
	// Reverse: the bottom entry is requested again; the one on top keeps its progress and the stack keeps both
	Stack.Request(1, Blend, 4);
	TestEqual(TEXT("promoted, not duplicated"), Stack.Num(), 2);
	TestEqual(TEXT("1 on top"), Stack.TopKey(), 1);
	TestEqual(TEXT("2 below"), Stack.Entries[0].Key, 2);
	// Entry 1 was at progress 1.0 when it was the bottom; it keeps that, so it completes at once and drops 2
	Stack.Advance(0.016f);
	TestEqual(TEXT("a complete entry covers the rest"), Stack.Num(), 1);

	// A genuinely partial one: A on top at 0.3, B requested (new), then A requested again keeps 0.3
	FGSTN_CameraModeStack Partial;
	Partial.Request(1, Blend, 4);
	Partial.Request(2, Blend, 4);
	Partial.Advance(0.3f);
	Partial.Request(3, Blend, 4);
	Partial.Advance(0.2f);
	TestEqual(TEXT("three entries"), Partial.Num(), 3);
	Partial.Request(2, Blend, 4);
	TestEqual(TEXT("2 promoted"), Partial.TopKey(), 2);
	TestTrue(TEXT("keeps its 0.3 progress"), FMath::IsNearlyEqual(Partial.Entries.Last().Progress, 0.3f, 1.0e-4f));
	TArray<int32> Removed;
	FGSTN_CameraModeStack Deep;
	for (int32 Key = 1; Key <= 6; ++Key)
	{
		Deep.Request(Key, Blend, 4, &Removed);
	}
	TestEqual(TEXT("depth is capped"), Deep.Num(), 4);
	TestEqual(TEXT("bottom entries dropped"), Removed.Num(), 2);
	return true;
}

GSTN_CAM_TEST(RailProjectsToNearest)
{
	GSTN_CAM_FOR_EACH_UP
	{
		const FVector A = SomeFlatDirection(U);
		const FVector B = FVector::CrossProduct(U, A).GetSafeNormal();
		// An L shaped rail in the plane normal to Up: 1000 along A, then 500 along B
		const FVector Origin = FVector(100, 200, 300);
		const TArray<FVector> Rail = { Origin, Origin + A * 1000.0, Origin + A * 1000.0 + B * 500.0 };
		FVector Nearest;
		float Along = 0.f;
		int32 Segment = -1;
		CHECK(FGSTN_CameraMath::ProjectOnPolyline(Rail, Origin + A * 400.0 + U * 700.0 - B * 50.0, Nearest, Along, &Segment));
		CHECK(Nearest.Equals(Origin + A * 400.0, 1.0e-3));
		CHECK(FMath::IsNearlyEqual(Along, 400.f, 1.0e-2f) && Segment == 0);
		CHECK(FGSTN_CameraMath::ProjectOnPolyline(Rail, Origin + A * 1300.0 + B * 200.0, Nearest, Along, &Segment));
		CHECK(Nearest.Equals(Origin + A * 1000.0 + B * 200.0, 1.0e-3));
		CHECK(FMath::IsNearlyEqual(Along, 1200.f, 1.0e-2f) && Segment == 1);
		// Past the end clamps to the end
		CHECK(FGSTN_CameraMath::ProjectOnPolyline(Rail, Origin + A * 1000.0 + B * 900.0, Nearest, Along));
		CHECK(Nearest.Equals(Rail.Last(), 1.0e-3));
		CHECK(!FGSTN_CameraMath::ProjectOnPolyline({ Origin }, Origin, Nearest, Along));
	}
	return true;
}

GSTN_CAM_TEST(ConvergedAimBeyondMaxUsesForward)
{
	GSTN_CAM_FOR_EACH_UP
	{
		const FVector Forward = SomeFlatDirection(U);
		const FVector Side = FVector::CrossProduct(U, Forward).GetSafeNormal();
		const FVector ViewOrigin = FVector(0, 0, 0);
		const FVector AimOrigin = Side * 55.0 + U * 20.0;
		const FVector NoHit = FGSTN_CameraMath::ConvergedAimDirection(ViewOrigin, Forward, AimOrigin, false, FVector::ZeroVector, 300.f, 6000.f);
		CHECK(NoHit.Equals(Forward, 1.0e-6));
		const FVector Far = FGSTN_CameraMath::ConvergedAimDirection(ViewOrigin, Forward, AimOrigin, true, Forward * 7000.0, 300.f, 6000.f);
		CHECK(Far.Equals(Forward, 1.0e-6));
		// A hit inside the range converges: the shot from the aim origin passes through the hit point
		const FVector Hit = Forward * 2000.0;
		const FVector Near = FGSTN_CameraMath::ConvergedAimDirection(ViewOrigin, Forward, AimOrigin, true, Hit, 300.f, 6000.f);
		CHECK(FMath::IsNearlyEqual(Near.Size(), 1.0, 1.0e-6));
		CHECK(FVector::Distance(AimOrigin + Near * FVector::Distance(AimOrigin, Hit), Hit) < 1.0e-2);
	}
	return true;
}

GSTN_CAM_TEST(ConvergedAimMinDistance)
{
	GSTN_CAM_FOR_EACH_UP
	{
		const FVector Forward = SomeFlatDirection(U);
		const FVector Side = FVector::CrossProduct(U, Forward).GetSafeNormal();
		const FVector AimOrigin = Side * 55.0;
		// A hit 100 cm away (inside the minimum of 300) is taken at 300 along the view axis
		const FVector Direction = FGSTN_CameraMath::ConvergedAimDirection(FVector::ZeroVector, Forward, AimOrigin, true, Forward * 100.0, 300.f, 6000.f);
		const FVector Expected = (Forward * 300.0 - AimOrigin).GetSafeNormal();
		CHECK(Direction.Equals(Expected, 1.0e-6));
		CHECK(FVector::DotProduct(Direction, Forward) > 0.9);
		// Just inside the maximum still converges
		const FVector AtMax = FGSTN_CameraMath::ConvergedAimDirection(FVector::ZeroVector, Forward, AimOrigin, true, Forward * 5990.0, 300.f, 6000.f);
		CHECK(AtMax.Equals((Forward * 5990.0 - AimOrigin).GetSafeNormal(), 1.0e-6));
	}
	return true;
}

GSTN_CAM_TEST(FrameChangeIgnoresManualOrbit)
{
	GSTN_CAM_FOR_EACH_UP
	{
		const FGSTN_CameraFrame Frame = FGSTN_CameraMath::MakeFrame(U, FVector::ZeroVector);
		const FVector From = FGSTN_CameraMath::HeadingToDirection(Frame, 10.f);
		const FVector Far = FGSTN_CameraMath::HeadingToDirection(Frame, 100.f);
		const FVector Near = FGSTN_CameraMath::HeadingToDirection(Frame, 25.f);
		float Delta = 0.f;
		CHECK(FGSTN_CameraMath::ShouldRaiseFrameChange(U, From, Far, 20.f, false, &Delta));
		CHECK(FMath::IsNearlyEqual(Delta, 90.f, 1.0e-2f));
		CHECK(!FGSTN_CameraMath::ShouldRaiseFrameChange(U, From, Far, 20.f, true));
		CHECK(!FGSTN_CameraMath::ShouldRaiseFrameChange(U, From, Near, 20.f, false));
		// A turn of the pitch alone is not a change of the reference
		CHECK(!FGSTN_CameraMath::ShouldRaiseFrameChange(U, From, From + U * 3.0, 20.f, false));
	}
	return true;
}

GSTN_CAM_TEST(FreeCameraGroundHeight)
{
	float Ground = 0.f;
	Ground = FGSTN_CameraMath::StepGroundHeight(Ground, 0.f, true, 250.f, 50.f);
	// A jump inside the dead zone leaves the ground height alone
	TestEqual(TEXT("jump arc ignored"), FGSTN_CameraMath::StepGroundHeight(Ground, 200.f, false, 250.f, 50.f), 0.f);
	// Above the dead zone the ground follows at the dead zone distance
	TestEqual(TEXT("high jump drags"), FGSTN_CameraMath::StepGroundHeight(Ground, 400.f, false, 250.f, 50.f), 150.f);
	// A drop follows at once
	TestEqual(TEXT("drop follows"), FGSTN_CameraMath::StepGroundHeight(Ground, -300.f, false, 250.f, 50.f), -250.f);
	// Landing on a higher platform snaps
	TestEqual(TEXT("landing on a higher platform"), FGSTN_CameraMath::StepGroundHeight(Ground, 500.f, true, 250.f, 50.f), 500.f);
	return true;
}

GSTN_CAM_TEST(DuelFramingKeepsFightersParallelToScreen)
{
	GSTN_CAM_FOR_EACH_UP
	{
		const FVector Axis = SomeFlatDirection(U);
		const FVector A = FVector(5, 6, 7);
		const FVector B = A + Axis * 600.0 + U * 120.0;
		FVector Direction;
		CHECK(FGSTN_CameraMath::DuelDirection(A, B, U, FVector::ZeroVector, true, 30.f, Direction));
		CHECK(FMath::IsNearlyZero(FVector::DotProduct(Direction, Axis), 1.0e-6));
		CHECK(FMath::IsNearlyZero(FVector::DotProduct(Direction, U), 1.0e-6));
		// A is on the left of the screen: the camera right (Up x Direction) points from A to B
		CHECK(FVector::DotProduct(FVector::CrossProduct(U, Direction), B - A) > 0.0);
		FVector Other;
		CHECK(FGSTN_CameraMath::DuelDirection(A, B, U, FVector::ZeroVector, false, 30.f, Other));
		CHECK(Other.Equals(-Direction, 1.0e-6));
		// The side it is on is kept when the fighters swap places: no flip across them
		FVector Kept;
		CHECK(FGSTN_CameraMath::DuelDirection(B, A, U, Direction, true, 30.f, Kept));
		CHECK(Kept.Equals(Direction, 1.0e-6));
		// A separation under the minimum keeps the last heading
		FVector Degenerate;
		CHECK(!FGSTN_CameraMath::DuelDirection(A, A + U * 400.0 + Axis * 5.0, U, Direction, true, 30.f, Degenerate));
		CHECK(Degenerate.Equals(Direction, 1.0e-6));
	}
	const float Narrow = FGSTN_CameraMath::DuelArm(200.f, 100.f, 250.f, 55.f, 16.f / 9.f, 450.f, 1800.f);
	const float Wide = FGSTN_CameraMath::DuelArm(1500.f, 100.f, 250.f, 55.f, 16.f / 9.f, 450.f, 1800.f);
	const float Huge = FGSTN_CameraMath::DuelArm(90000.f, 100.f, 250.f, 55.f, 16.f / 9.f, 450.f, 1800.f);
	TestTrue(TEXT("arm grows with the separation"), Wide > Narrow);
	TestTrue(TEXT("arm is clamped"), Narrow >= 450.f && FMath::IsNearlyEqual(Huge, 1800.f));
	return true;
}

GSTN_CAM_TEST(OcclusionDecision)
{
	using A = EGSTN_CameraOcclusionAction;
	using P = EGSTN_CameraOcclusionPolicy;
	TestEqual(TEXT("pull in always pulls"), FGSTN_CameraMath::DecideOcclusion(P::PullIn, true, 900.f, 50.f), A::PullIn);
	TestEqual(TEXT("fade fades the fadeable"), FGSTN_CameraMath::DecideOcclusion(P::Fade, true, 900.f, 50.f), A::Fade);
	TestEqual(TEXT("fade leaves the rest"), FGSTN_CameraMath::DecideOcclusion(P::Fade, false, 10.f, 50.f), A::None);
	TestEqual(TEXT("fallback fades first"), FGSTN_CameraMath::DecideOcclusion(P::FadeThenPullIn, true, 10.f, 50.f), A::Fade);
	TestEqual(TEXT("fallback pulls in a near hard blocker"), FGSTN_CameraMath::DecideOcclusion(P::FadeThenPullIn, false, 10.f, 50.f), A::PullIn);
	TestEqual(TEXT("fallback ignores a far hard blocker"), FGSTN_CameraMath::DecideOcclusion(P::FadeThenPullIn, false, 400.f, 50.f), A::None);
	return true;
}

#undef CHECK

#endif // WITH_DEV_AUTOMATION_TESTS
