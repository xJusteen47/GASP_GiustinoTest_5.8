// Automation tests of the hurtbox pose (owner 2026-11-16): hurtboxes follow their bones through a move, detail levels, a move's Hurtbox tracks,
// a kit's movement hurtbox. Pure data, so the same inputs give the same pose on every machine.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Hits/GSTN_HurtboxPose.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNHurtboxPoseTests
{
	FGSTN_Hurtbox MakeBox(FName Name, const FVector& Location, EGSTN_HurtboxDetailUse Use, FName Bone = NAME_None)
	{
		FGSTN_Hurtbox Box;
		Box.Name = Name;
		Box.Bone = Bone;
		Box.Shape.Type = EGSTN_ShapeType::Sphere;
		Box.Shape.Extent = FVector(10.0);
		Box.Shape.Local = FTransform(Location);
		Box.DetailUse = Use;
		return Box;
	}

	/** body (SimpleOnly), head (Both), shin (DetailedOnly, follows calf_r whose reference is at z 40) */
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> MakeSet()
	{
		TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>());
		Set->Hurtboxes.Add(MakeBox(TEXT("body"), FVector(0.0, 0.0, 0.0), EGSTN_HurtboxDetailUse::SimpleOnly));
		Set->Hurtboxes.Add(MakeBox(TEXT("head"), FVector(0.0, 0.0, 80.0), EGSTN_HurtboxDetailUse::Both));
		FGSTN_Hurtbox Shin = MakeBox(TEXT("shin"), FVector(0.0, 10.0, 30.0), EGSTN_HurtboxDetailUse::DetailedOnly, TEXT("calf_r"));
		Shin.bFollowBone = true;
		Shin.BoneReference = FTransform(FVector(0.0, 10.0, 40.0));
		Set->Hurtboxes.Add(Shin);
		return Set;
	}

	/** 10 frames; calf_r baked moving forward 5 cm a frame from its reference */
	FGSTN_CombatTimelineData MakeTimeline()
	{
		FGSTN_CombatTimelineData Data;
		Data.TotalFrames = 10;
		FGSTN_BakedSocketFrames& Calf = Data.BakedSockets.Add(TEXT("calf_r"));
		for (int32 Frame = 0; Frame < 10; ++Frame)
		{
			Calf.Frames.Add(FTransform(FVector(5.0 * Frame, 10.0, 40.0)));
		}
		return Data;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HurtboxPoseLegacyTest, "GSTNCombat.Core.Hits.HurtboxPose.LegacySetUnchanged", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HurtboxPoseLegacyTest::RunTest(const FString& Parameters)
{
	// A set made before the levels: Both, not following. Inside and outside a move the pose is the default (the set as authored)
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>());
	Set->Hurtboxes.Add(GSTNHurtboxPoseTests::MakeBox(TEXT("torso"), FVector(0.0, 0.0, 15.0), EGSTN_HurtboxDetailUse::Both, TEXT("spine_03")));
	const FGSTN_CombatTimelineData Timeline = GSTNHurtboxPoseTests::MakeTimeline();

	FGSTN_HurtboxPoseInput Input;
	Input.Set = Set.Get();
	FGSTN_HurtboxPose Pose;
	FGSTN_HurtboxPose::Build(Input, Pose);
	TestTrue(TEXT("outside a move: default pose"), Pose.IsDefault());
	Input.Timeline = &Timeline;
	Input.TimelineFrame = 4;
	FGSTN_HurtboxPose::Build(Input, Pose);
	TestTrue(TEXT("inside a move: default pose"), Pose.IsDefault());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HurtboxPoseLevelsTest, "GSTNCombat.Core.Hits.HurtboxPose.DetailLevels", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HurtboxPoseLevelsTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set = GSTNHurtboxPoseTests::MakeSet();
	FGSTN_CombatTimelineData Timeline = GSTNHurtboxPoseTests::MakeTimeline();
	FGSTN_HurtboxPoseInput Input;
	Input.Set = Set.Get();
	FGSTN_HurtboxPose Pose;

	// Idle / movement: Simple = body and head, no shin
	FGSTN_HurtboxPose::Build(Input, Pose);
	TestTrue(TEXT("movement: body"), Pose.IsPresent(0));
	TestTrue(TEXT("movement: head"), Pose.IsPresent(1));
	TestFalse(TEXT("movement: no shin"), Pose.IsPresent(2));

	// A move: Detailed = head and shin, no body
	Input.Timeline = &Timeline;
	FGSTN_HurtboxPose::Build(Input, Pose);
	TestFalse(TEXT("move: no body"), Pose.IsPresent(0));
	TestTrue(TEXT("move: head"), Pose.IsPresent(1));
	TestTrue(TEXT("move: shin"), Pose.IsPresent(2));

	// A move that asks for Simple
	Timeline.HurtboxDetail = EGSTN_HurtboxDetailOverride::Simple;
	FGSTN_HurtboxPose::Build(Input, Pose);
	TestTrue(TEXT("move override Simple: body"), Pose.IsPresent(0));
	TestFalse(TEXT("move override Simple: no shin"), Pose.IsPresent(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HurtboxPoseFollowTest, "GSTNCombat.Core.Hits.HurtboxPose.FollowsBone", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HurtboxPoseFollowTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set = GSTNHurtboxPoseTests::MakeSet();
	const FGSTN_CombatTimelineData Timeline = GSTNHurtboxPoseTests::MakeTimeline();
	FGSTN_HurtboxPoseInput Input;
	Input.Set = Set.Get();
	Input.Timeline = &Timeline;
	FGSTN_HurtboxPose Pose;

	Input.TimelineFrame = 0;
	FGSTN_HurtboxPose::Build(Input, Pose);
	TestTrue(TEXT("frame 0: the bone is at its reference, so is the shin"), Pose.GetShape(*Set, 2).Local.GetLocation().Equals(FVector(0.0, 10.0, 30.0), 1e-6));

	Input.TimelineFrame = 4;
	FGSTN_HurtboxPose::Build(Input, Pose);
	TestTrue(TEXT("frame 4: the shin moved 20 cm forward with the calf"), Pose.GetShape(*Set, 2).Local.GetLocation().Equals(FVector(20.0, 10.0, 30.0), 1e-6));
	TestTrue(TEXT("the head does not follow"), Pose.GetShape(*Set, 1).Local.GetLocation().Equals(FVector(0.0, 0.0, 80.0), 1e-6));

	Input.TimelineFrame = 50;
	FGSTN_HurtboxPose::Build(Input, Pose);
	TestTrue(TEXT("past the bake the last frame holds"), Pose.GetShape(*Set, 2).Local.GetLocation().Equals(FVector(45.0, 10.0, 30.0), 1e-6));

	// Determinism: the same inputs, the same bits
	FGSTN_HurtboxPose Again;
	FGSTN_HurtboxPose::Build(Input, Again);
	TestTrue(TEXT("same inputs, same pose"), Again.GetShape(*Set, 2).Local.Equals(Pose.GetShape(*Set, 2).Local, 0.0));

	// A bone the move did not bake: the reference pose
	FGSTN_CombatTimelineData Unbaked = Timeline;
	Unbaked.BakedSockets.Reset();
	Input.Timeline = &Unbaked;
	FGSTN_HurtboxPose::Build(Input, Pose);
	TestTrue(TEXT("unbaked bone: reference pose"), Pose.GetShape(*Set, 2).Local.GetLocation().Equals(FVector(0.0, 10.0, 30.0), 1e-6));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HurtboxPoseTrackTest, "GSTNCombat.Core.Hits.HurtboxPose.HurtboxTrack", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HurtboxPoseTrackTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set = GSTNHurtboxPoseTests::MakeSet();
	FGSTN_CombatTimelineData Timeline = GSTNHurtboxPoseTests::MakeTimeline();

	// Frames 3-6: a stretched leg on the calf, counting as the shin and replacing it
	FGSTN_TimelineTrack_Hurtbox Track;
	Track.StartFrame = 3;
	Track.EndFrame = 7;
	Track.CountsAs = TEXT("shin");
	Track.bReplaceCountsAs = true;
	FGSTN_HitboxSpec Leg;
	Leg.Space = EGSTN_HitboxSpace::BakedSocket;
	Leg.Socket = TEXT("calf_r");
	Leg.Shape.Extent = FVector(12.0);
	Leg.Shape.Local = FTransform(FVector(30.0, 0.0, 0.0));
	Track.Hitboxes.Add(Leg);
	Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hurtbox>(Track));

	FGSTN_HurtboxPoseInput Input;
	Input.Set = Set.Get();
	Input.Timeline = &Timeline;
	FGSTN_HurtboxPose Pose;

	Input.TimelineFrame = 2;
	FGSTN_HurtboxPose::Build(Input, Pose);
	TestEqual(TEXT("before the track: no extension"), Pose.Extensions.Num(), 0);
	TestTrue(TEXT("before the track: the shin is there"), Pose.IsPresent(2));

	Input.TimelineFrame = 4;
	FGSTN_HurtboxPose::Build(Input, Pose);
	TestEqual(TEXT("in the track: one extension"), Pose.Extensions.Num(), 1);
	if (Pose.Extensions.Num() == 1)
	{
		TestEqual(TEXT("it counts as the shin"), Pose.Extensions[0].HurtboxIndex, 2);
		TestTrue(TEXT("placed on the baked calf"), Pose.Extensions[0].Shape.Local.GetLocation().Equals(FVector(50.0, 10.0, 40.0), 1e-6));
	}
	TestFalse(TEXT("a replacing track hides the shin's own shape"), Pose.IsPresent(2));

	// A track is never an attack: the phases and the attack accessor skip it
	TestNull(TEXT("GetHitboxTrack skips a Hurtbox track"), Timeline.GetHitboxTrack(0));
	TestEqual(TEXT("no active frames from a Hurtbox track"), Timeline.GetActiveEndFrame(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HurtboxPoseKitTest, "GSTNCombat.Core.Hits.HurtboxPose.KitMovement", GSTN_CORE_TEST_FLAGS)
bool FGSTN_HurtboxPoseKitTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set = GSTNHurtboxPoseTests::MakeSet();
	const FGSTN_CombatTimelineData Timeline = GSTNHurtboxPoseTests::MakeTimeline();
	FGSTN_MovementHurtboxExtension Kit;
	FGSTN_MovementHurtboxShape Wide;
	Wide.CountsAs = TEXT("body");
	Kit.Shapes.Add(Wide);

	FGSTN_HurtboxPoseInput Input;
	Input.Set = Set.Get();
	Input.KitMovement = &Kit;
	FGSTN_HurtboxPose Pose;
	FGSTN_HurtboxPose::Build(Input, Pose);
	TestFalse(TEXT("the kit's body replaces the set's"), Pose.IsPresent(0));
	TestEqual(TEXT("one kit shape"), Pose.Extensions.Num(), 1);
	TestTrue(TEXT("the head stays"), Pose.IsPresent(1));

	Input.Timeline = &Timeline;
	FGSTN_HurtboxPose::Build(Input, Pose);
	TestEqual(TEXT("in a move the kit's movement shape is not used"), Pose.Extensions.Num(), 0);
	return true;
}

#endif
