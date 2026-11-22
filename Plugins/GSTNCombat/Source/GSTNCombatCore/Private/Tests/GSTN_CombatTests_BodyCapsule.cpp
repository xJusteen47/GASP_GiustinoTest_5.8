// Body Capsule (dropkick, 2026-11-15): the per-move movement capsule rules of FGSTN_TimelineTrack_BodyCapsule, pure.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Timeline/GSTN_BodyCapsule.h"
#include "Timeline/GSTN_TimelineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNBodyCapsuleTests
{
	const FGSTN_BodyCapsuleShape Standing = FGSTN_BodyCapsuleShape::Standing(90.f, 35.f);

	// Air [10, 27) on the torso, Body anchor, 20 cm/frame; land [60, 90) on the ground, Capsule anchor
	FGSTN_CombatTimelineData MakeData()
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 92;
		Data = UGSTN_TimelineUtils::AddBodyCapsuleTrack(Data, 10, 27, 35.f, 0.f, FVector(45.0, 0.0, 100.0), EGSTN_BodyCapsuleAnchor::Body, 20.f);
		return UGSTN_TimelineUtils::AddBodyCapsuleTrack(Data, 60, 90, 35.f, 0.f, FVector(10.0, 0.0, 35.0), EGSTN_BodyCapsuleAnchor::Capsule, 20.f);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BodyCapsuleTargetTest, "GSTNCombat.Core.BodyCapsule.Target", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BodyCapsuleTargetTest::RunTest(const FString& Parameters)
{
	using namespace GSTNBodyCapsuleTests;
	const FGSTN_CombatTimelineData Data = MakeData();
	TestEqual(TEXT("two tracks"), Data.Tracks.Num(), 2);
	TestEqual(TEXT("removed together"), UGSTN_TimelineUtils::RemoveBodyCapsuleTracks(Data).Tracks.Num(), 0);

	TestTrue(TEXT("outside a timeline: standing"), FGSTN_BodyCapsule::ResolveTarget(nullptr, 12, Standing).Shape.NearlyEquals(Standing));
	TestTrue(TEXT("frame before the track: standing"), FGSTN_BodyCapsule::ResolveTarget(&Data, 9, Standing).Shape.NearlyEquals(Standing));
	TestTrue(TEXT("INDEX_NONE: standing"), FGSTN_BodyCapsule::ResolveTarget(&Data, INDEX_NONE, Standing).Shape.NearlyEquals(Standing));
	TestTrue(TEXT("between the tracks: standing"), FGSTN_BodyCapsule::ResolveTarget(&Data, 40, Standing).Shape.NearlyEquals(Standing));

	const FGSTN_BodyCapsuleTarget Air = FGSTN_BodyCapsule::ResolveTarget(&Data, 26, Standing);
	TestEqual(TEXT("air half height"), Air.Shape.HalfHeight, 35.f);
	TestEqual(TEXT("radius 0 = the pawn's"), Air.Shape.Radius, 35.f);
	TestTrue(TEXT("air offset"), Air.Shape.Offset.Equals(FVector(45.0, 0.0, 100.0)));
	TestTrue(TEXT("air anchor"), Air.Anchor == EGSTN_BodyCapsuleAnchor::Body);

	const FGSTN_BodyCapsuleTarget Land = FGSTN_BodyCapsule::ResolveTarget(&Data, 60, Standing);
	TestTrue(TEXT("land anchor"), Land.Anchor == EGSTN_BodyCapsuleAnchor::Capsule);
	TestEqual(TEXT("capsule anchor has no step limit"), Land.MaxShiftPerFrame, 0.f);

	FGSTN_CombatTimelineData Thin = UGSTN_TimelineUtils::AddBodyCapsuleTrack(FGSTN_CombatTimelineData(), 0, 5, 10.f, 30.f, FVector::ZeroVector);
	TestEqual(TEXT("half height raised to the radius, as Unreal does"), FGSTN_BodyCapsule::ResolveTarget(&Thin, 0, Standing).Shape.HalfHeight, 30.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BodyCapsuleStepTest, "GSTNCombat.Core.BodyCapsule.Step", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BodyCapsuleStepTest::RunTest(const FString& Parameters)
{
	using namespace GSTNBodyCapsuleTests;
	const FGSTN_CombatTimelineData Data = MakeData();

	// Into the air track: the size at once, the centre 20 cm per frame, the mesh root (actor - offset) never moves
	FGSTN_BodyCapsuleShape Applied = Standing;
	FVector Actor = FVector(0.0, 0.0, 300.0);
	const FVector Root = Actor - Applied.Offset;
	int32 Frames = 0;
	const FGSTN_BodyCapsuleTarget Air = FGSTN_BodyCapsule::ResolveTarget(&Data, 12, Standing);
	while (!Applied.NearlyEquals(Air.Shape) && Frames < 20)
	{
		const FGSTN_BodyCapsuleStep Step = FGSTN_BodyCapsule::Step(Applied, Air);
		TestTrue(TEXT("step within the limit"), Step.ActorShiftLocal.Size() <= 20.0 + 1e-3);
		TestEqual(TEXT("size at once"), Step.Applied.HalfHeight, 35.f);
		Applied = Step.Applied;
		Actor += Step.ActorShiftLocal;
		TestTrue(TEXT("mesh root still"), (Actor - Applied.Offset).Equals(Root, 1e-3));
		++Frames;
	}
	TestEqual(TEXT("(45, 10) cm in 3 frames"), Frames, 3);

	// Landing on the Capsule anchor: the bottom stays (same half height: the actor does not move), the mesh root goes where the land track says
	const FGSTN_BodyCapsuleTarget Land = FGSTN_BodyCapsule::ResolveTarget(&Data, 60, Standing);
	const FGSTN_BodyCapsuleStep Landed = FGSTN_BodyCapsule::Step(Applied, Land);
	TestTrue(TEXT("same half height: actor still"), Landed.ActorShiftLocal.IsNearlyZero());
	TestTrue(TEXT("in one frame"), Landed.Applied.NearlyEquals(Land.Shape));
	const FVector Bottom = Actor - FVector(0.0, 0.0, Applied.HalfHeight);
	TestEqual(TEXT("mesh root on the capsule bottom (offset z = half height)"), (Actor - Landed.Applied.Offset).Z, Bottom.Z, 1e-3);

	// Back to standing (move over): Body anchor, at once, the mesh root stays on the ground and the capsule grows up from it
	Applied = Landed.Applied;
	const FGSTN_BodyCapsuleTarget Stand = FGSTN_BodyCapsule::ResolveTarget(&Data, 91, Standing);
	const FGSTN_BodyCapsuleStep Up = FGSTN_BodyCapsule::Step(Applied, Stand);
	TestTrue(TEXT("standing in one frame"), Up.Applied.NearlyEquals(Standing));
	const FVector NewActor = Actor + Up.ActorShiftLocal;
	TestEqual(TEXT("bottom kept on the ground"), NewActor.Z - Standing.HalfHeight, Bottom.Z, 1e-3);

	// A Capsule anchor with a smaller half height keeps the bottom
	FGSTN_BodyCapsuleTarget Low = Land;
	Low.Shape.HalfHeight = 20.f;
	const FGSTN_BodyCapsuleStep Shrunk = FGSTN_BodyCapsule::Step(Standing, Low);
	TestEqual(TEXT("bottom kept when shrinking"), Shrunk.ActorShiftLocal.Z, -70.0, 1e-3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BodyCapsuleGroundedRestoreTest, "GSTNCombat.Core.BodyCapsule.GroundedRestore", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BodyCapsuleGroundedRestoreTest::RunTest(const FString& Parameters)
{
	// An aerial landed on its small capsule: back to standing on the floor, the bottom stays (actor up by the half height change), not the mesh root
	const FGSTN_BodyCapsuleShape Standing = FGSTN_BodyCapsuleShape::Standing(86.f, 34.f);
	FGSTN_BodyCapsuleShape Small;
	Small.HalfHeight = 33.f;
	Small.Radius = 30.f;
	Small.Offset = FVector(0.0, 0.0, 85.0);
	FGSTN_BodyCapsuleTarget Restore;
	Restore.Shape = Standing;
	const FGSTN_BodyCapsuleTarget OnGround = FGSTN_BodyCapsule::GroundedRestore(Restore, Standing, true);
	TestTrue(TEXT("grounded: capsule anchor"), OnGround.Anchor == EGSTN_BodyCapsuleAnchor::Capsule);
	const FGSTN_BodyCapsuleStep Step = FGSTN_BodyCapsule::Step(Small, OnGround);
	TestEqual(TEXT("the actor rises by the half height change"), Step.ActorShiftLocal.Z, 86.0 - 33.0, 0.01);
	TestTrue(TEXT("standing offset applied"), Step.Applied.NearlyEquals(Standing));
	const FGSTN_BodyCapsuleTarget InAir = FGSTN_BodyCapsule::GroundedRestore(Restore, Standing, false);
	TestTrue(TEXT("airborne: body anchor kept"), InAir.Anchor == EGSTN_BodyCapsuleAnchor::Body);
	FGSTN_BodyCapsuleTarget Track;
	Track.Shape = Small;
	TestTrue(TEXT("a track's shape is left as it is"), FGSTN_BodyCapsule::GroundedRestore(Track, Standing, true).Anchor == EGSTN_BodyCapsuleAnchor::Body);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_BodyCapsuleSmoothingTest, "GSTNCombat.Core.BodyCapsule.SmoothingCorrection", GSTN_CORE_TEST_FLAGS)
bool FGSTN_BodyCapsuleSmoothingTest::RunTest(const FString& Parameters)
{
	// dropkick-capsule-3 f137: grounded restore, actor 37.2 -> 88.2 (shift +51, no other move); the smoothing drew the actor at 73.5
	// (mesh 14.7 cm too low): the correction gives the 14.7 back, so the mesh root is where the simulation put it
	const FVector Start(0.0, 0.0, 37.2);
	const FVector End(0.0, 0.0, 88.2);
	const FVector Shift(0.0, 0.0, 51.0);
	TestEqual(TEXT("restore: the missing part of the shift"), FGSTN_BodyCapsule::SmoothingCorrection(FVector(0.0, 0.0, 73.5), Start, End, Shift).Z, 14.7, 0.01);
	TestEqual(TEXT("presented at the frame's start: the whole shift"), FGSTN_BodyCapsule::SmoothingCorrection(Start, Start, End, Shift).Z, 51.0, 0.01);
	TestTrue(TEXT("presented at the frame's end: nothing"), FGSTN_BodyCapsule::SmoothingCorrection(End, Start, End, Shift).IsNearlyZero(0.01));
	TestTrue(TEXT("no shift this frame: nothing"), FGSTN_BodyCapsule::SmoothingCorrection(FVector(0.0, 0.0, 50.0), Start, End, FVector::ZeroVector).IsNearlyZero());

	// A falling frame with a shift: 10 cm of fall plus a 20 cm body-anchor shift up; half way through, half the shift is missing
	const FVector FallStart(0.0, 0.0, 200.0);
	const FVector FallEnd(0.0, 0.0, 210.0);
	const FVector Presented = FallStart + (FallEnd - FallStart) * 0.5;
	TestEqual(TEXT("mixed move: (1 - alpha) of the shift"), FGSTN_BodyCapsule::SmoothingCorrection(Presented, FallStart, FallEnd, FVector(0.0, 0.0, 20.0)).Z, 10.0, 0.01);
	TestTrue(TEXT("beyond the ends: clamped"), FGSTN_BodyCapsule::SmoothingCorrection(FallEnd + FVector(0.0, 0.0, 5.0), FallStart, FallEnd, FVector(0.0, 0.0, 20.0)).IsNearlyZero());
	return true;
}

#endif
