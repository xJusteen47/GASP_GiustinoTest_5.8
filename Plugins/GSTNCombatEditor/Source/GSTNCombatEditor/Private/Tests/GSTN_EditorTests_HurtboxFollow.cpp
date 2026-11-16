// Editor tests of the hurtbox levels and bone following (owner 2026-11-16): the simple body, the Hurtbox track's editor, the baker's hurtbox bones.

#include "Bake/GSTN_SocketBaker.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hurtbox/GSTN_HurtboxGenerator.h"
#include "Misc/AutomationTest.h"
#include "Timeline/GSTN_TimelineTrackEditor.h"
#include "Timeline/GSTN_TimelineTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_HURTBOX_FOLLOW_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EditorHurtboxSimpleBodyTest, "GSTNCombat.Editor.HurtboxFollow.SimpleBody", GSTN_HURTBOX_FOLLOW_TEST_FLAGS)
bool FGSTN_EditorHurtboxSimpleBodyTest::RunTest(const FString& Parameters)
{
	// Two limb spheres: a foot at z -80 (r 10) and a head at z 80 (r 12)
	TArray<FGSTN_Hurtbox> Boxes;
	FGSTN_Hurtbox Foot;
	Foot.Name = TEXT("foot_l");
	Foot.Shape.Extent = FVector(10.0);
	Foot.Shape.Local = FTransform(FVector(10.0, 15.0, -80.0));
	Boxes.Add(Foot);
	FGSTN_Hurtbox Head = Foot;
	Head.Name = TEXT("head");
	Head.Shape.Extent = FVector(12.0);
	Head.Shape.Local = FTransform(FVector(0.0, 0.0, 80.0));
	Boxes.Add(Head);

	FGSTN_Hurtbox Body;
	TestTrue(TEXT("a body is made"), GSTNHurtboxGenerator::MakeSimpleBody(Boxes, Body));
	TestEqual(TEXT("capsule"), Body.Shape.Type, EGSTN_ShapeType::Capsule);
	TestEqual(TEXT("from the feet to the head: half height"), Body.Shape.Extent.Z, 91.0, 1e-6);
	TestEqual(TEXT("centred between them"), Body.Shape.Local.GetLocation().Z, 1.0, 1e-6);
	TestEqual(TEXT("on the root axis"), Body.Shape.Local.GetLocation().X, 0.0, 1e-6);
	TestEqual(TEXT("Simple level only"), Body.DetailUse, EGSTN_HurtboxDetailUse::SimpleOnly);

	TestTrue(TEXT("applied"), GSTNHurtboxGenerator::ApplySimpleBody(Boxes, FGameplayTag()));
	TestEqual(TEXT("one body added"), Boxes.Num(), 3);
	TestEqual(TEXT("the limbs become per limb"), Boxes[0].DetailUse, EGSTN_HurtboxDetailUse::DetailedOnly);
	TestTrue(TEXT("applied again"), GSTNHurtboxGenerator::ApplySimpleBody(Boxes, FGameplayTag()));
	TestEqual(TEXT("remade, not added twice"), Boxes.Num(), 3);

	TArray<FGSTN_Hurtbox> Empty;
	TestFalse(TEXT("nothing to measure"), GSTNHurtboxGenerator::MakeSimpleBody(Empty, Body));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EditorHurtboxTrackEditorTest, "GSTNCombat.Editor.HurtboxFollow.TrackEditor", GSTN_HURTBOX_FOLLOW_TEST_FLAGS)
bool FGSTN_EditorHurtboxTrackEditorTest::RunTest(const FString& Parameters)
{
	// The Hurtbox track has its own editor (name, colour) and shares the hitbox lanes through the keyed adapter of its base struct
	const TSharedPtr<const IGSTN_TrackEditor> Editor = FGSTN_TrackEditorRegistry::Get().Find(FGSTN_TimelineTrack_Hurtbox::StaticStruct());
	TestTrue(TEXT("registered"), Editor.IsValid());
	if (Editor.IsValid())
	{
		TestEqual(TEXT("its own editor, not the hitbox one"), Editor->GetTrackStruct(), FGSTN_TimelineTrack_Hurtbox::StaticStruct());
		TestTrue(TEXT("keyed like a hitbox track"), Editor->HasKeys());
	}

	// The baker bakes the socket of a Hurtbox track shape and the timeline's listed hurtbox bones
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = 4;
	Data.HurtboxBones.Add(TEXT("calf_r"));
	FGSTN_TimelineTrack_Hurtbox Track;
	FGSTN_HitboxSpec Spec;
	Spec.Space = EGSTN_HitboxSpace::BakedSocket;
	Spec.Socket = TEXT("foot_r");
	Track.Hitboxes.Add(Spec);
	Data.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hurtbox>(Track));
	TArray<FGSTN_BakeSocketUse> Uses;
	FGSTN_SocketBaker::CollectUses(Data, Uses);
	const TArray<FName> Sockets = FGSTN_SocketBaker::GetDistinctSockets(Uses);
	TestTrue(TEXT("the Hurtbox track's socket is baked"), Sockets.Contains(FName(TEXT("foot_r"))));
	TestTrue(TEXT("the listed hurtbox bone is baked"), Sockets.Contains(FName(TEXT("calf_r"))));
	return true;
}

#endif
