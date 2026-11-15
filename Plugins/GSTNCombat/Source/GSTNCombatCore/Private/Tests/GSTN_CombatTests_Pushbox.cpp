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

	// Deep overlap: capped at half the cap per pair, the sum at the cap
	const FVector Deep = FGSTN_Pushbox::PairShare(A, Body(2, FVector(10.0, 0.0, 0.0)), Settings, Up);
	TestTrue(TEXT("pair share capped at cap/2"), FMath::IsNearlyEqual(Deep.Size(), Settings.MaxPushPerFrame * 0.5, 1e-4));
	const TArray<FGSTN_PushboxBody> Crowd = { Body(2, FVector(10.0, 0.0, 0.0)), Body(3, FVector(10.0, 1.0, 0.0)), Body(4, FVector(10.0, -1.0, 0.0)) };
	TestTrue(TEXT("sum capped at the cap"), FGSTN_Pushbox::ComputePush(A, Crowd, Settings, Up).Size() <= Settings.MaxPushPerFrame + 1e-4);
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
	TestTrue(TEXT("no step over the cap"), LargestStep <= Settings.MaxPushPerFrame * 0.5 + 1e-4);
	return true;
}

#endif
