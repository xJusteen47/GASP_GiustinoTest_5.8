// Characterization tests of the shared wall maths as it is today (wall-slide-v2.md W0): GSTN.Wall.Utils.*
//
// They pin the behaviour of the functions the v2 modes reuse, so a later change to them is deliberate. Pure: no world.

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/HitResult.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "WallMovement/MoveLibrary/GSTN_WallMovementUtils.h"
#include "WallMovement/MoveLibrary/GSTN_WallQueryUtils.h"
#include "WallMovement/GSTN_WallMovementDebug.h"

/** Double-safe tolerance compare (FVector2D components are double, the literals float: TestNearlyEqual is ambiguous) */
static void UtilsNearlyEq(FAutomationTestBase& Test, const TCHAR* What, double A, double B, double Tolerance = 1e-4)
{
	Test.TestTrue(What, FMath::Abs(A - B) <= Tolerance);
}

namespace GSTNWallUtilsCharTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Gravity-up vectors including diagonal ones, so nothing is secretly axis aligned */
	static TArray<FVector> UpVectors()
	{
		return { FVector(0, 0, 1), FVector(0, 0, -1), FVector(1, 0, 0), FVector(0, -1, 0), FVector(1, 1, 1).GetSafeNormal(), FVector(-2, 1, 3).GetSafeNormal() };
	}

	/** A unit vector perpendicular to V */
	static FVector Perp(const FVector& V)
	{
		const FVector Seed = FMath::Abs(V.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
		return FVector::CrossProduct(V, Seed).GetSafeNormal();
	}

	static float AngleDeg(const FVector& A, const FVector& B)
	{
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(A, B), -1.f, 1.f)));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallUtilsFilterNormalTest, "GSTN.Wall.Utils.FilterWallNormalDeadzoneAndRate", GSTNWallUtilsCharTest::TestFlags)
bool FGSTN_WallUtilsFilterNormalTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallUtilsCharTest;
	for (const FVector& Up : UpVectors())
	{
		const FVector Current = Perp(Up);
		const FVector Axis = FVector::CrossProduct(Current, Up).GetSafeNormal();
		auto Rotated = [&](float Deg) { return FQuat(Axis, FMath::DegreesToRadians(Deg)).RotateVector(Current); };

		// Inside the deadzone the response is quadratic: 1 deg of a 4 deg deadzone moves (1/4)^2 of it
		const float Small = AngleDeg(Current, UGSTN_WallMovementUtils::FilterWallNormal(Current, Rotated(1.f), 4.f, 0.f, 0.016f));
		UtilsNearlyEq(*this, TEXT("Deadzone shrinks small changes"), Small, 1.f / 16.f, 0.01f);

		// At or beyond the deadzone the change passes whole when the rate is unlimited
		const float Whole = AngleDeg(Current, UGSTN_WallMovementUtils::FilterWallNormal(Current, Rotated(20.f), 4.f, 0.f, 0.016f));
		UtilsNearlyEq(*this, TEXT("Beyond the deadzone passes whole"), Whole, 20.f, 0.05f);

		// The rate limit caps the turn per tick: 90 deg/s over 0.1 s = 9 deg
		const float Limited = AngleDeg(Current, UGSTN_WallMovementUtils::FilterWallNormal(Current, Rotated(40.f), 4.f, 90.f, 0.1f));
		UtilsNearlyEq(*this, TEXT("Rate limits the turn"), Limited, 9.f, 0.05f);

		// No deadzone: the change goes through (blend 1)
		const float NoDeadzone = AngleDeg(Current, UGSTN_WallMovementUtils::FilterWallNormal(Current, Rotated(0.5f), 0.f, 0.f, 0.016f));
		UtilsNearlyEq(*this, TEXT("Zero deadzone passes"), NoDeadzone, 0.5f, 0.01f);

		TestTrue(TEXT("Result stays unit"), UGSTN_WallMovementUtils::FilterWallNormal(Current, Rotated(40.f), 4.f, 90.f, 0.1f).IsNormalized());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallUtilsTransportTest, "GSTN.Wall.Utils.TransportVelocityKeepsSpeed", GSTNWallUtilsCharTest::TestFlags)
bool FGSTN_WallUtilsTransportTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallUtilsCharTest;
	for (const FVector& Up : UpVectors())
	{
		const FVector From = Perp(Up);
		const FVector Axis = FVector::CrossProduct(From, Up).GetSafeNormal();
		const FVector To = FQuat(Axis, FMath::DegreesToRadians(35.f)).RotateVector(From);
		const FVector Velocity = Up * -450.f + FVector::CrossProduct(From, Up).GetSafeNormal() * 200.f;

		const FVector Transported = UGSTN_WallMovementUtils::TransportVelocity(Velocity, From, To);
		UtilsNearlyEq(*this, TEXT("Speed kept"), Transported.Size(), Velocity.Size(), 0.1f);
		UtilsNearlyEq(*this, TEXT("Lies on the new plane"), FVector::DotProduct(Transported, To), 0.f, 0.1f);

		const FVector Same = UGSTN_WallMovementUtils::TransportVelocity(Velocity, From, From);
		TestTrue(TEXT("Same normal is identity"), Same.Equals(Velocity, 0.1f));
	}
	TestTrue(TEXT("Zero stays zero"), UGSTN_WallMovementUtils::TransportVelocity(FVector::ZeroVector, FVector::XAxisVector, FVector::YAxisVector).IsZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallUtilsAxisSpeedTest, "GSTN.Wall.Utils.AxisSpeedBrakingOnly", GSTNWallUtilsCharTest::TestFlags)
bool FGSTN_WallUtilsAxisSpeedTest::RunTest(const FString& Parameters)
{
	// No input (target 0): proportional + constant braking toward zero, never past it, snapping below StopSpeed
	float Speed = 300.f;
	float Previous = Speed;
	bool bReachedZero = false;
	for (int32 i = 0; i < 600; ++i)
	{
		Speed = UGSTN_WallMovementUtils::ComputeAxisSpeed(Speed, 0.f, 500.f, 8.f, 2.f, 200.f, 10.f, 1.f / 60.f);
		TestTrue(TEXT("Monotonic braking"), Speed <= Previous + UE_KINDA_SMALL_NUMBER);
		TestTrue(TEXT("Never flips sign"), Speed >= 0.f);
		Previous = Speed;
		if (Speed == 0.f)
		{
			bReachedZero = true;
			break;
		}
	}
	TestTrue(TEXT("Reaches exactly zero"), bReachedZero);

	// Negative speeds mirror
	TestTrue(TEXT("Negative mirrors"), UGSTN_WallMovementUtils::ComputeAxisSpeed(-300.f, 0.f, 500.f, 8.f, 2.f, 200.f, 10.f, 0.1f) > -300.f);
	TestEqual(TEXT("Zero delta time is unchanged"), UGSTN_WallMovementUtils::ComputeAxisSpeed(123.f, 0.f, 500.f, 8.f, 2.f, 200.f, 10.f, 0.f), 123.f);

	// Entry momentum above the target speed is braked back to it, never below
	float Fast = 900.f;
	for (int32 i = 0; i < 600; ++i)
	{
		Fast = UGSTN_WallMovementUtils::ComputeAxisSpeed(Fast, 400.f, 500.f, 0.f, 2.f, 200.f, 10.f, 1.f / 60.f);
		TestTrue(TEXT("Not below target"), Fast >= 400.f - UE_KINDA_SMALL_NUMBER);
	}
	UtilsNearlyEq(*this, TEXT("Settles at target"), Fast, 400.f, 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallUtilsFloorHitTest, "GSTN.Wall.Utils.IsFloorHitGravityRelative", GSTNWallUtilsCharTest::TestFlags)
bool FGSTN_WallUtilsFloorHitTest::RunTest(const FString& Parameters)
{
	using namespace GSTNWallUtilsCharTest;
	for (const FVector& Up : UpVectors())
	{
		const FVector Side = Perp(Up);
		const FVector Axis = FVector::CrossProduct(Up, Side).GetSafeNormal();
		auto Tilted = [&](float Deg) { return FQuat(Axis, FMath::DegreesToRadians(Deg)).RotateVector(Up); };

		FHitResult Hit;
		Hit.ImpactNormal = Up;
		TestTrue(TEXT("Flat floor, moving down"), UGSTN_WallQueryUtils::IsFloorHit(Hit, -Up * 10.f, Up, 45.f));
		TestFalse(TEXT("Moving up is not a floor hit"), UGSTN_WallQueryUtils::IsFloorHit(Hit, Up * 10.f, Up, 45.f));
		TestFalse(TEXT("Moving along is not a floor hit"), UGSTN_WallQueryUtils::IsFloorHit(Hit, Side * 10.f, Up, 45.f));

		Hit.ImpactNormal = Tilted(30.f);
		TestTrue(TEXT("30 deg slope within 45"), UGSTN_WallQueryUtils::IsFloorHit(Hit, -Up, Up, 45.f));
		Hit.ImpactNormal = Tilted(60.f);
		TestFalse(TEXT("60 deg slope beyond 45"), UGSTN_WallQueryUtils::IsFloorHit(Hit, -Up, Up, 45.f));

		Hit.ImpactNormal = Side;
		TestFalse(TEXT("Wall is not a floor"), UGSTN_WallQueryUtils::IsFloorHit(Hit, -Up, Up, 45.f));
	}
	return true;
}

#if GSTN_WALL_DEBUG
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallDebugQueryCounterTest, "GSTN.Wall.Debug.QueryCounter", GSTNWallUtilsCharTest::TestFlags)
bool FGSTN_WallDebugQueryCounterTest::RunTest(const FString& Parameters)
{
	IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("gstn.Wall.CountQueries"));
	if (!TestNotNull(TEXT("CVar exists"), Var))
	{
		return false;
	}
	const int32 Old = Var->GetInt();
	GSTNWallDebug::ResetQueryStats();

	Var->Set(0);
	{ GSTN_WALL_QUERY_SCOPE(); }
	TestEqual(TEXT("Disabled counts nothing"), GSTNWallDebug::GetTickQueryCount(), 0);

	Var->Set(1);
	const FName Mode(TEXT("GSTNTestMode"));
	for (int32 Tick = 0; Tick < 4; ++Tick)
	{
		for (int32 i = 0; i < Tick + 1; ++i)
		{
			GSTN_WALL_QUERY_SCOPE();
		}
		GSTNWallDebug::FinishTick(Mode, Tick * 0.016, 100.f, 0.f);
	}
	int32 Ticks, Max;
	double Avg;
	GSTNWallDebug::GetQueryStats(Mode, Ticks, Avg, Max);
	TestEqual(TEXT("Ticks"), Ticks, 4);
	TestEqual(TEXT("Max"), Max, 4);
	UtilsNearlyEq(*this, TEXT("Average"), (float)Avg, 2.5f);
	TestEqual(TEXT("Tick counter resets"), GSTNWallDebug::GetTickQueryCount(), 0);

	GSTNWallDebug::ResetQueryStats();
	Var->Set(Old);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallDebugMetricsCsvTest, "GSTN.Wall.Debug.MetricsCsv", GSTNWallUtilsCharTest::TestFlags)
bool FGSTN_WallDebugMetricsCsvTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Header"), GSTNWallDebug::MetricsCsvHeader(), FString(TEXT("time,mode,speed,hspeed,vspeed,dnormal_deg,queries,edge_ahead,events,x,y,z,source")));

	GSTNWallDebug::FMetricsRow Row;
	Row.Time = 1.25;
	Row.Mode = TEXT("WallSlidingV2");
	Row.Speed = 500.f;
	Row.HorizontalSpeed = 300.f;
	Row.VerticalSpeed = -400.f;
	Row.NormalChangeDeg = 12.5f;
	Row.Queries = 3;
	Row.EdgeAhead = EGSTN_WallEdgeKind::Convex;
	Row.Events = GSTNWallDebug::Ev_Grab | GSTNWallDebug::Ev_WallLost;
	Row.Location = FVector(10.0, -20.5, 300.0);
	Row.Source = TEXT("Pawn,1@client");
	TestEqual(TEXT("Row"), GSTNWallDebug::FormatMetricsRow(Row), FString(TEXT("1.250,WallSlidingV2,500.0,300.0,-400.0,12.50,3,convex,grab|wall-lost,10.0,-20.5,300.0,Pawn;1@client")));

	Row.Events = GSTNWallDebug::Ev_None;
	Row.EdgeAhead = EGSTN_WallEdgeKind::None;
	TestTrue(TEXT("No events: empty column"), GSTNWallDebug::FormatMetricsRow(Row).Contains(TEXT(",3,none,,")));
	TArray<FString> Cols;
	TestEqual(TEXT("Every column present"), GSTNWallDebug::FormatMetricsRow(Row).ParseIntoArray(Cols, TEXT(","), false), 13);

	const FName A(TEXT("A")), B(TEXT("B"));
	TestTrue(TEXT("First tick of a source is a grab"), GSTNWallDebug::IsGrabTick(false, 0.0, NAME_None, 1.0, A));
	TestFalse(TEXT("Next tick is not"), GSTNWallDebug::IsGrabTick(true, 1.0, A, 1.016, A));
	TestTrue(TEXT("A gap is a new grab"), GSTNWallDebug::IsGrabTick(true, 1.0, A, 1.5, A));
	TestTrue(TEXT("Another mode is a new grab"), GSTNWallDebug::IsGrabTick(true, 1.0, A, 1.016, B));
	TestFalse(TEXT("A re-simulated tick is never a grab"), GSTNWallDebug::IsGrabTick(true, 1.0, A, 0.9, A));
	return true;
}

#endif

#endif // WITH_DEV_AUTOMATION_TESTS
