// Pushbox (2026-11-15, pushbox plan): resolution order and the soft capped push of FGSTN_Pushbox, pure.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Timeline/GSTN_Pushbox.h"
#include "Timeline/GSTN_TimelineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPushboxTests
{
	const FGSTN_BodyCapsuleShape Standing = FGSTN_BodyCapsuleShape::Standing(90.f, 35.f);

	FGSTN_PushboxBody Body(int32 Id, const FVector& Center, float Radius = 28.f, float HalfHeight = 90.f, bool bFrozen = false)
	{
		FGSTN_PushboxBody Out;
		Out.EntityId = Id;
		Out.Cylinder.Center = Center;
		Out.Cylinder.Radius = Radius;
		Out.Cylinder.HalfHeight = HalfHeight;
		Out.bFrozen = bFrozen;
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PushboxResolveTest, "GSTNCombat.Core.Pushbox.Resolve", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PushboxResolveTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPushboxTests;
	const FGSTN_PushboxSettings Settings;
	FGSTN_BodyCapsuleShape Local;

	TestTrue(TEXT("no timeline: from the capsule"), FGSTN_Pushbox::ResolveLocal(nullptr, INDEX_NONE, Standing, Settings, Local));
	TestEqual(TEXT("radius x0.8"), Local.Radius, 28.f);
	TestEqual(TEXT("capsule half height"), Local.HalfHeight, 90.f);
	TestTrue(TEXT("capsule centre"), Local.Offset.Equals(Standing.Offset));

	// A body capsule (dair torso) gives the pushbox its place with no data of its own
	FGSTN_BodyCapsuleShape Torso;
	Torso.HalfHeight = 40.f;
	Torso.Radius = 30.f;
	Torso.Offset = FVector(20.0, 0.0, 110.0);
	TestTrue(TEXT("body capsule"), FGSTN_Pushbox::ResolveLocal(nullptr, INDEX_NONE, Torso, Settings, Local));
	TestEqual(TEXT("body capsule radius x0.8"), Local.Radius, 24.f);
	TestTrue(TEXT("body capsule centre"), Local.Offset.Equals(Torso.Offset));

	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = 60;
	Data = UGSTN_TimelineUtils::AddPushboxTrack(Data, 10, 20, true, 50.f, 30.f, true, FVector(0.0, 0.0, 30.0));
	Data = UGSTN_TimelineUtils::AddPushboxTrack(Data, 30, 40, false);
	TestEqual(TEXT("two tracks"), Data.Tracks.Num(), 2);
	TestEqual(TEXT("removed together"), UGSTN_TimelineUtils::RemovePushboxTracks(Data).Tracks.Num(), 0);

	TestTrue(TEXT("track wins over the capsule"), FGSTN_Pushbox::ResolveLocal(&Data, 15, Torso, Settings, Local));
	TestEqual(TEXT("track radius"), Local.Radius, 50.f);
	TestEqual(TEXT("track half height"), Local.HalfHeight, 30.f);
	TestTrue(TEXT("track offset"), Local.Offset.Equals(FVector(0.0, 0.0, 30.0)));
	TestFalse(TEXT("disabled track: none"), FGSTN_Pushbox::ResolveLocal(&Data, 35, Torso, Settings, Local));
	TestTrue(TEXT("between tracks: the capsule"), FGSTN_Pushbox::ResolveLocal(&Data, 25, Torso, Settings, Local) && FMath::IsNearlyEqual(Local.Radius, 24.f));

	FGSTN_PushboxSettings Off;
	Off.bEnabled = false;
	TestFalse(TEXT("settings off: none"), FGSTN_Pushbox::ResolveLocal(nullptr, INDEX_NONE, Standing, Off, Local));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PushboxPushTest, "GSTNCombat.Core.Pushbox.Push", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PushboxPushTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPushboxTests;
	const FGSTN_PushboxSettings Settings;
	const FVector Up = FVector::UpVector;

	// Apart: 60 cm between centres, radii 28 + 28 = 56
	TestTrue(TEXT("no overlap: zero"), FGSTN_Pushbox::PairShare(Body(1, FVector::ZeroVector), Body(2, FVector(60.0, 0.0, 0.0)), Settings, Up).IsZero());
	// Stacked: vertical spans do not meet
	TestTrue(TEXT("vertical apart: zero"), FGSTN_Pushbox::PairShare(Body(1, FVector::ZeroVector), Body(2, FVector(5.0, 0.0, 181.0)), Settings, Up).IsZero());

	// 4 cm overlap: share 0.25 * 4 / 2 = 0.5 cm, away from the other
	const FGSTN_PushboxBody A = Body(1, FVector::ZeroVector);
	const FGSTN_PushboxBody B = Body(2, FVector(52.0, 0.0, 30.0));
	const FVector ShareA = FGSTN_Pushbox::PairShare(A, B, Settings, Up);
	const FVector ShareB = FGSTN_Pushbox::PairShare(B, A, Settings, Up);
	TestTrue(TEXT("A pushed away along -X"), ShareA.Equals(FVector(-0.5, 0.0, 0.0), 1e-4));
	TestTrue(TEXT("A and B opposite"), (ShareA + ShareB).IsNearlyZero(1e-6));
	TestEqual(TEXT("horizontal only"), ShareA.Z, 0.0);

	// Deep overlap (P4): the cap grows with the overlap, so the push grows with it. 46 cm: gain share 5.75 under the cap 14.5 / 2
	const FVector Deep = FGSTN_Pushbox::PairShare(A, Body(2, FVector(10.0, 0.0, 0.0)), Settings, Up);
	TestTrue(TEXT("deep pair share grows with the overlap"), FMath::IsNearlyEqual(Deep.Size(), Settings.PushGainPerFrame * 46.0 * 0.5, 1e-4));
	FGSTN_PushboxSettings OldCap = Settings;
	OldCap.PushCapGrowth = 0.f;
	TestTrue(TEXT("growth 0: the P1 fixed cap"), FMath::IsNearlyEqual(FGSTN_Pushbox::PairShare(A, Body(2, FVector(10.0, 0.0, 0.0)), OldCap, Up).Size(), OldCap.MaxPushPerFrame * 0.5, 1e-4));
	const TArray<FGSTN_PushboxBody> Crowd = { Body(2, FVector(10.0, 0.0, 0.0)), Body(3, FVector(10.0, 1.0, 0.0)), Body(4, FVector(10.0, -1.0, 0.0)) };
	TestTrue(TEXT("sum capped at the grown cap"), FGSTN_Pushbox::ComputePush(A, Crowd, Settings, Up).Size() <= Settings.MaxPushPerFrame + Settings.PushCapGrowth * 47.0 + 1e-4);
	TestTrue(TEXT("own id skipped"), FGSTN_Pushbox::ComputePush(A, TArray<FGSTN_PushboxBody>{ A }, Settings, Up).IsZero());

	// Coincident: opposite along a fixed axis, by id order
	const FVector C1 = FGSTN_Pushbox::PairShare(Body(1, FVector::ZeroVector), Body(2, FVector::ZeroVector), Settings, Up);
	const FVector C2 = FGSTN_Pushbox::PairShare(Body(2, FVector::ZeroVector), Body(1, FVector::ZeroVector), Settings, Up);
	TestFalse(TEXT("coincident pushes"), C1.IsNearlyZero());
	TestTrue(TEXT("coincident opposite"), (C1 + C2).IsNearlyZero(1e-6));

	// Hitstop: no pair with a frozen member pushes
	TestTrue(TEXT("self frozen"), FGSTN_Pushbox::PairShare(Body(1, FVector::ZeroVector, 28.f, 90.f, true), B, Settings, Up).IsZero());
	TestTrue(TEXT("other frozen"), FGSTN_Pushbox::PairShare(A, Body(2, FVector(52.0, 0.0, 0.0), 28.f, 90.f, true), Settings, Up).IsZero());

	// Inactive pushbox (disabled track, KO): nothing
	TestTrue(TEXT("inactive other"), FGSTN_Pushbox::PairShare(A, Body(2, FVector(10.0, 0.0, 0.0), 0.f), Settings, Up).IsZero());

	// Converges: two overlapping bodies stepped with their shares separate, no step over the cap
	FGSTN_PushboxBody P = Body(1, FVector::ZeroVector);
	FGSTN_PushboxBody Q = Body(2, FVector(20.0, 0.0, 0.0));
	double LargestStep = 0.0;
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		const FVector MoveP = FGSTN_Pushbox::ComputePush(P, TArray<FGSTN_PushboxBody>{ Q }, Settings, Up);
		const FVector MoveQ = FGSTN_Pushbox::ComputePush(Q, TArray<FGSTN_PushboxBody>{ P }, Settings, Up);
		LargestStep = FMath::Max(LargestStep, FMath::Max(MoveP.Size(), MoveQ.Size()));
		P.Cylinder.Center += MoveP;
		Q.Cylinder.Center += MoveQ;
	}
	const double Gap = FVector::Dist(P.Cylinder.Center, Q.Cylinder.Center);
	AddInfo(FString::Printf(TEXT("pushbox converge: gap %.2f cm after 120 frames, largest step %.3f cm"), Gap, LargestStep));
	TestTrue(TEXT("separated to within 1 cm"), Gap > 55.0);
	TestTrue(TEXT("no step over the first frame's grown cap"), LargestStep <= (Settings.MaxPushPerFrame + Settings.PushCapGrowth * 36.0) * 0.5 + 1e-4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PushboxStateRulesTest, "GSTNCombat.Core.Pushbox.StateRules", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PushboxStateRulesTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPushboxTests;
	// Pushbox P2 (owner 2A): the default data takes the pushbox off for a downed and a tumbling fighter, by state tag name
	FGSTN_PushboxSettings Settings;
	FGSTN_BodyCapsuleShape Local;
	const FName Knockdown(TEXT("Combat.State.Knockdown"));
	const FName Tumble(TEXT("Combat.State.Tumble"));
	const FName Other(TEXT("Combat.State.Hitstun"));

	TestEqual(TEXT("two default rules"), Settings.StateRules.Num(), 2);
	TestFalse(TEXT("knockdown: none"), FGSTN_Pushbox::ResolveLocal(nullptr, INDEX_NONE, Standing, Settings, TArray<FName>{ Knockdown }, Local));
	TestFalse(TEXT("tumble: none"), FGSTN_Pushbox::ResolveLocal(nullptr, INDEX_NONE, Standing, Settings, TArray<FName>{ Tumble }, Local));
	TestTrue(TEXT("unlisted state: the capsule"), FGSTN_Pushbox::ResolveLocal(nullptr, INDEX_NONE, Standing, Settings, TArray<FName>{ Other }, Local) && FMath::IsNearlyEqual(Local.Radius, 28.f));

	// A rule that keeps a pushbox with its own size; the first matching rule wins
	FGSTN_PushboxStateRule Small;
	Small.StateTag = Other;
	Small.bEnabled = true;
	Small.Radius = 15.f;
	Small.HalfHeight = 25.f;
	Settings.StateRules.Insert(Small, 0);
	TestTrue(TEXT("enabled rule"), FGSTN_Pushbox::ResolveLocal(nullptr, INDEX_NONE, Standing, Settings, TArray<FName>{ Knockdown, Other }, Local));
	TestEqual(TEXT("rule radius"), Local.Radius, 15.f);
	TestEqual(TEXT("rule half height"), Local.HalfHeight, 25.f);

	// A move's Pushbox track wins over the state
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = 60;
	Data = UGSTN_TimelineUtils::AddPushboxTrack(Data, 0, 20, true, 40.f, 30.f, false, FVector::ZeroVector);
	TestTrue(TEXT("track over a downed state"), FGSTN_Pushbox::ResolveLocal(&Data, 5, Standing, Settings, TArray<FName>{ Knockdown }, Local) && FMath::IsNearlyEqual(Local.Radius, 40.f));
	TestFalse(TEXT("past the track: the state"), FGSTN_Pushbox::ResolveLocal(&Data, 30, Standing, Settings, TArray<FName>{ Knockdown }, Local));
	return true;
}
namespace GSTNPushboxTests
{
	/** A body of the P4 tests: pushbox 24 (30 x 0.8), core 15 (30 x 0.5), half height 90, walking at Velocity */
	struct FSimBody
	{
		FGSTN_PushboxBody Body;
		FVector Velocity = FVector::ZeroVector;
	};

	FSimBody SimBody(int32 Id, const FVector& Center, const FVector& Velocity, float Core = 15.f)
	{
		FSimBody Out;
		Out.Body = Body(Id, Center, 24.f, 90.f);
		Out.Body.Cylinder.CoreRadius = Core;
		Out.Velocity = Velocity;
		return Out;
	}

	/**
	 * One 62.5 Hz frame as the game runs it: every body reads the same start (the step snapshot), is pushed (ComputePush), makes its own move, and
	 * ConstrainMove corrects the whole move; then the world clamps it as a swept move would (walls at X = MinX, side walls at |Y| = MaxAbsY)
	 */
	void Step(TArray<FSimBody>& Bodies, const FGSTN_PushboxSettings& Settings, double MinX = -1e9, double MaxAbsY = 1e9)
	{
		const FVector Up = FVector::UpVector;
		const double Dt = 1.0 / 62.5;
		TArray<FGSTN_PushboxBody> Start;
		for (const FSimBody& Sim : Bodies)
		{
			Start.Add(Sim.Body);
		}
		for (int32 Index = 0; Index < Bodies.Num(); ++Index)
		{
			FVector Velocity = Bodies[Index].Velocity;
			const FVector Move = FGSTN_Pushbox::ComputePush(Start[Index], Start, Settings, Up) + Velocity * Dt;
			FVector End = Start[Index].Cylinder.Center + FGSTN_Pushbox::ConstrainMove(Start[Index], Move, Start, Settings, Up, &Velocity);
			End.X = FMath::Max(End.X, MinX);
			End.Y = FMath::Clamp(End.Y, -MaxAbsY, MaxAbsY);
			Bodies[Index].Body.Cylinder.Center = End;
		}
	}

	double SmallestGap(const TArray<FSimBody>& Bodies)
	{
		double Smallest = TNumericLimits<double>::Max();
		for (int32 I = 0; I < Bodies.Num(); ++I)
		{
			for (int32 J = I + 1; J < Bodies.Num(); ++J)
			{
				Smallest = FMath::Min(Smallest, FVector::Dist2D(Bodies[I].Body.Cylinder.Center, Bodies[J].Body.Cylinder.Center));
			}
		}
		return Smallest;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PushboxCoreHeadOnTest, "GSTNCombat.Core.Pushbox.CoreHeadOn", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PushboxCoreHeadOnTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPushboxTests;
	const FGSTN_PushboxSettings Settings;
	// P3 capture v471-v504: one fighter walked through the other (centres 95 -> 4 cm). Now: both run into each other at 600 cm/s
	TArray<FSimBody> Bodies = { SimBody(1, FVector(0.0, 0.0, 90.0), FVector(600.0, 0.0, 0.0)), SimBody(2, FVector(95.0, 0.0, 90.0), FVector(-600.0, 0.0, 0.0)) };
	double Smallest = TNumericLimits<double>::Max();
	bool bCrossed = false;
	for (int32 Frame = 0; Frame < 180; ++Frame)
	{
		Step(Bodies, Settings);
		Smallest = FMath::Min(Smallest, SmallestGap(Bodies));
		bCrossed |= Bodies[0].Body.Cylinder.Center.X >= Bodies[1].Body.Cylinder.Center.X;
	}
	const double Final = SmallestGap(Bodies);
	AddInfo(FString::Printf(TEXT("pushbox core head-on: smallest gap %.2f cm, final %.2f cm (core sum 30, pushbox sum 48)"), Smallest, Final));
	TestFalse(TEXT("never cross"), bCrossed);
	TestTrue(TEXT("cores never enter"), Smallest >= 30.0 - 1e-3);
	TestTrue(TEXT("both moved equally (symmetric)"), FMath::IsNearlyEqual(Bodies[0].Body.Cylinder.Center.X, 95.0 - Bodies[1].Body.Cylinder.Center.X, 1e-3));

	// One walks into a standing one: the standing one is shoved a bit, the walker slows and stops at the core
	TArray<FSimBody> Shove = { SimBody(1, FVector(0.0, 0.0, 90.0), FVector(400.0, 0.0, 0.0)), SimBody(2, FVector(95.0, 0.0, 90.0), FVector::ZeroVector) };
	double ShoveSmallest = TNumericLimits<double>::Max();
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Step(Shove, Settings);
		ShoveSmallest = FMath::Min(ShoveSmallest, SmallestGap(Shove));
	}
	const double Shoved = Shove[1].Body.Cylinder.Center.X - 95.0;
	AddInfo(FString::Printf(TEXT("pushbox core walk-in: smallest gap %.2f cm, standing one shoved %.2f cm in 60 frames"), ShoveSmallest, Shoved));
	TestTrue(TEXT("walk-in: core holds"), ShoveSmallest >= 30.0 - 1e-3);
	TestTrue(TEXT("walk-in: the standing one is shoved"), Shoved > 1.0);

	// Pushbox only (no core on one side): the approach clamp alone, the walker still slows down
	const FGSTN_PushboxBody SoftWalker = SimBody(1, FVector(0.0, 0.0, 90.0), FVector::ZeroVector, 0.f).Body;
	const FGSTN_PushboxBody Target = SimBody(2, FVector(24.0, 0.0, 90.0), FVector::ZeroVector).Body;
	FVector Velocity(400.0, 0.0, 0.0);
	const FVector SoftMove = FGSTN_Pushbox::ConstrainMove(SoftWalker, FVector(6.4, 0.0, 0.0), TArray<FGSTN_PushboxBody>{ Target }, Settings, FVector::UpVector, &Velocity);
	TestTrue(TEXT("pushbox only: still moves in (no core)"), SoftMove.X > 0.0);
	TestTrue(TEXT("pushbox only: halfway in, 60% kept"), FMath::IsNearlyEqual(SoftMove.X, 6.4 * FMath::Lerp(0.2, 1.0, 0.5), 1e-4));
	TestTrue(TEXT("pushbox only: the velocity slows the same"), FMath::IsNearlyEqual(Velocity.X, 400.0 * 0.6, 1e-3));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PushboxCoreCorridorTest, "GSTNCombat.Core.Pushbox.CoreCorridor", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PushboxCoreCorridorTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPushboxTests;
	const FGSTN_PushboxSettings Settings;
	// Owner worry (P4 b): four bodies in a 140 cm corridor all running at the end wall. A 30 cm capsule's centre stops at X 30, side walls at |Y| 40
	const double WallX = 30.0;
	const double SideY = 40.0;
	TArray<FSimBody> Bodies = {
		SimBody(1, FVector(60.0, 1.0, 90.0), FVector(-600.0, 0.0, 0.0)),
		SimBody(2, FVector(110.0, -2.0, 90.0), FVector(-600.0, 0.0, 0.0)),
		SimBody(3, FVector(160.0, 3.0, 90.0), FVector(-600.0, 0.0, 0.0)),
		SimBody(4, FVector(210.0, 0.0, 90.0), FVector(-600.0, 0.0, 0.0)) };
	double Smallest = TNumericLimits<double>::Max();
	double LateJitter = 0.0;
	bool bThroughWall = false;
	for (int32 Frame = 0; Frame < 400; ++Frame)
	{
		TArray<FVector> Before;
		for (const FSimBody& Sim : Bodies)
		{
			Before.Add(Sim.Body.Cylinder.Center);
		}
		Step(Bodies, Settings, WallX, SideY);
		Smallest = FMath::Min(Smallest, SmallestGap(Bodies));
		for (int32 Index = 0; Index < Bodies.Num(); ++Index)
		{
			bThroughWall |= Bodies[Index].Body.Cylinder.Center.X < WallX - 1e-6;
			if (Frame >= 340)
			{
				LateJitter = FMath::Max(LateJitter, FVector::Dist(Before[Index], Bodies[Index].Body.Cylinder.Center));
			}
		}
	}
	FString Places;
	for (const FSimBody& Sim : Bodies)
	{
		Places += FString::Printf(TEXT(" (%.1f,%.1f)"), Sim.Body.Cylinder.Center.X, Sim.Body.Cylinder.Center.Y);
	}
	AddInfo(FString::Printf(TEXT("pushbox core corridor: smallest gap %.2f cm, last-60-frame largest step %.3f cm, places%s"), Smallest, LateJitter, *Places));
	TestFalse(TEXT("nobody through the wall"), bThroughWall);
	TestTrue(TEXT("cores hold in the crowd (1 cm slack for the wall clamp)"), Smallest >= 29.0);
	TestTrue(TEXT("settled: no jitter over 0.5 cm per frame"), LateJitter <= 0.5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PushboxCoreHeadLandingTest, "GSTNCombat.Core.Pushbox.CoreHeadLanding", GSTN_CORE_TEST_FLAGS)
bool FGSTN_PushboxCoreHeadLandingTest::RunTest(const FString& Parameters)
{
	using namespace GSTNPushboxTests;
	const FGSTN_PushboxSettings Settings;
	const FVector Up = FVector::UpVector;
	// Owner question (P4 c): a body falling onto a standing one's head, 5 cm off centre. The core never touches the vertical: no standing on heads
	// (the movement capsules ignore each other), the faller drops and the two part sideways a few cm per frame, no pop
	FGSTN_PushboxBody Faller = SimBody(1, FVector(5.0, 0.0, 300.0), FVector::ZeroVector).Body;
	const FGSTN_PushboxBody Below = SimBody(2, FVector(0.0, 0.0, 90.0), FVector::ZeroVector).Body;
	double LargestSide = 0.0;
	bool bVerticalTouched = false;
	for (int32 Frame = 0; Frame < 40; ++Frame)
	{
		const FVector Fall(0.0, 0.0, -10.0);
		const FVector Move = FGSTN_Pushbox::ConstrainMove(Faller, Fall, TArray<FGSTN_PushboxBody>{ Below }, Settings, Up);
		bVerticalTouched |= !FMath::IsNearlyEqual(Move.Z, Fall.Z, 1e-6);
		LargestSide = FMath::Max(LargestSide, FVector::Dist2D(Move, Fall));
		Faller.Cylinder.Center += Move;
		Faller.Cylinder.Center.Z = FMath::Max(Faller.Cylinder.Center.Z, 90.0);
	}
	AddInfo(FString::Printf(TEXT("pushbox core head landing: faller ends at (%.1f, %.1f), largest sideways step %.2f cm"), Faller.Cylinder.Center.X, Faller.Cylinder.Center.Z, LargestSide));
	TestFalse(TEXT("the core never changes the vertical"), bVerticalTouched);
	TestTrue(TEXT("no pop: at most half the recovery per frame"), LargestSide <= Settings.CoreRecoverPerFrame * 0.5 + 1e-4);
	TestTrue(TEXT("it parts sideways"), Faller.Cylinder.Center.X > 5.0);
	return true;
}
#endif