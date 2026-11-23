// Tests of the ground contact correction math (FGSTN_GroundContactSolver): GSTNAnimation.Core.GroundContact.*
//
// Pure: no pose, no world. The rule under test is "only remove penetration": a sphere above the floor is never moved.

#if WITH_DEV_AUTOMATION_TESTS

#include "GSTN_GroundContactSolver.h"
#include "Misc/AutomationTest.h"

namespace GSTNGroundContactTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	static FGSTN_GroundPlane Flat(double Z = 0.0)
	{
		FGSTN_GroundPlane Plane;
		Plane.Point = FVector(0.0, 0.0, Z);
		Plane.Normal = FVector::UpVector;
		return Plane;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GroundContactLiftTest, "GSTNAnimation.Core.GroundContact.Lift", GSTNGroundContactTest::TestFlags)
bool FGSTN_GroundContactLiftTest::RunTest(const FString& Parameters)
{
	using namespace GSTNGroundContactTest;
	const FGSTN_GroundPlane Floor = Flat(10.0);
	// Above the floor: no lift (the authored pose passes through)
	{
		const FVector Points[] = {FVector(0, 0, 30), FVector(20, 0, 25)};
		const double Radii[] = {12.0, 10.0};
		TestEqual(TEXT("no penetration, no lift"), FGSTN_GroundContactSolver::RequiredLift(Floor, Points, Radii, 35.0), 0.0);
	}
	// The deepest sphere wins: pelvis 4 cm in, head 9 cm in
	{
		const FVector Points[] = {FVector(0, 0, 18), FVector(40, 0, 12)};
		const double Radii[] = {12.0, 11.0};
		TestEqual(TEXT("deepest sphere sets the lift"), FGSTN_GroundContactSolver::RequiredLift(Floor, Points, Radii, 35.0), 9.0, 1e-6);
	}
	// Capped: a 60 cm sink is a game bug, not hidden
	{
		const FVector Points[] = {FVector(0, 0, -50)};
		const double Radii[] = {12.0};
		TestEqual(TEXT("capped at MaxLift"), FGSTN_GroundContactSolver::RequiredLift(Floor, Points, Radii, 35.0), 35.0, 1e-6);
	}
	// Sloped floor: height measured along the normal
	{
		FGSTN_GroundPlane Slope;
		Slope.Normal = FVector(0.0, 0.6, 0.8);
		TestEqual(TEXT("sloped penetration along the normal"), FGSTN_GroundContactSolver::Penetration(Slope, FVector(0, 0, 5), 10.0), 6.0, 1e-6);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GroundContactSmoothingTest, "GSTNAnimation.Core.GroundContact.Smoothing", GSTNGroundContactTest::TestFlags)
bool FGSTN_GroundContactSmoothingTest::RunTest(const FString& Parameters)
{
	// Up at once: no frame shows the body in the floor
	TestEqual(TEXT("rises at once"), FGSTN_GroundContactSolver::SmoothLift(2.0, 10.0, 1.0 / 60.0, 0.05), 10.0);
	// Down with the half-life: after one half-life, half way
	TestEqual(TEXT("falls by half in one half-life"), FGSTN_GroundContactSolver::SmoothLift(10.0, 0.0, 0.05, 0.05), 5.0, 1e-6);
	TestEqual(TEXT("half-life 0 falls at once"), FGSTN_GroundContactSolver::SmoothLift(10.0, 0.0, 0.016, 0.0), 0.0);
	// Weight blend
	TestEqual(TEXT("blend in over BlendIn"), FGSTN_GroundContactSolver::BlendWeight(0.f, 1.f, 0.025f, 0.05f, 0.15f), 0.5f, 1e-5f);
	TestEqual(TEXT("blend out over BlendOut"), FGSTN_GroundContactSolver::BlendWeight(1.f, 0.f, 0.075f, 0.05f, 0.15f), 0.5f, 1e-5f);
	TestEqual(TEXT("blend never overshoots"), FGSTN_GroundContactSolver::BlendWeight(0.9f, 1.f, 1.f, 0.05f, 0.15f), 1.f);
	TestEqual(TEXT("blend time 0 is a cut"), FGSTN_GroundContactSolver::BlendWeight(1.f, 0.f, 0.001f, 0.05f, 0.f), 0.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_GroundContactLimbTest, "GSTNAnimation.Core.GroundContact.Limb", GSTNGroundContactTest::TestFlags)
bool FGSTN_GroundContactLimbTest::RunTest(const FString& Parameters)
{
	using namespace GSTNGroundContactTest;
	const FGSTN_GroundPlane Floor = Flat();
	FVector Target;
	// A hand in the air that the lift carries up: untouched
	TestFalse(TEXT("airborne hand is not moved"),
		FGSTN_GroundContactSolver::LimbTarget(Floor, FVector(0, 0, 40), FVector(0, 0, 50), 4.0, 3.0, true, 30.0, Target));
	// A hand resting on the floor before a 10 cm body lift: back on the floor (no floating hand)
	TestTrue(TEXT("resting hand keeps its contact"),
		FGSTN_GroundContactSolver::LimbTarget(Floor, FVector(5, 0, 4), FVector(5, 0, 14), 4.0, 3.0, true, 30.0, Target));
	TestEqual(TEXT("resting hand target = where the animation put it"), Target, FVector(5, 0, 4));
	// Same with contacts off: the hand rides the lift
	TestFalse(TEXT("contacts off: hand rides the lift"),
		FGSTN_GroundContactSolver::LimbTarget(Floor, FVector(5, 0, 4), FVector(5, 0, 14), 4.0, 3.0, false, 30.0, Target));
	// A foot 6 cm in the floor, no lift: raised exactly out of it
	TestTrue(TEXT("sinking foot is raised"),
		FGSTN_GroundContactSolver::LimbTarget(Floor, FVector(0, 0, 1), FVector(0, 0, 1), 7.0, 3.0, false, 30.0, Target));
	TestEqual(TEXT("sinking foot target on the floor"), Target, FVector(0, 0, 7));
	// Capped move
	TestTrue(TEXT("deep foot is raised"),
		FGSTN_GroundContactSolver::LimbTarget(Floor, FVector(0, 0, -50), FVector(0, 0, -50), 7.0, 3.0, false, 30.0, Target));
	TestEqual(TEXT("capped at MaxLimbCorrection"), Target, FVector(0, 0, -20));
	return true;
}

#endif
