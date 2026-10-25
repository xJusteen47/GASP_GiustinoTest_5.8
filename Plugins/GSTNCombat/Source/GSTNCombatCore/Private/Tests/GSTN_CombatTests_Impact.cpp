// Automation tests of the launch direction, directional influence and surface impact resolution (checkpoint H3). Pure: in-memory
// structs, no world and no content asset. The DI worked examples are the owner's (combat-m4-hits 3, DI).

#include "Tests/GSTN_CombatTestUtils.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitMathUtils.h"
#include "Hits/GSTN_ImpactMath.h"
#include "Hits/GSTN_ImpactTypes.h"
#include "GSTN_SurfaceTags.h"
#include "GSTN_SurfaceUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNImpactTest
{
	/** A launch of the given speed whose elevation dot against Up is E and whose horizontal part is Horizontal */
	FVector MakeLaunch(double Speed, double E, const FVector& Horizontal, const FVector& Up)
	{
		return (Horizontal * FMath::Sqrt(1.0 - E * E) + Up * E) * Speed;
	}

	double ElevationOf(const FVector& Launch, const FVector& Up)
	{
		return FVector::DotProduct(Launch.GetSafeNormal(), Up);
	}

	FGameplayTag Surface(const TCHAR* Leaf)
	{
		return FGameplayTag::RequestGameplayTag(FName(*FString::Printf(TEXT("Surface.Type.%s"), Leaf)), false);
	}

	FGSTN_SurfaceImpactRow MakeRow(const FGameplayTag& Tag, EGSTN_ImpactOutcome Default, float MinSpeed)
	{
		FGSTN_SurfaceImpactRow Row;
		Row.Surface = Tag;
		Row.DefaultOutcome = Default;
		Row.MinImpactSpeed = MinSpeed;
		return Row;
	}

	/**
	 * Wall (splat, min 300, allows all), Glass (break, min 600, allows only break), Crate (nothing, min 400, allows nothing),
	 * Trampoline (bounce forced, restitution 1.2). Row 0 to 3 in that order (the rows are found through the GSTNSurface type index of their tag, whatever the order).
	 */
	TStrongObjectPtr<UGSTN_SurfaceImpactTable> MakeTable()
	{
		TStrongObjectPtr<UGSTN_SurfaceImpactTable> Table(NewObject<UGSTN_SurfaceImpactTable>(GetTransientPackage()));
		FGSTN_SurfaceImpactRow Wall = MakeRow(Surface(TEXT("Wall")), EGSTN_ImpactOutcome::Splat, 300.f);
		Wall.Restitution = 0.5f;
		Wall.SplatFrames = 20;
		FGSTN_SurfaceImpactRow Glass = MakeRow(Surface(TEXT("Glass")), EGSTN_ImpactOutcome::BreakSurface, 600.f);
		Glass.Allowed.bSplat = false;
		Glass.Allowed.bBounce = false;
		FGSTN_SurfaceImpactRow Crate = MakeRow(Surface(TEXT("Crate")), EGSTN_ImpactOutcome::None, 400.f);
		Crate.Allowed.bSplat = Crate.Allowed.bBounce = Crate.Allowed.bBreakSurface = false;
		FGSTN_SurfaceImpactRow Trampoline = MakeRow(Surface(TEXT("Trampoline")), EGSTN_ImpactOutcome::Bounce, 0.f);
		Trampoline.bForceDefault = true;
		Trampoline.Restitution = 1.2f;
		Table->Rows = { Wall, Glass, Crate, Trampoline };
		Table->Compile();
		return Table;
	}

	FGSTN_ImpactResponse NoSplatVictim()
	{
		FGSTN_ImpactResponse Response;
		Response.Default.Allowed.bSplat = false;
		return Response;
	}

	FGSTN_ImpactProfile Forcing(EGSTN_ImpactOutcome Outcome)
	{
		FGSTN_ImpactProfile Profile;
		Profile.Force = Outcome;
		return Profile;
	}

	/** The contact input of a test row: the GSTNSurface type index of the row's tag; a row that does not exist is used as a raw index */
	FGSTN_ImpactInput InputOf(const UGSTN_SurfaceImpactTable* Table, int32 Row, double Speed)
	{
		const uint8 TypeIndex = Table->Rows.IsValidIndex(Row) ? UGSTN_SurfaceUtils::GetSurfaceTypeIndex(Table->Rows[Row].Surface) : static_cast<uint8>(Row);
		return FGSTN_ImpactInput::Make(TypeIndex, Speed);
	}

	EGSTN_ImpactOutcome Outcome(const UGSTN_SurfaceImpactTable* Table, int32 Row, double Speed, const FGSTN_ImpactResponse& Response, const FGSTN_ImpactProfile& Profile)
	{
		return UGSTN_ImpactMath::Resolve(InputOf(Table, Row, Speed), Table, Response, Profile).Outcome;
	}

	bool NearlyEqual(const FVector& A, const FVector& B, double Tolerance = 1.0e-9)
	{
		return (A - B).Size() <= Tolerance;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DIWorkedExamplesTest, "GSTNCombat.Core.Hits.DI.WorkedExamples", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DIWorkedExamplesTest::RunTest(const FString& Parameters)
{
	using namespace GSTNImpactTest;
	const FGSTN_DIParams Params;
	const FVector Up = FVector::ZAxisVector;
	const FVector H = FVector::XAxisVector;
	const FVector DIIn = -H;  // the stick against the launch's horizontal direction
	const FVector DIOut = H;

	struct FCase { double E; double In; double Out; };
	const FCase Cases[] = { { 0.4, 0.6, 0.2 }, { 0.0, 0.2, 0.0 }, { -0.5, -0.7, -0.3 }, { -0.05, 0.15, -0.05 } };
	for (const FCase& Case : Cases)
	{
		const FVector Launch = MakeLaunch(1000.0, Case.E, H, Up);
		TestEqual(*FString::Printf(TEXT("e %.2f, DI in"), Case.E), ElevationOf(UGSTN_HitMathUtils::ApplyDI(Launch, DIIn, Up, H, Params), Up), Case.In, 1.0e-6);
		TestEqual(*FString::Printf(TEXT("e %.2f, DI out"), Case.E), ElevationOf(UGSTN_HitMathUtils::ApplyDI(Launch, DIOut, Up, H, Params), Up), Case.Out, 1.0e-6);
	}

	// Half the stick, half the change: 0.4 -> 0.5
	TestEqual(TEXT("half stick DI in"), ElevationOf(UGSTN_HitMathUtils::ApplyDI(MakeLaunch(1000.0, 0.4, H, Up), DIIn * 0.5, Up, H, Params), Up), 0.5, 1.0e-6);

	// The jump at the threshold is inherent in the rule: -0.09 steepens up, -0.11 steepens down
	TestEqual(TEXT("just above the threshold"), ElevationOf(UGSTN_HitMathUtils::ApplyDI(MakeLaunch(1000.0, -0.09, H, Up), DIIn, Up, H, Params), Up), 0.11, 1.0e-6);
	TestEqual(TEXT("just below the threshold"), ElevationOf(UGSTN_HitMathUtils::ApplyDI(MakeLaunch(1000.0, -0.11, H, Up), DIIn, Up, H, Params), Up), -0.31, 1.0e-6);

	// No stick, no change
	const FVector Launch = MakeLaunch(1000.0, 0.4, H, Up);
	TestTrue(TEXT("no stick returns the launch"), UGSTN_HitMathUtils::ApplyDI(Launch, FVector::ZeroVector, Up, H, Params) == Launch);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DILateralRotatesAroundUpTest, "GSTNCombat.Core.Hits.DI.LateralRotatesAroundUp", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DILateralRotatesAroundUpTest::RunTest(const FString& Parameters)
{
	using namespace GSTNImpactTest;
	const FGSTN_DIParams Params;
	const FVector Up = FVector::ZAxisVector;
	const FVector Launch = FVector(1000.0, 0.0, 0.0);

	// Up x H = Z x X = Y: a stick along +Y turns the launch 15 degrees towards +Y, elevation untouched
	const double Radians = FMath::DegreesToRadians(15.0);
	FVector Result = UGSTN_HitMathUtils::ApplyDI(Launch, FVector(0.0, 1.0, 0.0), Up, Launch, Params);
	TestTrue(TEXT("+Y stick turns towards +Y"), NearlyEqual(Result, FVector(FMath::Cos(Radians), FMath::Sin(Radians), 0.0) * 1000.0));
	Result = UGSTN_HitMathUtils::ApplyDI(Launch, FVector(0.0, -1.0, 0.0), Up, Launch, Params);
	TestTrue(TEXT("-Y stick turns towards -Y"), NearlyEqual(Result, FVector(FMath::Cos(Radians), -FMath::Sin(Radians), 0.0) * 1000.0));
	Result = UGSTN_HitMathUtils::ApplyDI(Launch, FVector(0.0, 0.5, 0.0), Up, Launch, Params);
	TestTrue(TEXT("half stick, half angle"), NearlyEqual(Result, FVector(FMath::Cos(Radians * 0.5), FMath::Sin(Radians * 0.5), 0.0) * 1000.0));

	// A raised launch keeps its elevation under a pure lateral stick
	const FVector Raised = MakeLaunch(800.0, 0.4, FVector::XAxisVector, Up);
	TestEqual(TEXT("lateral stick keeps the elevation"), ElevationOf(UGSTN_HitMathUtils::ApplyDI(Raised, FVector(0.0, 1.0, 0.0), Up, FVector::XAxisVector, Params), Up), 0.4, 1.0e-6);

	// A stick off the plane is projected onto it first
	TestTrue(TEXT("a stick component along Up is ignored"), NearlyEqual(UGSTN_HitMathUtils::ApplyDI(Launch, FVector(0.0, 1.0, 5.0), Up, Launch, Params), UGSTN_HitMathUtils::ApplyDI(Launch, FVector(0.0, 1.0, 0.0), Up, Launch, Params)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DIOutNeverCrossesHorizontalTest, "GSTNCombat.Core.Hits.DI.OutNeverCrossesHorizontal", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DIOutNeverCrossesHorizontalTest::RunTest(const FString& Parameters)
{
	using namespace GSTNImpactTest;
	const FGSTN_DIParams Params;
	const FVector Up = FVector::ZAxisVector;
	bool bNeverCrosses = true;
	for (int32 Step = -90; Step <= 90; ++Step)
	{
		const double E = Step / 100.0;
		const double After = ElevationOf(UGSTN_HitMathUtils::ApplyDI(MakeLaunch(1000.0, E, FVector::XAxisVector, Up), FVector::XAxisVector, Up, FVector::XAxisVector, Params), Up);
		// Out moves towards the horizontal and stops on it: the sign of the elevation never flips, and its size never grows
		bNeverCrosses &= (E >= 0.0 ? After >= -1.0e-12 : After <= 1.0e-12) && FMath::Abs(After) <= FMath::Abs(E) + 1.0e-12;
	}
	TestTrue(TEXT("DI out never crosses the horizontal and never steepens"), bNeverCrosses);

	TestTrue(TEXT("a horizontal launch is not changed by DI out"), NearlyEqual(UGSTN_HitMathUtils::ApplyDI(FVector(1000.0, 0.0, 0.0), FVector::XAxisVector, Up, FVector::XAxisVector, Params), FVector(1000.0, 0.0, 0.0)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DIElevationClampTest, "GSTNCombat.Core.Hits.DI.ElevationClamp", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DIElevationClampTest::RunTest(const FString& Parameters)
{
	using namespace GSTNImpactTest;
	const FGSTN_DIParams Params;
	const FVector Up = FVector::ZAxisVector;
	const FVector H = FVector::XAxisVector;
	TestEqual(TEXT("steep up, DI in, clamped"), ElevationOf(UGSTN_HitMathUtils::ApplyDI(MakeLaunch(1000.0, 0.95, H, Up), -H, Up, H, Params), Up), 0.98, 1.0e-6);
	TestEqual(TEXT("steep down, DI in, clamped"), ElevationOf(UGSTN_HitMathUtils::ApplyDI(MakeLaunch(1000.0, -0.95, H, Up), -H, Up, H, Params), Up), -0.98, 1.0e-6);

	FGSTN_DIParams Tight = Params;
	Tight.MaxElevationDot = 0.5f;
	TestEqual(TEXT("the clamp is a parameter"), ElevationOf(UGSTN_HitMathUtils::ApplyDI(MakeLaunch(1000.0, 0.45, H, Up), -H, Up, H, Tight), Up), 0.5, 1.0e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DIStraightUpUsesYawSourceTest, "GSTNCombat.Core.Hits.DI.StraightUpUsesYawSource", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DIStraightUpUsesYawSourceTest::RunTest(const FString& Parameters)
{
	using namespace GSTNImpactTest;
	const FGSTN_DIParams Params;
	const FVector Up = FVector::ZAxisVector;
	const FVector Launch = FVector(0.0, 0.0, 1000.0);

	// h = the yaw source (+X), lateral = Up x h = +Y: a +Y stick turns h by 15 degrees; the launch is steepest so it clamps to 0.98
	const FVector Result = UGSTN_HitMathUtils::ApplyDI(Launch, FVector(0.0, 1.0, 0.0), Up, FVector::XAxisVector, Params);
	const double Radians = FMath::DegreesToRadians(15.0);
	const double HorizontalPart = FMath::Sqrt(1.0 - 0.98 * 0.98);
	TestTrue(TEXT("horizontal part follows the yaw source, turned by the lateral stick"), NearlyEqual(Result, FVector(FMath::Cos(Radians) * HorizontalPart, FMath::Sin(Radians) * HorizontalPart, 0.98) * 1000.0, 1.0e-4));

	// A stick along the yaw source is DI out: 1.0 -> 0.8
	TestEqual(TEXT("DI out from straight up"), ElevationOf(UGSTN_HitMathUtils::ApplyDI(Launch, FVector::XAxisVector, Up, FVector::XAxisVector, Params), Up), 0.8, 1.0e-6);

	// A different yaw source gives a different horizontal part
	const FVector Other = UGSTN_HitMathUtils::ApplyDI(Launch, FVector(0.0, 1.0, 0.0), Up, FVector::YAxisVector, Params);
	TestTrue(TEXT("the yaw source decides"), Other.Y > 0.0 && FMath::Abs(Other.X) < Other.Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DITiltedGravityTest, "GSTNCombat.Core.Hits.DI.TiltedGravity", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DITiltedGravityTest::RunTest(const FString& Parameters)
{
	using namespace GSTNImpactTest;
	const FGSTN_DIParams Params;

	// The same situation in a frame turned by an arbitrary rotation gives the turned result
	const FQuat Rotation(FVector(1.0, 2.0, 3.0).GetSafeNormal(), FMath::DegreesToRadians(110.0));
	const FVector Up = FVector::ZAxisVector;
	const FVector H = FVector::XAxisVector;
	const FVector Launch = MakeLaunch(900.0, 0.3, H, Up);
	const FVector Stick = FVector(-0.6, 0.5, 0.0);
	const FVector Base = UGSTN_HitMathUtils::ApplyDI(Launch, Stick, Up, H, Params);
	const FVector Tilted = UGSTN_HitMathUtils::ApplyDI(Rotation.RotateVector(Launch), Rotation.RotateVector(Stick), Rotation.RotateVector(Up), Rotation.RotateVector(H), Params);
	TestTrue(TEXT("a rotated frame gives the rotated result"), NearlyEqual(Tilted, Rotation.RotateVector(Base), 1.0e-8));

	// Inverted gravity: Up = -Z. The launch's elevation is measured against that Up, not against world Z
	const FVector InvertedUp = -FVector::ZAxisVector;
	const FVector InvertedLaunch = MakeLaunch(1000.0, 0.4, H, InvertedUp);
	const FVector Result = UGSTN_HitMathUtils::ApplyDI(InvertedLaunch, -H, InvertedUp, H, Params);
	TestEqual(TEXT("elevation against the inverted up"), ElevationOf(Result, InvertedUp), 0.6, 1.0e-6);
	TestTrue(TEXT("the launch still points away from world Z"), Result.Z < 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DISpeedUnchangedTest, "GSTNCombat.Core.Hits.DI.SpeedUnchanged", GSTN_CORE_TEST_FLAGS)
bool FGSTN_DISpeedUnchangedTest::RunTest(const FString& Parameters)
{
	using namespace GSTNImpactTest;
	const FGSTN_DIParams Params;
	const FVector Up = FVector(0.3, -0.2, 0.9).GetSafeNormal();
	const FVector H = UGSTN_HitMathUtils::ProjectOnPlane(FVector::XAxisVector, Up).GetSafeNormal();
	uint32 Seed = 777u;
	auto Next = [&Seed]() { Seed = Seed * 1664525u + 1013904223u; return static_cast<double>(Seed >> 8) / 16777216.0; };
	bool bSameSpeed = true;
	TArray<FVector> Runs[2];
	for (int32 Run = 0; Run < 2; ++Run)
	{
		Seed = 777u;
		for (int32 Case = 0; Case < 500; ++Case)
		{
			const double Speed = 100.0 + Next() * 4000.0;
			const FVector Launch = MakeLaunch(Speed, Next() * 1.8 - 0.9, H.RotateAngleAxis(Next() * 360.0, Up), Up);
			const FVector Stick = UGSTN_HitMathUtils::ProjectOnPlane(FVector(Next() * 2 - 1, Next() * 2 - 1, Next() * 2 - 1), Up);
			const FVector Result = UGSTN_HitMathUtils::ApplyDI(Launch, Stick, Up, H, Params);
			bSameSpeed &= FMath::Abs(Result.Size() - Speed) <= Speed * 1.0e-12;
			Runs[Run].Add(Result);
		}
	}
	TestTrue(TEXT("DI changes direction only, never speed"), bSameSpeed);
	bool bIdentical = Runs[0].Num() == Runs[1].Num();
	for (int32 Index = 0; bIdentical && Index < Runs[0].Num(); ++Index)
	{
		bIdentical = Runs[0][Index].X == Runs[1][Index].X && Runs[0][Index].Y == Runs[1][Index].Y && Runs[0][Index].Z == Runs[1][Index].Z;
	}
	TestTrue(TEXT("500 cases give the same bits on two runs"), bIdentical);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LaunchDirectionTest, "GSTNCombat.Core.Hits.DI.LaunchDirection", GSTN_CORE_TEST_FLAGS)
bool FGSTN_LaunchDirectionTest::RunTest(const FString& Parameters)
{
	using namespace GSTNImpactTest;
	const FGSTN_SakuraiParams Sakurai;
	const FVector Up = FVector::ZAxisVector;
	const FVector Facing = FVector(0.0, 1.0, 0.0);
	const FVector ToVictim = FVector(-1.0, 0.0, 0.5);
	const FVector Velocity = FVector(1.0, 1.0, 0.0);
	const double C45 = FMath::Cos(FMath::DegreesToRadians(45.0)), S45 = FMath::Sin(FMath::DegreesToRadians(45.0));

	FVector Direction = UGSTN_HitMathUtils::LaunchDirection(EGSTN_YawSource::AttackerFacing, 45.f, EGSTN_SpecialAngle::None, Up, Facing, ToVictim, Velocity, false, 0.f, Sakurai);
	TestTrue(TEXT("attacker facing, 45 degrees"), NearlyEqual(Direction, FVector(0.0, C45, S45)));
	Direction = UGSTN_HitMathUtils::LaunchDirection(EGSTN_YawSource::HitboxToVictim, 45.f, EGSTN_SpecialAngle::None, Up, Facing, ToVictim, Velocity, false, 0.f, Sakurai);
	TestTrue(TEXT("hitbox to victim: the horizontal part of the vector"), NearlyEqual(Direction, FVector(-C45, 0.0, S45)));
	Direction = UGSTN_HitMathUtils::LaunchDirection(EGSTN_YawSource::HitboxVelocity, 0.f, EGSTN_SpecialAngle::None, Up, Facing, ToVictim, Velocity, false, 0.f, Sakurai);
	TestTrue(TEXT("hitbox velocity, flat"), NearlyEqual(Direction, FVector(1.0, 1.0, 0.0).GetSafeNormal()));
	Direction = UGSTN_HitMathUtils::LaunchDirection(EGSTN_YawSource::HitboxVelocity, 30.f, EGSTN_SpecialAngle::None, Up, Facing, ToVictim, FVector::ZeroVector, false, 0.f, Sakurai);
	TestTrue(TEXT("no velocity falls back to the facing"), NearlyEqual(Direction, FVector(0.0, FMath::Cos(FMath::DegreesToRadians(30.0)), 0.5)));
	TestEqual(TEXT("always a unit vector"), Direction.Size(), 1.0, 1.0e-12);
	Direction = UGSTN_HitMathUtils::LaunchDirection(EGSTN_YawSource::AttackerFacing, 0.f, EGSTN_SpecialAngle::None, Up, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, false, 0.f, Sakurai);
	TestTrue(TEXT("nothing to go by: a fixed horizontal direction on the plane"), FMath::IsNearlyZero(Direction.Z) && FMath::IsNearlyEqual(Direction.Size(), 1.0));

	// Sakurai3D: low on a grounded victim below the threshold, high otherwise (the authored angle is ignored)
	Direction = UGSTN_HitMathUtils::LaunchDirection(EGSTN_YawSource::AttackerFacing, 80.f, EGSTN_SpecialAngle::Sakurai3D, Up, Facing, ToVictim, Velocity, true, 500.f, Sakurai);
	TestTrue(TEXT("sakurai grounded, weak: low"), FMath::IsNearlyZero(Direction.Z));
	Direction = UGSTN_HitMathUtils::LaunchDirection(EGSTN_YawSource::AttackerFacing, 80.f, EGSTN_SpecialAngle::Sakurai3D, Up, Facing, ToVictim, Velocity, true, 900.f, Sakurai);
	TestEqual(TEXT("sakurai grounded, strong: high"), Direction.Z, FMath::Sin(FMath::DegreesToRadians(44.0)), 1.0e-12);
	Direction = UGSTN_HitMathUtils::LaunchDirection(EGSTN_YawSource::AttackerFacing, 80.f, EGSTN_SpecialAngle::Sakurai3D, Up, Facing, ToVictim, Velocity, false, 100.f, Sakurai);
	TestEqual(TEXT("sakurai airborne: high"), Direction.Z, FMath::Sin(FMath::DegreesToRadians(44.0)), 1.0e-12);

	// AutoLink: along the hitbox velocity, in three dimensions
	Direction = UGSTN_HitMathUtils::LaunchDirection(EGSTN_YawSource::AttackerFacing, 10.f, EGSTN_SpecialAngle::AutoLink, Up, Facing, ToVictim, FVector(0.0, 3.0, 4.0), false, 0.f, Sakurai);
	TestTrue(TEXT("autolink"), NearlyEqual(Direction, FVector(0.0, 0.6, 0.8)));

	// Tilted up: the pitch is measured against Up
	const FVector TiltedUp = FVector(1.0, 0.0, 0.0);
	Direction = UGSTN_HitMathUtils::LaunchDirection(EGSTN_YawSource::AttackerFacing, 30.f, EGSTN_SpecialAngle::None, TiltedUp, Facing, ToVictim, Velocity, false, 0.f, Sakurai);
	TestEqual(TEXT("elevation against the tilted up"), FVector::DotProduct(Direction, TiltedUp), 0.5, 1.0e-12);
	TestTrue(TEXT("tilted up: horizontal part along the projected facing"), NearlyEqual(Direction, FVector(0.5, FMath::Cos(FMath::DegreesToRadians(30.0)), 0.0)));
	TestTrue(TEXT("a zero up gives no direction"), UGSTN_HitMathUtils::LaunchDirection(EGSTN_YawSource::AttackerFacing, 30.f, EGSTN_SpecialAngle::None, FVector::ZeroVector, Facing, ToVictim, Velocity, false, 0.f, Sakurai).IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ImpactPrecedenceTest, "GSTNCombat.Core.Impact.PrecedenceHitVictimSurface", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ImpactPrecedenceTest::RunTest(const FString& Parameters)
{
	using namespace GSTNImpactTest;
	TStrongObjectPtr<UGSTN_SurfaceImpactTable> Table = MakeTable();
	if (!TestTrue(TEXT("the surface tags are registered"), Surface(TEXT("Wall")).IsValid() && Table->FindRowIndex(Surface(TEXT("Wall"))) == 0))
	{
		return false;
	}
	const int32 Wall = 0;
	const FGSTN_ImpactResponse Anyone;
	const FGSTN_ImpactProfile Nothing;

	TestEqual(TEXT("surface default"), Outcome(Table.Get(), Wall, 1000.0, Anyone, Nothing), EGSTN_ImpactOutcome::Splat);
	TestEqual(TEXT("the victim restricts the surface default"), Outcome(Table.Get(), Wall, 1000.0, NoSplatVictim(), Nothing), EGSTN_ImpactOutcome::None);

	// The attack's Force wins over the victim's restriction
	TestEqual(TEXT("force wins over the victim"), Outcome(Table.Get(), Wall, 1000.0, NoSplatVictim(), Forcing(EGSTN_ImpactOutcome::Splat)), EGSTN_ImpactOutcome::Splat);
	TestEqual(TEXT("force wins over the surface default"), Outcome(Table.Get(), Wall, 1000.0, Anyone, Forcing(EGSTN_ImpactOutcome::Bounce)), EGSTN_ImpactOutcome::Bounce);

	// Forbid removes the surface default
	FGSTN_ImpactProfile NoSplat;
	NoSplat.Forbid = { EGSTN_ImpactOutcome::Splat };
	TestEqual(TEXT("a jab never splats"), Outcome(Table.Get(), Wall, 1000.0, Anyone, NoSplat), EGSTN_ImpactOutcome::None);
	FGSTN_ImpactProfile NoBounce;
	NoBounce.Forbid = { EGSTN_ImpactOutcome::Bounce };
	TestEqual(TEXT("forbidding another outcome changes nothing"), Outcome(Table.Get(), Wall, 1000.0, Anyone, NoBounce), EGSTN_ImpactOutcome::Splat);

	// The result carries the row's numbers, then the victim's where it sets them
	FGSTN_ImpactResult Result = UGSTN_ImpactMath::Resolve(InputOf(Table.Get(), 0, 1000.0), Table.Get(), Anyone, Nothing);
	TestEqual(TEXT("splat frames of the surface"), Result.SplatFrames, 20);
	TestEqual(TEXT("restitution of the surface"), Result.Restitution, 0.5f);
	FGSTN_ImpactResponse Heavy;
	Heavy.PerSurface.AddDefaulted();
	Heavy.PerSurface[0].Surface = Surface(TEXT("Wall"));
	Heavy.PerSurface[0].SplatFrames = 5;
	Heavy.PerSurface[0].ImpactDamageScale = 0.5f;
	Result = UGSTN_ImpactMath::Resolve(InputOf(Table.Get(), 0, 1000.0), Table.Get(), Heavy, Nothing);
	TestEqual(TEXT("splat frames of the victim"), Result.SplatFrames, 5);
	TestEqual(TEXT("damage scales multiply"), Result.ImpactDamageScale, 0.5f);

	TestEqual(TEXT("an unknown surface index resolves to None"), Outcome(Table.Get(), 77, 1000.0, Anyone, Nothing), EGSTN_ImpactOutcome::None);
	TestEqual(TEXT("so does no table"), UGSTN_ImpactMath::Resolve(FGSTN_ImpactInput::Make(1, 1000.0), nullptr, Anyone, Nothing).Outcome, EGSTN_ImpactOutcome::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ImpactForbidRemovesForceTest, "GSTNCombat.Core.Impact.ForbidRemovesForce", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ImpactForbidRemovesForceTest::RunTest(const FString& Parameters)
{
	using namespace GSTNImpactTest;
	TStrongObjectPtr<UGSTN_SurfaceImpactTable> Table = MakeTable();
	const FGSTN_ImpactResponse Anyone;
	FGSTN_ImpactProfile Profile = Forcing(EGSTN_ImpactOutcome::Splat);
	Profile.Forbid = { EGSTN_ImpactOutcome::Splat };

	// The forbidden force is gone and the surface decides: a wall's default is also the forbidden splat, glass breaks
	TestEqual(TEXT("wall: force and forbid cancel out"), Outcome(Table.Get(), 0, 1000.0, Anyone, Profile), EGSTN_ImpactOutcome::None);
	TestEqual(TEXT("glass: the surface default remains"), Outcome(Table.Get(), 1, 1000.0, Anyone, Profile), EGSTN_ImpactOutcome::BreakSurface);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ImpactForceIgnoresSurfaceFlagTest, "GSTNCombat.Core.Impact.ForceIgnoresSurfaceFlag", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ImpactForceIgnoresSurfaceFlagTest::RunTest(const FString& Parameters)
{
	using namespace GSTNImpactTest;
	TStrongObjectPtr<UGSTN_SurfaceImpactTable> Table = MakeTable();
	const FGSTN_ImpactResponse Anyone;
	const int32 Crate = 2; // allows no outcome at all

	FGSTN_ImpactProfile Profile = Forcing(EGSTN_ImpactOutcome::Splat);
	TestEqual(TEXT("a surface that allows nothing refuses a force"), Outcome(Table.Get(), Crate, 1000.0, Anyone, Profile), EGSTN_ImpactOutcome::None);
	Profile.bForceIgnoresSurface = true;
	TestEqual(TEXT("bForceIgnoresSurface forces it anyway"), Outcome(Table.Get(), Crate, 1000.0, Anyone, Profile), EGSTN_ImpactOutcome::Splat);
	// The speed threshold still holds
	TestEqual(TEXT("below the crate's threshold: none"), Outcome(Table.Get(), Crate, 400.0, Anyone, Profile), EGSTN_ImpactOutcome::None);

	// A forced outcome the surface does not list is allowed while the surface allows anything (glass, force splat)
	TestEqual(TEXT("glass allows something, so a force passes"), Outcome(Table.Get(), 1, 1000.0, Anyone, Forcing(EGSTN_ImpactOutcome::Splat)), EGSTN_ImpactOutcome::Splat);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ImpactMinSpeedGivesNoneTest, "GSTNCombat.Core.Impact.MinSpeedGivesNone", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ImpactMinSpeedGivesNoneTest::RunTest(const FString& Parameters)
{
	using namespace GSTNImpactTest;
	TStrongObjectPtr<UGSTN_SurfaceImpactTable> Table = MakeTable();
	const FGSTN_ImpactResponse Anyone;
	const FGSTN_ImpactProfile Nothing;

	TestEqual(TEXT("exactly the threshold is not enough"), Outcome(Table.Get(), 0, 300.0, Anyone, Nothing), EGSTN_ImpactOutcome::None);
	TestEqual(TEXT("one above is"), Outcome(Table.Get(), 0, 301.0, Anyone, Nothing), EGSTN_ImpactOutcome::Splat);
	TestEqual(TEXT("the input is quantised to whole cm/s"), Outcome(Table.Get(), 0, 300.4, Anyone, Nothing), EGSTN_ImpactOutcome::None);

	// The victim can raise the threshold, never lower the surface's
	FGSTN_ImpactResponse Sturdy;
	Sturdy.Default.MinImpactSpeed = 500.f;
	TestEqual(TEXT("sturdy victim, 400"), Outcome(Table.Get(), 0, 400.0, Sturdy, Nothing), EGSTN_ImpactOutcome::None);
	TestEqual(TEXT("sturdy victim, 501"), Outcome(Table.Get(), 0, 501.0, Sturdy, Nothing), EGSTN_ImpactOutcome::Splat);
	FGSTN_ImpactResponse Soft;
	Soft.Default.MinImpactSpeed = 10.f;
	TestEqual(TEXT("a lower victim threshold does not lower the surface's"), Outcome(Table.Get(), 0, 200.0, Soft, Nothing), EGSTN_ImpactOutcome::None);

	// The attack's SpeedScale scales the speed it is judged with
	FGSTN_ImpactProfile Hard;
	Hard.SpeedScale = 2.f;
	TestEqual(TEXT("scaled speed 400 > 300"), Outcome(Table.Get(), 0, 200.0, Anyone, Hard), EGSTN_ImpactOutcome::Splat);
	FGSTN_ImpactProfile Weak;
	Weak.SpeedScale = 0.5f;
	TestEqual(TEXT("scaled speed 250 < 300"), Outcome(Table.Get(), 0, 500.0, Anyone, Weak), EGSTN_ImpactOutcome::None);
	TestEqual(TEXT("the judged speed is reported"), UGSTN_ImpactMath::Resolve(InputOf(Table.Get(), 0, 500.0), Table.Get(), Anyone, Weak).JudgedSpeed, 250.f);

	// Glass needs more than a wall
	TestEqual(TEXT("glass at 500: none"), Outcome(Table.Get(), 1, 500.0, Anyone, Nothing), EGSTN_ImpactOutcome::None);
	TestEqual(TEXT("glass at 700: breaks"), Outcome(Table.Get(), 1, 700.0, Anyone, Nothing), EGSTN_ImpactOutcome::BreakSurface);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ImpactTrampolineRowForcesBounceTest, "GSTNCombat.Core.Impact.TrampolineRowForcesBounce", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ImpactTrampolineRowForcesBounceTest::RunTest(const FString& Parameters)
{
	using namespace GSTNImpactTest;
	TStrongObjectPtr<UGSTN_SurfaceImpactTable> Table = MakeTable();
	const int32 Trampoline = 3;

	FGSTN_ImpactResponse NoBounceVictim;
	NoBounceVictim.Default.Allowed.bBounce = false;
	FGSTN_ImpactProfile Hostile = Forcing(EGSTN_ImpactOutcome::Splat);
	Hostile.Forbid = { EGSTN_ImpactOutcome::Bounce };

	TestEqual(TEXT("bounces whatever the victim and the attack prefer"), Outcome(Table.Get(), Trampoline, 100.0, NoBounceVictim, Hostile), EGSTN_ImpactOutcome::Bounce);
	const FGSTN_ImpactResult Result = UGSTN_ImpactMath::Resolve(InputOf(Table.Get(), 3, 100.0), Table.Get(), FGSTN_ImpactResponse(), FGSTN_ImpactProfile());
	TestEqual(TEXT("the trampoline's own restitution"), Result.Restitution, 1.2f);
	TestEqual(TEXT("no speed, no impact"), Outcome(Table.Get(), Trampoline, 0.0, FGSTN_ImpactResponse(), FGSTN_ImpactProfile()), EGSTN_ImpactOutcome::None);

	// Row lookup: exact tag, unknown tag, invalid tag
	TestEqual(TEXT("trampoline index"), Table->FindRowIndex(Surface(TEXT("Trampoline"))), 3);
	TestEqual(TEXT("an invalid tag finds nothing"), Table->FindRowIndex(FGameplayTag()), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("a root tag finds nothing"), Table->FindRowIndex(GSTNSurfaceTags::Root_Type.GetTag()), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_ImpactGoldenMatrixTest, "GSTNCombat.Core.Impact.GoldenMatrix", GSTN_CORE_TEST_FLAGS)
bool FGSTN_ImpactGoldenMatrixTest::RunTest(const FString& Parameters)
{
	using namespace GSTNImpactTest;
	TStrongObjectPtr<UGSTN_SurfaceImpactTable> Table = MakeTable();
	const TCHAR* SurfaceNames[4] = { TEXT("Wall"), TEXT("Glass"), TEXT("Crate"), TEXT("Trampoline") };

	// Profiles P0..P4: nothing, force splat, force bounce, forbid everything, force splat ignoring the surface
	TArray<FGSTN_ImpactProfile> Profiles;
	Profiles.Add(FGSTN_ImpactProfile());
	Profiles.Add(Forcing(EGSTN_ImpactOutcome::Splat));
	Profiles.Add(Forcing(EGSTN_ImpactOutcome::Bounce));
	FGSTN_ImpactProfile ForbidAll;
	ForbidAll.Forbid = { EGSTN_ImpactOutcome::Splat, EGSTN_ImpactOutcome::Bounce, EGSTN_ImpactOutcome::BreakSurface };
	Profiles.Add(ForbidAll);
	FGSTN_ImpactProfile Ignoring = Forcing(EGSTN_ImpactOutcome::Splat);
	Ignoring.bForceIgnoresSurface = true;
	Profiles.Add(Ignoring);

	// Victims V0: anything goes, V1: never splats
	const FGSTN_ImpactResponse Victims[2] = { FGSTN_ImpactResponse(), NoSplatVictim() };

	FString Matrix;
	for (int32 SurfaceIndex = 0; SurfaceIndex < 4; ++SurfaceIndex)
	{
		for (int32 VictimIndex = 0; VictimIndex < 2; ++VictimIndex)
		{
			Matrix += FString::Printf(TEXT("%s V%d:"), SurfaceNames[SurfaceIndex], VictimIndex);
			for (const FGSTN_ImpactProfile& Profile : Profiles)
			{
				switch (Outcome(Table.Get(), SurfaceIndex, 1000.0, Victims[VictimIndex], Profile))
				{
				case EGSTN_ImpactOutcome::Splat: Matrix += TEXT(" S"); break;
				case EGSTN_ImpactOutcome::Bounce: Matrix += TEXT(" B"); break;
				case EGSTN_ImpactOutcome::BreakSurface: Matrix += TEXT(" K"); break;
				default: Matrix += TEXT(" -"); break;
				}
			}
			Matrix += TEXT("\n");
		}
	}

	// Worked out by hand from the rules (speed 1000 clears every threshold), then pinned
	const FString Golden =
		TEXT("Wall V0: S S B - S\n")
		TEXT("Wall V1: - S B - S\n")
		TEXT("Glass V0: K S B - S\n")
		TEXT("Glass V1: K S B - S\n")
		TEXT("Crate V0: - - - - S\n")
		TEXT("Crate V1: - - - - S\n")
		TEXT("Trampoline V0: B B B B B\n")
		TEXT("Trampoline V1: B B B B B\n");
	TestEqual(TEXT("the resolved outcome matrix"), Matrix, Golden);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
