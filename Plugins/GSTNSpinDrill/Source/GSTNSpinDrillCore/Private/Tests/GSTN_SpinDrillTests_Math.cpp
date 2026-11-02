// Automation tests of the spin and drill plugin: data and pure math (plan D2).

#include "Curves/CurveFloat.h"
#include "Data/GSTN_DrillTargetResponse.h"
#include "Data/GSTN_SpinDrillTypes.h"
#include "Math/GSTN_DrillMath.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNSpinDrillTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Angle between two vectors in degrees, in double so a 0.01 degree bound is measurable */
	double AngleDegrees(const FVector& A, const FVector& B)
	{
		const double Cosine = FMath::Clamp(FVector::DotProduct(A.GetSafeNormal(), B.GetSafeNormal()), -1.0, 1.0);
		return FMath::RadiansToDegrees(FMath::Acos(Cosine));
	}

	FGSTN_DrillEndInputs Latched(int32 FramesBored = 10, int32 MaxBoreFrames = 90)
	{
		FGSTN_DrillEndInputs Inputs;
		Inputs.bCrouchHeld = true;
		Inputs.FramesBored = FramesBored;
		Inputs.MaxBoreFrames = MaxBoreFrames;
		return Inputs;
	}
}

using namespace GSTNSpinDrillTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillMathAnchorTest, "GSTNSpinDrill.Core.Math.AnchorRoundTrip", GSTNSpinDrillTest::Flags)
bool FGSTN_DrillMathAnchorTest::RunTest(const FString& Parameters)
{
	FRandomStream Random(1234);
	for (int32 Index = 0; Index < 2000; ++Index)
	{
		const FVector Point(Random.FRandRange(-30000.f, 30000.f), Random.FRandRange(-30000.f, 30000.f), Random.FRandRange(-30000.f, 30000.f));
		FGSTN_DrillAnchorQ Anchor;
		const bool bInRange = UGSTN_DrillMath::QuantizeAnchor(Point, Anchor);
		const FVector Back = UGSTN_DrillMath::DequantizeAnchor(Anchor);
		if (!TestTrue(TEXT("in range"), bInRange)
			|| !TestTrue(*FString::Printf(TEXT("%s within 0.5 cm per axis (got %s)"), *Point.ToString(), *Back.ToString()),
				FMath::Abs(Back.X - Point.X) <= 0.5 && FMath::Abs(Back.Y - Point.Y) <= 0.5 && FMath::Abs(Back.Z - Point.Z) <= 0.5))
		{
			break;
		}
		FGSTN_DrillAnchorQ Again;
		UGSTN_DrillMath::QuantizeAnchor(Back, Again);
		if (!TestTrue(TEXT("a decoded anchor encodes to the same bits"), Again == Anchor))
		{
			break;
		}
	}

	// Rounding to nearest, half away from minus infinity
	FGSTN_DrillAnchorQ Anchor;
	UGSTN_DrillMath::QuantizeAnchor(FVector(0.49, -0.49, 0.5), Anchor);
	TestEqual(TEXT("0.49 -> 0"), static_cast<int32>(Anchor.X), 0);
	TestEqual(TEXT("-0.49 -> 0"), static_cast<int32>(Anchor.Y), 0);
	TestEqual(TEXT("0.5 -> 1"), static_cast<int32>(Anchor.Z), 1);

	// The range edges and beyond: clamped, reported
	TestTrue(TEXT("327.67 m fits"), UGSTN_DrillMath::QuantizeAnchor(FVector(32767.0, -32768.0, 0.0), Anchor));
	TestEqual(TEXT("max"), static_cast<int32>(Anchor.X), 32767);
	TestEqual(TEXT("min"), static_cast<int32>(Anchor.Y), -32768);
	TestFalse(TEXT("400 m does not fit"), UGSTN_DrillMath::QuantizeAnchor(FVector(40000.0, 0.0, -40000.0), Anchor));
	TestEqual(TEXT("clamped high"), static_cast<int32>(Anchor.X), 32767);
	TestEqual(TEXT("clamped low"), static_cast<int32>(Anchor.Z), -32768);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillMathAxisTest, "GSTNSpinDrill.Core.Math.AxisRoundTrip", GSTNSpinDrillTest::Flags)
bool FGSTN_DrillMathAxisTest::RunTest(const FString& Parameters)
{
	// Poles, axes and the octant diagonals, where the octahedral fold has its edges
	const TArray<FVector> Special = {
		FVector(0, 0, 1), FVector(0, 0, -1), FVector(1, 0, 0), FVector(-1, 0, 0), FVector(0, 1, 0), FVector(0, -1, 0),
		FVector(1, 1, 1), FVector(-1, -1, -1), FVector(1, -1, -1), FVector(-1, 1, -1), FVector(1, 1, 0), FVector(0.001, 0, -1) };
	for (const FVector& Axis : Special)
	{
		const FVector Back = UGSTN_DrillMath::DecodeAxis(UGSTN_DrillMath::EncodeAxis(Axis));
		TestTrue(*FString::Printf(TEXT("%s round trip within 0.01 deg (%.5f)"), *Axis.ToString(), AngleDegrees(Axis, Back)), AngleDegrees(Axis, Back) < 0.01);
		TestTrue(*FString::Printf(TEXT("%s decodes to a unit vector"), *Axis.ToString()), FMath::IsNearlyEqual(Back.Size(), 1.0, 1e-9));
	}

	const FGSTN_DrillAxisQ Up = UGSTN_DrillMath::EncodeAxis(FVector(0, 0, 1));
	const FGSTN_DrillAxisQ Down = UGSTN_DrillMath::EncodeAxis(FVector(0, 0, -1));
	TestTrue(TEXT("+Z encodes to (0, 0)"), Up.X == 0 && Up.Y == 0);
	TestTrue(TEXT("-Z encodes to a corner"), Down.X == 32767 && Down.Y == 32767);
	TestTrue(TEXT("-Z decodes exactly"), UGSTN_DrillMath::DecodeAxis(Down).Equals(FVector(0, 0, -1), 1e-12));

	// Length does not matter; a zero vector is +Z
	TestTrue(TEXT("length 5 encodes as length 1"), UGSTN_DrillMath::EncodeAxis(FVector(0, 3, -4)) == UGSTN_DrillMath::EncodeAxis(FVector(0, 0.6, -0.8)));
	TestTrue(TEXT("zero encodes +Z"), UGSTN_DrillMath::EncodeAxis(FVector::ZeroVector) == Up);

	FRandomStream Random(42);
	double Worst = 0.0;
	for (int32 Index = 0; Index < 5000; ++Index)
	{
		const FVector Axis = Random.GetUnitVector();
		const FGSTN_DrillAxisQ Encoded = UGSTN_DrillMath::EncodeAxis(Axis);
		const FVector Back = UGSTN_DrillMath::DecodeAxis(Encoded);
		Worst = FMath::Max(Worst, AngleDegrees(Axis, Back));
		if (!TestTrue(TEXT("a decoded axis encodes to the same bits"), UGSTN_DrillMath::EncodeAxis(Back) == Encoded))
		{
			break;
		}
	}
	TestTrue(*FString::Printf(TEXT("worst of 5000 random axes below 0.01 deg (%.5f)"), Worst), Worst < 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillMathWobbleTest, "GSTNSpinDrill.Core.Math.Wobble", GSTNSpinDrillTest::Flags)
bool FGSTN_DrillMathWobbleTest::RunTest(const FString& Parameters)
{
	const TArray<float> NoSamples;
	const auto Amplitude = [](int32 Frames, const TArray<float>& Samples, int32 Seat = 12)
	{
		return UGSTN_DrillMath::ComputeWobble(Frames, Seat, 12.f, 3.f, 60.f, Samples).AmplitudeDegrees;
	};

	// Default: linear decay from the full amplitude to 0 at SeatFrames, never negative, never increasing
	TestEqual(TEXT("full at the latch"), Amplitude(0, NoSamples), 12.f);
	TestEqual(TEXT("half at half the seat"), Amplitude(6, NoSamples), 6.f);
	TestEqual(TEXT("0 at SeatFrames"), Amplitude(12, NoSamples), 0.f);
	TestEqual(TEXT("0 after the seat"), Amplitude(40, NoSamples), 0.f);
	TestEqual(TEXT("0 with no seat"), Amplitude(0, NoSamples, 0), 0.f);
	TestEqual(TEXT("negative frames read as the latch"), Amplitude(-3, NoSamples), 12.f);
	float Previous = TNumericLimits<float>::Max();
	for (int32 Frame = 0; Frame <= 14; ++Frame)
	{
		const float Value = Amplitude(Frame, NoSamples);
		TestTrue(*FString::Printf(TEXT("frame %d does not grow"), Frame), Value <= Previous && Value >= 0.f);
		Previous = Value;
	}

	// Samples: read linearly; a curve that never decays is still cut to 0 at SeatFrames
	const TArray<float> Flat = { 1.f, 1.f, 1.f };
	TestEqual(TEXT("flat curve before the end"), Amplitude(11, Flat), 12.f);
	TestEqual(TEXT("flat curve cut at SeatFrames"), Amplitude(12, Flat), 0.f);
	const TArray<float> Shaped = { 1.f, 0.2f, 0.f };
	TestTrue(TEXT("between samples: a quarter of the seat is 0.6"), FMath::IsNearlyEqual(Amplitude(3, Shaped), 12.f * 0.6f, 1e-4f));
	TestEqual(TEXT("values above 1 are clamped"), Amplitude(0, TArray<float>{ 3.f, 3.f }), 12.f);

	// Phase: 3 Hz at a 60 Hz tick is 20 frames a turn
	TestTrue(TEXT("phase 0 at the latch"), FMath::IsNearlyEqual(UGSTN_DrillMath::ComputeWobble(0, 12, 12.f, 3.f, 60.f, NoSamples).PhaseDegrees, 0.f, 1e-3f));
	TestTrue(TEXT("quarter turn at frame 5"), FMath::IsNearlyEqual(UGSTN_DrillMath::ComputeWobble(5, 12, 12.f, 3.f, 60.f, NoSamples).PhaseDegrees, 90.f, 1e-3f));
	TestTrue(TEXT("wraps at frame 25"), FMath::IsNearlyEqual(UGSTN_DrillMath::ComputeWobble(25, 12, 12.f, 3.f, 60.f, NoSamples).PhaseDegrees, 90.f, 1e-3f));
	TestEqual(TEXT("no tick rate, no phase"), UGSTN_DrillMath::ComputeWobble(5, 12, 12.f, 3.f, 0.f, NoSamples).PhaseDegrees, 0.f);

	// Curve sampling helper (outside the simulation)
	TStrongObjectPtr<UCurveFloat> Curve(NewObject<UCurveFloat>(GetTransientPackage()));
	Curve->FloatCurve.AddKey(0.f, 1.f);
	Curve->FloatCurve.AddKey(1.f, 0.f);
	TArray<float> Samples;
	UGSTN_DrillMath::SampleWobbleCurve(Curve.Get(), 5, Samples);
	if (TestEqual(TEXT("five samples"), Samples.Num(), 5))
	{
		TestTrue(TEXT("samples follow the curve"), FMath::IsNearlyEqual(Samples[0], 1.f) && FMath::IsNearlyEqual(Samples[2], 0.5f, 1e-4f)
			&& FMath::IsNearlyEqual(Samples[4], 0.f));
	}
	UGSTN_DrillMath::SampleWobbleCurve(nullptr, 5, Samples);
	TestEqual(TEXT("no curve, no samples"), Samples.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillMathLaunchTest, "GSTNSpinDrill.Core.Math.LaunchAndDive", GSTNSpinDrillTest::Flags)
bool FGSTN_DrillMathLaunchTest::RunTest(const FString& Parameters)
{
	using EDir = EGSTN_DrillLaunchDirection;
	const FVector EarthGravity(0, 0, -980);

	TestTrue(TEXT("normal up"), UGSTN_DrillMath::ComputeLaunchVelocity(EDir::SurfaceNormal, FVector(0, 0, 5), EarthGravity, 1000.f).Equals(FVector(0, 0, 1000), 1e-6));
	TestTrue(TEXT("wall normal"), UGSTN_DrillMath::ComputeLaunchVelocity(EDir::SurfaceNormal, FVector(-1, 0, 0), EarthGravity, 1000.f).Equals(FVector(-1000, 0, 0), 1e-6));
	TestTrue(TEXT("gravity up, earth"), UGSTN_DrillMath::ComputeLaunchVelocity(EDir::GravityUp, FVector(1, 0, 0), EarthGravity, 1000.f).Equals(FVector(0, 0, 1000), 1e-6));

	// Tilted and inverted gravity: always against it
	const FVector Tilted = UGSTN_DrillMath::ComputeLaunchVelocity(EDir::GravityUp, FVector::ZeroVector, FVector(980, 0, -980), 1000.f);
	TestTrue(TEXT("tilted gravity"), Tilted.Equals(FVector(-1, 0, 1).GetSafeNormal() * 1000.0, 1e-6));
	TestTrue(TEXT("inverted gravity"), UGSTN_DrillMath::ComputeLaunchVelocity(EDir::GravityUp, FVector(0, 0, 1), FVector(0, 0, 980), 1000.f).Equals(FVector(0, 0, -1000), 1e-6));

	// Fallbacks
	TestTrue(TEXT("zero normal falls back to gravity up"), UGSTN_DrillMath::ComputeLaunchVelocity(EDir::SurfaceNormal, FVector::ZeroVector, FVector(0, 0, 980), 500.f).Equals(FVector(0, 0, -500), 1e-6));
	TestTrue(TEXT("zero gravity falls back to the normal"), UGSTN_DrillMath::ComputeLaunchVelocity(EDir::GravityUp, FVector(0, 1, 0), FVector::ZeroVector, 500.f).Equals(FVector(0, 500, 0), 1e-6));
	TestTrue(TEXT("both zero gives zero"), UGSTN_DrillMath::ComputeLaunchVelocity(EDir::GravityUp, FVector::ZeroVector, FVector::ZeroVector, 500.f).IsZero());

	// Dive: lateral steering only, capped, along any gravity
	const FVector Down(0, 0, -1);
	TestTrue(TEXT("no steer: straight down"), UGSTN_DrillMath::ComputeDiveVelocity(Down, FVector::ZeroVector, 1800.f, 0.15f).Equals(FVector(0, 0, -1800), 1e-6));
	TestTrue(TEXT("steer along the dive is ignored"), UGSTN_DrillMath::ComputeDiveVelocity(Down, FVector(0, 0, 1), 1800.f, 0.15f).Equals(FVector(0, 0, -1800), 1e-6));
	TestTrue(TEXT("full side steer"), UGSTN_DrillMath::ComputeDiveVelocity(Down, FVector(1, 0, 0), 1800.f, 0.15f).Equals(FVector(270, 0, -1800), 1e-6));
	TestTrue(TEXT("steer is capped at full stick"), UGSTN_DrillMath::ComputeDiveVelocity(Down, FVector(3, 0, 0), 1800.f, 0.15f).Equals(FVector(270, 0, -1800), 1e-6));
	const FVector TiltedDive = FVector(1, 0, -1).GetSafeNormal();
	const FVector Steered = UGSTN_DrillMath::ComputeDiveVelocity(TiltedDive, FVector(1, 1, 0), 1800.f, 0.15f);
	TestTrue(TEXT("tilted dive keeps its speed along the dive"), FMath::IsNearlyEqual(FVector::DotProduct(Steered, TiltedDive), 1800.0, 1e-6));
	TestTrue(TEXT("no dive direction, no velocity"), UGSTN_DrillMath::ComputeDiveVelocity(FVector::ZeroVector, FVector(1, 0, 0), 1800.f, 0.15f).IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillMathEndTest, "GSTNSpinDrill.Core.Math.EndConditions", GSTNSpinDrillTest::Flags)
bool FGSTN_DrillMathEndTest::RunTest(const FString& Parameters)
{
	using EEnd = EGSTN_DrillEndResult;
	const auto Expect = [this](const TCHAR* What, const FGSTN_DrillEndInputs& Inputs, EEnd Expected)
	{
		TestEqual(What, static_cast<int32>(UGSTN_DrillMath::ResolveEnd(Inputs)), static_cast<int32>(Expected));
	};

	Expect(TEXT("held, inside the time: continue"), Latched(), EEnd::Continue);
	Expect(TEXT("one frame before the maximum: continue"), Latched(89, 90), EEnd::Continue);

	FGSTN_DrillEndInputs Inputs = Latched();
	Inputs.bCrouchHeld = false;
	Expect(TEXT("early release: hop out"), Inputs, EEnd::DetachPop);
	Inputs.bJumpBoost = true;
	Expect(TEXT("early release with a jump: high jump"), Inputs, EEnd::Launch);

	Inputs = Latched(90, 90);
	Expect(TEXT("maximum without a jump: hop out"), Inputs, EEnd::DetachPop);
	Inputs.bJumpBoost = true;
	Expect(TEXT("maximum with a jump: high jump"), Inputs, EEnd::Launch);

	Inputs = Latched();
	Inputs.bTargetGone = true;
	Expect(TEXT("target gone: final jump"), Inputs, EEnd::Launch);
	Inputs.bContinueThroughDestroyed = true;
	Expect(TEXT("target gone, keep plunging"), Inputs, EEnd::KeepPlunging);
	Inputs = Latched();
	Inputs.bTargetNotDrillable = true;
	Expect(TEXT("target no longer drillable: final jump"), Inputs, EEnd::Launch);

	// Priority: hitstun and KO beat everything; target loss beats release and maximum
	Inputs = Latched(200, 90);
	Inputs.bCrouchHeld = false;
	Inputs.bJumpBoost = true;
	Inputs.bTargetGone = true;
	Inputs.bDrillerInHitstun = true;
	Expect(TEXT("hitstun wins over everything"), Inputs, EEnd::DetachHitstun);
	Inputs.bDrillerInHitstun = false;
	Inputs.bDrillerKnockedOut = true;
	Expect(TEXT("KO wins over everything"), Inputs, EEnd::DetachHitstun);
	Inputs.bDrillerKnockedOut = false;
	Inputs.bJumpBoost = false;
	Expect(TEXT("target loss wins over release and maximum"), Inputs, EEnd::Launch);

	// Effective numbers
	TestEqual(TEXT("override wins"), UGSTN_DrillMath::ResolveMaxBoreFrames(90, 40), 40);
	TestEqual(TEXT("no override"), UGSTN_DrillMath::ResolveMaxBoreFrames(90, 0), 90);
	TestEqual(TEXT("never below 1"), UGSTN_DrillMath::ResolveMaxBoreFrames(0, 0), 1);
	TestEqual(TEXT("seat scaled"), UGSTN_DrillMath::ResolveSeatFrames(12, 1.5f), 18);
	TestEqual(TEXT("seat never negative"), UGSTN_DrillMath::ResolveSeatFrames(12, -1.f), 0);

	// Jump boost window (13b): held, or pressed at most WindowFrames before the exit frame
	TestTrue(TEXT("held"), UGSTN_DrillMath::IsJumpBoostInWindow(100, INDEX_NONE, true, 8));
	TestTrue(TEXT("pressed on the exit frame"), UGSTN_DrillMath::IsJumpBoostInWindow(100, 100, false, 8));
	TestTrue(TEXT("pressed 8 frames before"), UGSTN_DrillMath::IsJumpBoostInWindow(100, 92, false, 8));
	TestFalse(TEXT("pressed 9 frames before"), UGSTN_DrillMath::IsJumpBoostInWindow(100, 91, false, 8));
	TestFalse(TEXT("never pressed"), UGSTN_DrillMath::IsJumpBoostInWindow(100, INDEX_NONE, false, 8));
	TestFalse(TEXT("a press in the future does not count"), UGSTN_DrillMath::IsJumpBoostInWindow(100, 101, false, 8));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DrillMathRefreshTest, "GSTNSpinDrill.Core.Math.AirSpinRefresh", GSTNSpinDrillTest::Flags)
bool FGSTN_DrillMathRefreshTest::RunTest(const FString& Parameters)
{
	const FGSTN_AirSpinTuning Defaults;
	const int32 Land = static_cast<int32>(EGSTN_AirSpinRefresh::Land);
	const int32 Wall = static_cast<int32>(EGSTN_AirSpinRefresh::Wall);
	const int32 Trampoline = static_cast<int32>(EGSTN_AirSpinRefresh::Trampoline);
	const int32 Launch = static_cast<int32>(EGSTN_AirSpinRefresh::Launch);
	const int32 Hit = static_cast<int32>(EGSTN_AirSpinRefresh::Hit);

	// Default mask: land, wall, trampoline, launch; not hit
	for (const int32 Event : { Land, Wall, Trampoline, Launch })
	{
		TestEqual(*FString::Printf(TEXT("event %d refreshes by default"), Event), UGSTN_DrillMath::AirSpinsUsedAfterEvents(1, Defaults.RefreshOn, Event), 0);
	}
	TestEqual(TEXT("a hit does not refresh by default"), UGSTN_DrillMath::AirSpinsUsedAfterEvents(1, Defaults.RefreshOn, Hit), 1);
	TestEqual(TEXT("no event keeps the count"), UGSTN_DrillMath::AirSpinsUsedAfterEvents(1, Defaults.RefreshOn, 0), 1);
	TestEqual(TEXT("several events on one frame"), UGSTN_DrillMath::AirSpinsUsedAfterEvents(1, Defaults.RefreshOn, Hit | Wall), 0);
	TestTrue(TEXT("custom mask: hit only"), UGSTN_DrillMath::ShouldRefreshAirSpin(Hit, Hit) && !UGSTN_DrillMath::ShouldRefreshAirSpin(Hit, Land));

	TestTrue(TEXT("first air spin allowed"), UGSTN_DrillMath::CanAirSpin(0, Defaults.UsesPerAirtime));
	TestFalse(TEXT("second refused"), UGSTN_DrillMath::CanAirSpin(1, Defaults.UsesPerAirtime));

	// Only a launch that the launch tuning lets refresh raises Launch (a hop out and a bounce do not, 7.6)
	TestEqual(TEXT("launch refreshes"), UGSTN_DrillMath::RefreshEventsForEnd(EGSTN_DrillEndResult::Launch, true), Launch);
	TestEqual(TEXT("launch with refresh off"), UGSTN_DrillMath::RefreshEventsForEnd(EGSTN_DrillEndResult::Launch, false), 0);
	TestEqual(TEXT("hop out"), UGSTN_DrillMath::RefreshEventsForEnd(EGSTN_DrillEndResult::DetachPop, true), 0);
	TestEqual(TEXT("hitstun detach"), UGSTN_DrillMath::RefreshEventsForEnd(EGSTN_DrillEndResult::DetachHitstun, true), 0);

	// Target response stays a small POD, defaults change nothing
	const FGSTN_DrillTargetResponse Response;
	TestTrue(TEXT("response fits 20 bytes"), sizeof(FGSTN_DrillTargetResponse) <= 20);
	TestTrue(TEXT("response defaults are neutral"), Response.SeatFramesScale == 1.f && Response.MaxBoreFramesOverride == 0
		&& Response.DamagePerHitScale == 1.f && !Response.bLatchMoves && !Response.bNeverDrillable);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
