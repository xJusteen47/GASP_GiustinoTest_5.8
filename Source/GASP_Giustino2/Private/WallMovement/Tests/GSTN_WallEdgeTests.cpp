// Pure tests of the edge look-ahead maths (wall-slide-v2.md W1): GSTN.Wall.Edge.*

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "WallMovement/GSTN_WallEdgeTypes.h"
#include "WallMovement/MoveLibrary/GSTN_WallEdgeUtils.h"

/** Double-safe tolerance compare (FVector2D components are double, the literals float: TestNearlyEqual is ambiguous) */
static void EdgeNearlyEq(FAutomationTestBase& Test, const TCHAR* What, double A, double B, double Tolerance = 1e-4)
{
	Test.TestTrue(What, FMath::Abs(A - B) <= Tolerance);
}

namespace GSTNWallEdgeTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float MaxEdge = 80.f;
	constexpr float Terminal = 600.f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallEdgeBumpWinsTest, "GSTN.Wall.Edge.BumpWinsOverAngle", GSTNWallEdgeTest::TestFlags)
bool FGSTN_WallEdgeBumpWinsTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallEdgeTest;
	const FGSTN_WallEdgeSettings S;
	for (float Angle : { 20.f, 45.f, 75.f, 89.f })
	{
		TestEqual(TEXT("Bump within step height at any angle"), UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, true, -S.MaxStepHeight, Angle, false), EGSTN_WallEdgeKind::Bump);
		TestEqual(TEXT("Recess within step height too"), UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, true, S.MaxStepHeight * 0.5f, Angle, false), EGSTN_WallEdgeKind::Bump);
	}
	// Taller than the step: the angle decides
	TestEqual(TEXT("Tall + 45 deg = concave"), UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, true, -S.MaxStepHeight - 1.f, 45.f, false), EGSTN_WallEdgeKind::Concave);
	TestEqual(TEXT("Tall + beyond limit = blocked"), UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, true, -S.MaxStepHeight - 1.f, 85.f, false), EGSTN_WallEdgeKind::Blocked);
	// No hit: the wall ends
	TestEqual(TEXT("Wall ends = convex"), UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, false, 0.f, 90.f, false), EGSTN_WallEdgeKind::Convex);
	TestEqual(TEXT("Far face in band = convex"), UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, false, 0.f, 60.f, true), EGSTN_WallEdgeKind::Convex);
	TestEqual(TEXT("Far face beyond limit = blocked"), UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, false, 0.f, 85.f, true), EGSTN_WallEdgeKind::Blocked);
	TestEqual(TEXT("Flush flat wall"), UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, true, 0.f, 0.f, false), EGSTN_WallEdgeKind::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallEdgeSlopeTest, "GSTN.Wall.Edge.SlopeBelowSmoothAngleNoCap", GSTNWallEdgeTest::TestFlags)
bool FGSTN_WallEdgeSlopeTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallEdgeTest;
	const FGSTN_WallEdgeSettings S;
	for (float Angle : { 1.f, 8.f, S.MaxSmoothAngle })
	{
		const EGSTN_WallEdgeKind Kind = UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, true, -50.f, Angle, false);
		TestEqual(TEXT("Smooth long change is a slope"), Kind, EGSTN_WallEdgeKind::Slope);
		TestEqual(TEXT("Slope never caps"), UGSTN_WallEdgeUtils::EdgeSpeedCap(S, Kind, Angle, Terminal), TNumericLimits<float>::Max());
	}
	TestEqual(TEXT("Smooth far face on a missing wall is a slope"), UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, false, 0.f, 10.f, true), EGSTN_WallEdgeKind::Slope);
	TestEqual(TEXT("Bump never caps"), UGSTN_WallEdgeUtils::EdgeSpeedCap(S, EGSTN_WallEdgeKind::Bump, 70.f, Terminal), TNumericLimits<float>::Max());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallEdgeConcaveCapTest, "GSTN.Wall.Edge.ConcaveCapScalesWithAngle", GSTNWallEdgeTest::TestFlags)
bool FGSTN_WallEdgeConcaveCapTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallEdgeTest;
	const FGSTN_WallEdgeSettings S;
	EdgeNearlyEq(*this, TEXT("Full speed at the smooth limit"), UGSTN_WallEdgeUtils::EdgeSpeedCap(S, EGSTN_WallEdgeKind::Concave, S.MaxSmoothAngle, Terminal), Terminal, 0.01f);
	EdgeNearlyEq(*this, TEXT("Min scale at 90"), UGSTN_WallEdgeUtils::EdgeSpeedCap(S, EGSTN_WallEdgeKind::Concave, 90.f, Terminal), Terminal * S.ConcaveMinScale, 0.01f);
	float Previous = Terminal + 1.f;
	for (float Angle = S.MaxSmoothAngle; Angle <= 90.f; Angle += 5.f)
	{
		const float Cap = UGSTN_WallEdgeUtils::EdgeSpeedCap(S, EGSTN_WallEdgeKind::Concave, Angle, Terminal);
		TestTrue(TEXT("Cap decreases with angle"), Cap <= Previous);
		Previous = Cap;
	}
	FGSTN_WallEdgeSettings Off = S;
	Off.bSlowOnConcave = false;
	TestEqual(TEXT("Disabled class never caps"), UGSTN_WallEdgeUtils::EdgeSpeedCap(Off, EGSTN_WallEdgeKind::Concave, 80.f, Terminal), TNumericLimits<float>::Max());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallEdgeConvexCapTest, "GSTN.Wall.Edge.ConvexCapScalesWithAngle", GSTNWallEdgeTest::TestFlags)
bool FGSTN_WallEdgeConvexCapTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallEdgeTest;
	const FGSTN_WallEdgeSettings S;
	EdgeNearlyEq(*this, TEXT("Min scale at 90"), UGSTN_WallEdgeUtils::EdgeSpeedCap(S, EGSTN_WallEdgeKind::Convex, 90.f, Terminal), Terminal * S.ConvexMinScale, 0.01f);
	const float Mid = UGSTN_WallEdgeUtils::EdgeSpeedCap(S, EGSTN_WallEdgeKind::Convex, 52.5f, Terminal);
	EdgeNearlyEq(*this, TEXT("Halfway is the middle scale"), Mid, Terminal * (1.f + S.ConvexMinScale) * 0.5f, 0.5f);
	TestTrue(TEXT("Convex is stricter than concave at the same angle"),
		UGSTN_WallEdgeUtils::EdgeSpeedCap(S, EGSTN_WallEdgeKind::Convex, 70.f, Terminal) < UGSTN_WallEdgeUtils::EdgeSpeedCap(S, EGSTN_WallEdgeKind::Concave, 70.f, Terminal));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallEdgeApproachTest, "GSTN.Wall.Edge.ApproachCurveMonotonic", GSTNWallEdgeTest::TestFlags)
bool FGSTN_WallEdgeApproachTest::RunTest(const FString& Parameters)
{
	const FGSTN_WallEdgeSettings S;
	float Previous = -1.f;
	for (float D = 0.f; D <= 400.f; D += 10.f)
	{
		const float Allowed = UGSTN_WallEdgeUtils::AllowedSpeedAtDistance(S, 240.f, D);
		TestTrue(TEXT("Allowed speed grows with distance"), Allowed >= Previous);
		Previous = Allowed;
	}
	EdgeNearlyEq(*this, TEXT("At the edge it is the corner speed"), UGSTN_WallEdgeUtils::AllowedSpeedAtDistance(S, 240.f, 0.f), 240.f, 0.01f);
	EdgeNearlyEq(*this, TEXT("Constant braking curve"), UGSTN_WallEdgeUtils::AllowedSpeedAtDistance(S, 100.f, 50.f), FMath::Sqrt(100.f * 100.f + 2.f * S.CornerBraking * 50.f), 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallEdgeCornerFloorTest, "GSTN.Wall.Edge.CapNeverBelowCornerSpeed", GSTNWallEdgeTest::TestFlags)
bool FGSTN_WallEdgeCornerFloorTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallEdgeTest;
	const FGSTN_WallEdgeSettings S;
	for (EGSTN_WallEdgeKind Kind : { EGSTN_WallEdgeKind::Concave, EGSTN_WallEdgeKind::Convex })
	{
		for (float Angle = 16.f; Angle <= 90.f; Angle += 7.f)
		{
			const float Corner = UGSTN_WallEdgeUtils::EdgeSpeedCap(S, Kind, Angle, Terminal);
			TestTrue(TEXT("Corner speed within (0, terminal]"), Corner > 0.f && Corner <= Terminal);
			for (float D : { 0.f, 5.f, 50.f, 500.f })
			{
				TestTrue(TEXT("Allowed >= corner"), UGSTN_WallEdgeUtils::AllowedSpeedAtDistance(S, Corner, D) >= Corner);
			}
		}
	}
	EdgeNearlyEq(*this, TEXT("Negative distance is clamped"), UGSTN_WallEdgeUtils::AllowedSpeedAtDistance(S, 100.f, -30.f), 100.f, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallEdgeGravityIndependentTest, "GSTN.Wall.Edge.GravityIndependentAngles", GSTNWallEdgeTest::TestFlags)
bool FGSTN_WallEdgeGravityIndependentTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallEdgeTest;
	const FGSTN_WallEdgeSettings S;
	// The same wall + edge pair rotated into several frames (several gravity-up directions) classifies and caps identically
	const FVector WallN = FVector(1, 0, 0);
	const FVector HitN = FQuat(FVector::UpVector, FMath::DegreesToRadians(50.f)).RotateVector(WallN);
	const float Reference = UGSTN_WallEdgeUtils::AngleBetweenNormalsDeg(WallN, HitN);
	EdgeNearlyEq(*this, TEXT("Reference angle"), Reference, 50.f, 0.01f);
	const EGSTN_WallEdgeKind RefKind = UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, true, -30.f, Reference, false);
	const float RefCap = UGSTN_WallEdgeUtils::EdgeSpeedCap(S, RefKind, Reference, Terminal);

	const TArray<FQuat> Frames = {
		FQuat::Identity,
		FQuat(FVector::XAxisVector, PI),
		FQuat(FVector::YAxisVector, FMath::DegreesToRadians(90.f)),
		FQuat(FVector(1, 1, 1).GetSafeNormal(), FMath::DegreesToRadians(77.f)),
		FQuat(FVector(-2, 1, 3).GetSafeNormal(), FMath::DegreesToRadians(133.f)),
	};
	for (const FQuat& Frame : Frames)
	{
		const float Angle = UGSTN_WallEdgeUtils::AngleBetweenNormalsDeg(Frame.RotateVector(WallN), Frame.RotateVector(HitN));
		EdgeNearlyEq(*this, TEXT("Angle is frame independent"), Angle, Reference, 0.01f);
		const EGSTN_WallEdgeKind Kind = UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, true, -30.f, Angle, false);
		TestEqual(TEXT("Kind is frame independent"), Kind, RefKind);
		EdgeNearlyEq(*this, TEXT("Cap is frame independent"), UGSTN_WallEdgeUtils::EdgeSpeedCap(S, Kind, Angle, Terminal), RefCap, 0.1f);
	}
	TestEqual(TEXT("Bump height of a 6 cm rib"), UGSTN_WallEdgeUtils::BumpHeightOf(S, -6.f), 6.f);
	TestEqual(TEXT("Bump height of a recess is 0"), UGSTN_WallEdgeUtils::BumpHeightOf(S, 4.f), 0.f);
	TestEqual(TEXT("Bump height clamps to the step"), UGSTN_WallEdgeUtils::BumpHeightOf(S, -50.f), S.MaxStepHeight);
	TestEqual(TEXT("Lookahead floor"), UGSTN_WallEdgeUtils::LookaheadDistance(S, 10.f), S.MinLookahead);
	EdgeNearlyEq(*this, TEXT("Lookahead scales with speed"), UGSTN_WallEdgeUtils::LookaheadDistance(S, 600.f), 600.f * S.LookaheadTime, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallEdgeStepFaceTest, "GSTN.Wall.Edge.FlatFacedStepIsBumpAndRecedingEdgeIsConvex", GSTNWallEdgeTest::TestFlags)
bool FGSTN_WallEdgeStepFaceTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallEdgeTest;
	const FGSTN_WallEdgeSettings S;
	// A face parallel to the wall 5 cm proud of it, sampled 60 cm ahead: a slope could not have made that offset, it is a step
	TestEqual(TEXT("Flat-faced step"), UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, true, -5.f, 0.f, false, 60.f), EGSTN_WallEdgeKind::Bump);
	// A 10 degree slope sampled 20 cm ahead makes up to 3.5 cm: slope
	TestEqual(TEXT("Gentle slope"), UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, true, -3.f, 10.f, false, 20.f), EGSTN_WallEdgeKind::Slope);
	TestEqual(TEXT("Unknown distance keeps the old rule"), UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, true, -5.f, 0.f, false), EGSTN_WallEdgeKind::Slope);
	// Past the step height the sign of the offset says which way the surface turns
	TestEqual(TEXT("Rising toward the character"), UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, true, -40.f, 45.f, false, 90.f), EGSTN_WallEdgeKind::Concave);
	TestEqual(TEXT("Falling away"), UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, true, 40.f, 45.f, false, 90.f), EGSTN_WallEdgeKind::Convex);
	TestEqual(TEXT("Falling away beyond the limit"), UGSTN_WallEdgeUtils::ClassifyAhead(S, MaxEdge, true, 40.f, 85.f, false, 90.f), EGSTN_WallEdgeKind::Blocked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallEdgeStepLiftTest, "GSTN.Wall.Edge.StepLiftAndCapsuleSupport", GSTNWallEdgeTest::TestFlags)
bool FGSTN_WallEdgeStepLiftTest::RunTest(const FString& Parameters)
{
	const FGSTN_WallEdgeSettings S;
	// Seated 2 cm above the plane: only the part of the bump above the capsule has to be cleared, plus the margin
	EdgeNearlyEq(*this, TEXT("5 cm bump"), UGSTN_WallEdgeUtils::StepLiftFor(S, 5.f, 2.f), 3.f + S.StepMargin);
	EdgeNearlyEq(*this, TEXT("A bump under the gap needs only the margin"), UGSTN_WallEdgeUtils::StepLiftFor(S, 1.f, 2.f), S.StepMargin);
	EdgeNearlyEq(*this, TEXT("Never more than step + margin"), UGSTN_WallEdgeUtils::StepLiftFor(S, 80.f, 2.f), S.MaxStepHeight + S.StepMargin);

	// An upright capsule: its radius sideways, its half height along its axis, in between for a diagonal
	const FQuat Upright = FQuat::Identity;
	EdgeNearlyEq(*this, TEXT("Sideways"), UGSTN_WallEdgeUtils::CapsuleSupportAlong(34.f, 88.f, Upright, FVector::ForwardVector), 34.f);
	EdgeNearlyEq(*this, TEXT("Along the axis"), UGSTN_WallEdgeUtils::CapsuleSupportAlong(34.f, 88.f, Upright, FVector::UpVector), 88.f);
	const FVector Diagonal = FVector(1, 0, 1).GetSafeNormal();
	EdgeNearlyEq(*this, TEXT("Diagonal"), UGSTN_WallEdgeUtils::CapsuleSupportAlong(34.f, 88.f, Upright, Diagonal), 54.f * Diagonal.Z + 34.f);
	const FQuat Tilted(FVector::RightVector, FMath::DegreesToRadians(90.f));
	EdgeNearlyEq(*this, TEXT("A tilted capsule follows its own axis"), UGSTN_WallEdgeUtils::CapsuleSupportAlong(34.f, 88.f, Tilted, FVector::ForwardVector), 88.f, 1e-3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
