// Automation tests of the additive locomotion lean (docs/notes/additive-lean-1.md). Pure: no world.

#include "Misc/AutomationTest.h"
#include "Presentation/GSTN_LocomotionLean.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_LEAN_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LocomotionLeanDirectionTest, "GSTNCombat.NP.LocomotionLean.Direction", GSTN_LEAN_TEST_FLAGS)
bool FGSTN_LocomotionLeanDirectionTest::RunTest(const FString& Parameters)
{
	const FGSTN_LocomotionLeanSettings Settings;
	const FVector Run(600.f, 0.f, 0.f);
	const FVector Fwd(1.f, 0.f, 0.f);

	const FVector Turn = FGSTN_LocomotionLeanSolver::TargetLean(Settings, Run, FVector(0.f, 1500.f, 0.f), Fwd, 1.f);
	TestTrue(TEXT("a turn to the right leans right (into the turn)"), Turn.Y > 1.f && FMath::Abs(Turn.X) < KINDA_SMALL_NUMBER);

	const FVector Brake = FGSTN_LocomotionLeanSolver::TargetLean(Settings, Run, FVector(-1500.f, 0.f, 0.f), Fwd, 1.f);
	TestTrue(TEXT("braking leans back"), Brake.X < -0.1f);
	TestTrue(TEXT("speed changes lean less than turns"), FMath::Abs(Brake.X) < Turn.Y);

	const FVector Hard = FGSTN_LocomotionLeanSolver::TargetLean(Settings, Run, FVector(0.f, 50000.f, 0.f), Fwd, 1.f);
	TestTrue(TEXT("never past the max angle"), Hard.Size() <= Settings.MaxAngleDeg + KINDA_SMALL_NUMBER);

	TestTrue(TEXT("no lean under the min speed"),
		FGSTN_LocomotionLeanSolver::TargetLean(Settings, FVector(30.f, 0.f, 0.f), FVector(0.f, 1500.f, 0.f), Fwd, 1.f).IsNearlyZero());
	TestTrue(TEXT("a zero state scale (air, attack) leans nothing"),
		FGSTN_LocomotionLeanSolver::TargetLean(Settings, Run, FVector(0.f, 1500.f, 0.f), Fwd, 0.f).IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LocomotionLeanSolverTest, "GSTNCombat.NP.LocomotionLean.Solver", GSTN_LEAN_TEST_FLAGS)
bool FGSTN_LocomotionLeanSolverTest::RunTest(const FString& Parameters)
{
	const FGSTN_LocomotionLeanSettings Settings;
	const float Dt = 1.f / 60.f;
	const FVector Fwd(1.f, 0.f, 0.f);

	// Straight at a constant speed: settles straight
	FGSTN_LocomotionLeanSolver Straight;
	for (int32 i = 0; i < 120; ++i)
	{
		Straight.Step(Settings, FVector(600.f, 0.f, 0.f), Fwd, Dt, 1.f);
	}
	TestTrue(TEXT("constant run: no lean"), Straight.Lean.Size() < 0.05f);

	// A steady circle, counter-clockwise (turning left): the lean points to the centre
	FGSTN_LocomotionLeanSolver Circle;
	const float Speed = 600.f;
	const float Rate = 2.f;  // rad/s: 1200 cm/s^2 centripetal
	FVector Velocity;
	for (int32 i = 0; i < 180; ++i)
	{
		const float Angle = Rate * Dt * i;
		Velocity = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Speed;
		Circle.Step(Settings, Velocity, Velocity.GetSafeNormal(), Dt, 1.f);
	}
	const FVector Left = FVector::CrossProduct(FVector::UpVector, Velocity.GetSafeNormal());
	TestTrue(TEXT("steady turn leans into the turn"), FVector::DotProduct(Circle.Lean.GetSafeNormal(), Left) > 0.9f);
	const float Expected = Speed * Rate * Settings.MaxAngleDeg / Settings.AccelForMaxLean;
	TestTrue(TEXT("by the expected angle"), FMath::IsNearlyEqual(Circle.Lean.Size(), Expected, Expected * 0.15f));

	// A teleport (huge velocity jump) is ignored
	FGSTN_LocomotionLeanSolver Teleport;
	Teleport.Step(Settings, FVector(600.f, 0.f, 0.f), Fwd, Dt, 1.f);
	Teleport.Step(Settings, FVector(-600.f, 0.f, 0.f), Fwd, 0.001f, 1.f);
	TestTrue(TEXT("a teleport does not snap the body"), Teleport.Lean.Size() < 0.05f);
	return true;
}

#endif
