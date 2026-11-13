#include "Editor.h"
#include "Editor/Transactor.h"
#include "Hits/GSTN_HitShapeUtils.h"
#include "Misc/AutomationTest.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_HitboxKeyedAccess.h"
#include "Timeline/GSTN_KeyedTrackAccess.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineGesture.h"
#include "Timeline/GSTN_TimelineLayout.h"
#include "Timeline/GSTN_TimelineTrackEditor.h"
#include "Timeline/GSTN_TimelineUndo.h"
#include "UObject/StrongObjectPtr.h"
#include "Validation/GSTN_CombatValidators.h"

#if WITH_DEV_AUTOMATION_TESTS

// The Hitbox and HurtboxModifier tracks in the timeline (plan m5 E5b): the keyed adapter with one lane per hitbox, the key gestures on it, the rows. The
// adapter and the track editors are the built-ins the module registered at startup, so these tests also prove the registration.

#define GSTN_HITBOX_TRACK_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace GSTNHitboxTrackTest
{
	using FTrackInstance = TInstancedStruct<FGSTN_TimelineTrack>;

	FGSTN_HitboxKey Key(int32 Frame, double X, double Y = 0.0)
	{
		FGSTN_HitboxKey Result;
		Result.Frame = Frame;
		Result.Offset = FVector(X, Y, 0.0);
		return Result;
	}

	FGSTN_HitboxSpec Spec(std::initializer_list<FGSTN_HitboxKey> Keys, uint8 Group = 0)
	{
		FGSTN_HitboxSpec Result;
		Result.HitGroup = Group;
		Result.Keys = Keys;
		return Result;
	}

	/** A transient timeline of 40 frames with one hitbox track [4, 14) of the given hitboxes, a model on it and an undo client */
	struct FFixture
	{
		TStrongObjectPtr<UGSTN_CombatTimeline> Timeline;
		TUniquePtr<FGSTN_TimelineEditorModel> Model;
		TUniquePtr<FGSTN_TimelineUndoClient> UndoClient;
		static constexpr int32 TrackIndex = 0;

		explicit FFixture(std::initializer_list<FGSTN_HitboxSpec> Specs)
		{
			Timeline.Reset(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
			Timeline->Data.TotalFrames = 40;
			Timeline->Data.FirstActionableFrame = 20;
			FGSTN_TimelineTrack_Hitbox Track;
			Track.StartFrame = 4;
			Track.EndFrame = 14;
			Track.Part.GroupName = TEXT("Hands");
			Track.Hitboxes = Specs;
			Timeline->Data.Tracks.Add(FTrackInstance::Make<FGSTN_TimelineTrack_Hitbox>(Track));
			Model = MakeUnique<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
			Model->SetTickRate(62.5f);
			UndoClient = MakeUnique<FGSTN_TimelineUndoClient>(*Model);
		}

		FGSTN_TimelineTrack_Hitbox& Track() { return Timeline->Data.Tracks[TrackIndex].GetMutable<FGSTN_TimelineTrack_Hitbox>(); }
		const FGSTN_CombatTimelineData& Data() const { return Timeline->Data; }
	};

	int32 AppliedSteps()
	{
		return (GEditor && GEditor->Trans) ? GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() : 0;
	}
}

using namespace GSTNHitboxTrackTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitboxAdapterTest, "GSTNCombat.Editor.Model.HitboxKeysAreFlatOverLanes", GSTN_HITBOX_TRACK_TEST_FLAGS)
bool FGSTN_HitboxAdapterTest::RunTest(const FString& Parameters)
{
	FFixture F({ Spec({ Key(0, 0.0), Key(3, 30.0), Key(6, 60.0) }, 0), Spec({ Key(2, 5.0), Key(8, 80.0) }, 1) });
	const IGSTN_KeyedTrackAccess* Access = FGSTN_KeyedTrackAccessRegistry::Find(FGSTN_TimelineTrack_Hitbox::StaticStruct());
	if (!TestNotNull(TEXT("the module registered the hitbox adapter"), Access))
	{
		return false;
	}
	const FGSTN_TimelineTrack& Track = *F.Data().Tracks[0].GetPtr<FGSTN_TimelineTrack>();

	TestEqual(TEXT("five keys over two lanes"), Access->GetKeyCount(Track), 5);
	TestEqual(TEXT("two lanes"), Access->GetLaneCount(Track), 2);
	// Spec-major: 0..2 are hitbox 0, 3..4 are hitbox 1
	TestEqual(TEXT("key 2 is in lane 0"), Access->GetKeyLane(Track, 2), 0);
	TestEqual(TEXT("key 3 is in lane 1"), Access->GetKeyLane(Track, 3), 1);
	// The model sees absolute frames: the track starts at 4
	TestEqual(TEXT("first key of lane 0 at 4"), Access->GetKeyFrame(Track, 0), 4);
	TestEqual(TEXT("first key of lane 1 at 4 + 2"), Access->GetKeyFrame(Track, 3), 6);
	TestEqual(TEXT("last key at 4 + 8"), Access->GetKeyFrame(Track, 4), 12);
	TestEqual(TEXT("out of range"), Access->GetKeyFrame(Track, 5), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("flat index of a key"), FGSTN_HitboxKeyedAccess::ToFlat(F.Track(), 1, 1), 4);

	// The pose the gizmo reads is the key's own
	FTransform Pose;
	TestTrue(TEXT("a key has a pose"), Access->GetKeyTransform(Track, 1, Pose));
	TestTrue(TEXT("its offset"), Pose.GetLocation().Equals(FVector(30.0, 0.0, 0.0)));

	// Moving the window moves the keys with it: they count from the first frame of the track
	TestTrue(TEXT("move the window by 3"), F.Model->MoveWindow(0, 3));
	TestEqual(TEXT("the key moved with it"), Access->GetKeyFrame(*F.Data().Tracks[0].GetPtr<FGSTN_TimelineTrack>(), 0), 7);
	TestEqual(TEXT("without being re-keyed"), F.Track().Hitboxes[0].Keys[0].Frame, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitboxAutoKeyTest, "GSTNCombat.Editor.Model.AutoKeyCreatesInterpolatedKey", GSTN_HITBOX_TRACK_TEST_FLAGS)
bool FGSTN_HitboxAutoKeyTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->Trans == nullptr)
	{
		AddWarning(TEXT("No transaction buffer in this run."));
		return true;
	}
	FFixture F({ Spec({ Key(0, 0.0), Key(6, 60.0, 30.0) }) });

	// What the runtime evaluates on the path before the key is added, at every frame of the window
	TArray<FTransform> Before;
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		Before.Add(UGSTN_HitShapeUtils::EvaluateKeys(F.Track().Hitboxes[0].Keys, Frame));
	}

	const int32 Steps = AppliedSteps();
	const int32 NewIndex = F.Model->AddKey(0, 4 + 3); // absolute frame 7, three frames into the window
	TestEqual(TEXT("the key is the second of the lane"), NewIndex, 1);
	TestEqual(TEXT("one undo step"), AppliedSteps(), Steps + 1);
	TestEqual(TEXT("three keys"), F.Track().Hitboxes[0].Keys.Num(), 3);

	const FGSTN_HitboxKey& Added = F.Track().Hitboxes[0].Keys[1];
	TestEqual(TEXT("at the frame asked for, counted from the track's start"), Added.Frame, 3);
	TestTrue(TEXT("halfway along the path: x = 30"), FMath::IsNearlyEqual(Added.Offset.X, 30.0, 1e-6));
	TestTrue(TEXT("y = 15"), FMath::IsNearlyEqual(Added.Offset.Y, 15.0, 1e-6));

	// The point of the interpolated key: the path the runtime evaluates did not change
	bool bPathUnchanged = true;
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		bPathUnchanged &= UGSTN_HitShapeUtils::EvaluateKeys(F.Track().Hitboxes[0].Keys, Frame).Equals(Before[Frame], 1e-6f);
	}
	TestTrue(TEXT("the evaluated path is the same at every frame"), bPathUnchanged);
	TestTrue(TEXT("and the new key is selected"), F.Model->IsSelected(FGSTN_Selection::Key(0, 1)));

	// An auto-key outside the keyed range takes the nearest key's values (the path holds there)
	F.Model->AddKey(0, 4 + 9);
	const FGSTN_HitboxKey& Late = F.Track().Hitboxes[0].Keys.Last();
	TestEqual(TEXT("a key past the last one"), Late.Frame, 9);
	TestTrue(TEXT("holds the last key's offset"), Late.Offset.Equals(FVector(60.0, 30.0, 0.0), 1e-6));

	GEditor->UndoTransaction();
	GEditor->UndoTransaction();
	TestEqual(TEXT("two undos restore the two keys"), F.Track().Hitboxes[0].Keys.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitboxLaneAddTest, "GSTNCombat.Editor.Model.HitboxAddKeyGoesToItsLane", GSTN_HITBOX_TRACK_TEST_FLAGS)
bool FGSTN_HitboxLaneAddTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->Trans == nullptr)
	{
		AddWarning(TEXT("No transaction buffer in this run."));
		return true;
	}
	FFixture F({ Spec({ Key(0, 0.0) }, 0), Spec({ Key(1, 10.0) }, 1) });

	// An explicit lane
	const int32 InLaneOne = F.Model->AddKey(0, 4 + 5, 1);
	TestEqual(TEXT("the lane named"), F.Track().Hitboxes[1].Keys.Num(), 2);
	TestEqual(TEXT("and no key went to the other"), F.Track().Hitboxes[0].Keys.Num(), 1);
	TestEqual(TEXT("its flat index is after lane 0's key and the lane's first"), InLaneOne, 2);

	// No lane: the lane of the selected key
	F.Model->SelectKey(0, 0); // the key of lane 0
	F.Model->AddKey(0, 4 + 7);
	TestEqual(TEXT("the lane of the selected key"), F.Track().Hitboxes[0].Keys.Num(), 2);

	// The same frame in another lane is allowed and a double click on the lane checks only its own lane
	TestTrue(TEXT("a key on a frame that has one in another lane"), FGSTN_TimelineActions::AddAtFrame(*F.Model, 0, 4 + 1, 0));
	TestFalse(TEXT("but not twice in the lane"), FGSTN_TimelineActions::AddAtFrame(*F.Model, 0, 4 + 1, 0));

	// A track that has no hitbox: the first key makes one
	FFixture Bare(std::initializer_list<FGSTN_HitboxSpec>{});
	TestEqual(TEXT("no lanes"), FGSTN_TimelineLayout::GetLaneCount(Bare.Data(), 0), 1);
	TestNotEqual(TEXT("the first key of an empty track"), Bare.Model->AddKey(0, 4 + 2), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("made a hitbox"), Bare.Track().Hitboxes.Num(), 1);
	TestEqual(TEXT("with that one key"), Bare.Track().Hitboxes[0].Keys.Num(), 1);
	TestEqual(TEXT("two frames in"), Bare.Track().Hitboxes[0].Keys[0].Frame, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitboxKeysSortedTest, "GSTNCombat.Editor.Model.HitboxKeysSortedAfterMove", GSTN_HITBOX_TRACK_TEST_FLAGS)
bool FGSTN_HitboxKeysSortedTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->Trans == nullptr)
	{
		AddWarning(TEXT("No transaction buffer in this run."));
		return true;
	}
	FFixture F({ Spec({ Key(0, 0.0), Key(3, 30.0), Key(6, 60.0) }, 0), Spec({ Key(2, 5.0), Key(8, 80.0) }, 1) });

	// Move the first key of lane 0 (absolute frame 4) past the second (absolute 7): to absolute 8
	F.Model->SelectKey(0, 0);
	const int32 Steps = AppliedSteps();
	TestEqual(TEXT("one key moved"), F.Model->MoveKeys(F.Model->GetSelection(), 4), 1);
	TestEqual(TEXT("one undo step"), AppliedSteps(), Steps + 1);

	const TArray<FGSTN_HitboxKey>& Lane0 = F.Track().Hitboxes[0].Keys;
	TestEqual(TEXT("lane 0 still has three keys"), Lane0.Num(), 3);
	TestTrue(TEXT("sorted by frame"), Lane0[0].Frame <= Lane0[1].Frame && Lane0[1].Frame <= Lane0[2].Frame);
	TestEqual(TEXT("the moved key is in the middle now: frame 4 relative"), Lane0[1].Frame, 4);
	TestTrue(TEXT("and kept its offset"), Lane0[1].Offset.Equals(FVector(0.0, 0.0, 0.0)));
	TestTrue(TEXT("the selection follows it"), F.Model->IsSelected(FGSTN_Selection::Key(0, 1)));
	TestEqual(TEXT("lane 1 was not touched"), F.Track().Hitboxes[1].Keys[0].Frame, 2);

	// Lane 1's keys are indexed after lane 0's; moving one of them sorts inside its lane only
	F.Model->SelectKey(0, 3);
	F.Model->MoveKeys(F.Model->GetSelection(), 9); // the key at relative 2 goes to relative 11: past the other one
	const TArray<FGSTN_HitboxKey>& Lane1 = F.Track().Hitboxes[1].Keys;
	TestTrue(TEXT("lane 1 sorted"), Lane1[0].Frame < Lane1[1].Frame);
	TestEqual(TEXT("lane 0 untouched by it"), F.Track().Hitboxes[0].Keys.Num(), 3);
	TestTrue(TEXT("the selection is the moved key, now last in its lane (flat index 4)"), F.Model->IsSelected(FGSTN_Selection::Key(0, 4)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitboxDuplicateTest, "GSTNCombat.Editor.Model.DuplicateKeysAcrossFrames", GSTN_HITBOX_TRACK_TEST_FLAGS)
bool FGSTN_HitboxDuplicateTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->Trans == nullptr)
	{
		AddWarning(TEXT("No transaction buffer in this run."));
		return true;
	}
	FFixture F({ Spec({ Key(0, 0.0), Key(2, 20.0) }, 0), Spec({ Key(1, 11.0), Key(3, 33.0) }, 1), Spec({ Key(0, 100.0) }, 2) });

	// Keys of three lanes at once: one from lane 0 (flat 1), two from lane 1 (flat 2, 3) and the one of lane 2 (flat 4)
	const TArray<FGSTN_Selection> Chosen = { FGSTN_Selection::Key(0, 1), FGSTN_Selection::Key(0, 2), FGSTN_Selection::Key(0, 3), FGSTN_Selection::Key(0, 4) };
	const int32 Steps = AppliedSteps();
	TestEqual(TEXT("four copies"), F.Model->DuplicateKeys(Chosen, 4), 4);
	TestEqual(TEXT("one undo step"), AppliedSteps(), Steps + 1);

	// Each copy went into the lane of its original, 4 frames later
	TestEqual(TEXT("lane 0 has three keys"), F.Track().Hitboxes[0].Keys.Num(), 3);
	TestEqual(TEXT("lane 1 has four"), F.Track().Hitboxes[1].Keys.Num(), 4);
	TestEqual(TEXT("lane 2 has two"), F.Track().Hitboxes[2].Keys.Num(), 2);
	TestEqual(TEXT("the copy of lane 0's key at 2 is at 6"), F.Track().Hitboxes[0].Keys.Last().Frame, 6);
	TestTrue(TEXT("with its offset"), F.Track().Hitboxes[0].Keys.Last().Offset.Equals(FVector(20.0, 0.0, 0.0)));
	TestEqual(TEXT("lane 2's copy at 4"), F.Track().Hitboxes[2].Keys.Last().Frame, 4);
	TestTrue(TEXT("with its offset"), F.Track().Hitboxes[2].Keys.Last().Offset.Equals(FVector(100.0, 0.0, 0.0)));

	// The selection is the four copies, found through the sort and the shift of the lanes after an insert
	TestEqual(TEXT("four selected"), F.Model->GetSelection().Num(), 4);
	TArray<double> SelectedOffsets;
	for (const FGSTN_Selection& Item : F.Model->GetSelection())
	{
		const FGSTN_HitboxKey* Selected = static_cast<const FGSTN_HitboxKey*>(FGSTN_KeyedTrackAccessRegistry::Find(FGSTN_TimelineTrack_Hitbox::StaticStruct())->GetKey(*F.Data().Tracks[0].GetPtr<FGSTN_TimelineTrack>(), Item.KeyIndex));
		SelectedOffsets.Add(Selected ? Selected->Offset.X : -1.0);
	}
	SelectedOffsets.Sort();
	TestEqual(TEXT("the selected keys are the copies (20, 11, 33, 100)"), SelectedOffsets.Num() == 4 ? SelectedOffsets[0] + SelectedOffsets[1] + SelectedOffsets[2] + SelectedOffsets[3] : -1.0, 20.0 + 11.0 + 33.0 + 100.0);

	// The copy of a key in the last frames is clamped to the timeline, not lost: duplicate the last key of lane 0 far to the right
	F.Model->SelectKey(0, FGSTN_HitboxKeyedAccess::ToFlat(F.Track(), 0, 2));
	TestEqual(TEXT("clamped, still one copy"), F.Model->DuplicateKeys(F.Model->GetSelection(), 1000), 1);
	TestTrue(TEXT("inside the timeline"), F.Data().Tracks[0].GetPtr<FGSTN_TimelineTrack>()->StartFrame + F.Track().Hitboxes[0].Keys.Last().Frame <= 39);

	GEditor->UndoTransaction();
	GEditor->UndoTransaction();
	TestEqual(TEXT("undo restores lane 0"), F.Track().Hitboxes[0].Keys.Num(), 2);
	TestEqual(TEXT("and lane 2"), F.Track().Hitboxes[2].Keys.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitboxClipboardTest, "GSTNCombat.Editor.Model.HitboxKeysCopyAndPaste", GSTN_HITBOX_TRACK_TEST_FLAGS)
bool FGSTN_HitboxClipboardTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->Trans == nullptr)
	{
		AddWarning(TEXT("No transaction buffer in this run."));
		return true;
	}
	FFixture F({ Spec({ Key(0, 0.0), Key(4, 40.0, 7.0) }, 0), Spec({ Key(1, 1.0) }, 1) });
	const FString Text = F.Model->CopyKeysToText({ FGSTN_Selection::Key(0, 1) });
	TestFalse(TEXT("something to paste"), Text.IsEmpty());

	// Pasted with a lane-1 key selected: it goes there, at the playhead frame
	F.Model->SelectKey(0, 2);
	TestEqual(TEXT("one key pasted"), F.Model->PasteKeysFromText(0, 4 + 8, Text), 1);
	TestEqual(TEXT("into the lane of the selected key"), F.Track().Hitboxes[1].Keys.Num(), 2);
	TestEqual(TEXT("at the frame"), F.Track().Hitboxes[1].Keys.Last().Frame, 8);
	TestTrue(TEXT("with the copied values"), F.Track().Hitboxes[1].Keys.Last().Offset.Equals(FVector(40.0, 7.0, 0.0), 1e-4));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitboxLayoutTest, "GSTNCombat.Editor.Timeline.HitboxLanesAreHitTestedByHeight", GSTN_HITBOX_TRACK_TEST_FLAGS)
bool FGSTN_HitboxLayoutTest::RunTest(const FString& Parameters)
{
	// Two hitboxes with a key on the same frame: told apart by the height of the click
	FFixture F({ Spec({ Key(2, 0.0) }, 0), Spec({ Key(2, 10.0) }, 1) });
	FGSTN_TimelineViewTransform View;
	View.PixelsPerFrame = 20.0;
	View.FirstVisibleFrame = 0.0;
	const double KeyX = View.FrameToPixel(4 + 2 + 0.5);

	TestEqual(TEXT("two lanes"), FGSTN_TimelineLayout::GetLaneCount(F.Data(), 0), 2);
	const double Lane0Y = FGSTN_TimelineLayout::GetKeyY(F.Data(), 0, 0);
	const double Lane1Y = FGSTN_TimelineLayout::GetKeyY(F.Data(), 0, 1);
	TestTrue(TEXT("lane 1 is below lane 0"), Lane1Y > Lane0Y);
	TestTrue(TEXT("both inside the row"), Lane0Y > 0.0 && Lane1Y < FGSTN_TimelineLayout::RowHeight);
	TestEqual(TEXT("the lane's y is the key's y"), FGSTN_TimelineLayout::GetLaneY(F.Data(), 0, 1), Lane1Y);

	TestEqual(TEXT("a click at the top lane's height picks the first key"), FGSTN_TimelineLayout::HitTestTrack(F.Data(), 0, View, KeyX, Lane0Y).KeyIndex, 0);
	TestEqual(TEXT("a click at the lower lane's height picks the other"), FGSTN_TimelineLayout::HitTestTrack(F.Data(), 0, View, KeyX, Lane1Y).KeyIndex, 1);
	TestTrue(TEXT("with no y it is still a key hit (the nearest in x, a tie goes to the later key)"), FGSTN_TimelineLayout::HitTestTrack(F.Data(), 0, View, KeyX).Kind == EGSTN_LaneHitKind::Key);
	TestEqual(TEXT("a click beside both is the bar, not a key"), static_cast<int32>(FGSTN_TimelineLayout::HitTestTrack(F.Data(), 0, View, View.FrameToPixel(4 + 7.5), Lane1Y).Kind), static_cast<int32>(EGSTN_LaneHitKind::Body));

	// A row with one lane (or none) keeps its keys in the middle
	FFixture One({ Spec({ Key(1, 0.0) }) });
	TestEqual(TEXT("one lane: the middle"), FGSTN_TimelineLayout::GetKeyY(One.Data(), 0, 0), FGSTN_TimelineLayout::RowHeight * 0.5);
	TestTrue(TEXT("bigger diamonds when alone"), FGSTN_TimelineLayout::GetKeyHalfSize(One.Data(), 0) > FGSTN_TimelineLayout::GetKeyHalfSize(F.Data(), 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitboxRowsTest, "GSTNCombat.Editor.Registry.HitboxAndHurtboxRows", GSTN_HITBOX_TRACK_TEST_FLAGS)
bool FGSTN_HitboxRowsTest::RunTest(const FString& Parameters)
{
	const FGSTN_TrackEditorRegistry& Registry = FGSTN_TrackEditorRegistry::Get();
	const TSharedPtr<const IGSTN_TrackEditor> HitboxEditor = Registry.Find(FGSTN_TimelineTrack_Hitbox::StaticStruct());
	const TSharedPtr<const IGSTN_TrackEditor> HurtEditor = Registry.Find(FGSTN_TimelineTrack_HurtboxModifier::StaticStruct());
	if (!TestTrue(TEXT("both rows are registered"), HitboxEditor.IsValid() && HurtEditor.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("the hitbox row has keys (the adapter)"), HitboxEditor->HasKeys());
	TestFalse(TEXT("the hurtbox modifier is a window"), HurtEditor->HasKeys());

	// Hitbox label: the part, then each hitbox by group and where its space is
	FGSTN_TimelineTrack_Hitbox Hitbox;
	TestEqual(TEXT("no hitbox"), HitboxEditor->GetRowLabel(Hitbox).ToString(), FString(TEXT("(no part): no hitbox")));
	Hitbox.Part.GroupName = TEXT("Hands");
	Hitbox.Hitboxes.Add(Spec({ Key(0, 0.0) }, 0));
	FGSTN_HitboxSpec Socketed = Spec({ Key(0, 0.0) }, 1);
	Socketed.Space = EGSTN_HitboxSpace::BakedSocket;
	Socketed.Socket = TEXT("hand_r");
	Socketed.Zone = EGSTN_HitboxZone::Sweet;
	Hitbox.Hitboxes.Add(Socketed);
	TestEqual(TEXT("part, group and socket"), HitboxEditor->GetRowLabel(Hitbox).ToString(), FString(TEXT("Hands: G0, G1 @hand_r sweet")));
	for (int32 Extra = 0; Extra < 3; ++Extra)
	{
		Hitbox.Hitboxes.Add(Spec({ Key(0, 0.0) }, 2));
	}
	TestTrue(TEXT("a few hitboxes then +N"), HitboxEditor->GetRowLabel(Hitbox).ToString().EndsWith(TEXT("+2")));

	// A new hitbox track starts with a default sphere so it has a lane
	FGSTN_TimelineTrack_Hitbox Fresh;
	HitboxEditor->InitNewTrack(Fresh, 5, FGSTN_CombatTimelineData());
	TestEqual(TEXT("a new track has a hitbox"), Fresh.Hitboxes.Num(), 1);
	TestEqual(TEXT("with a key"), Fresh.Hitboxes[0].Keys.Num(), 1);
	TestTrue(TEXT("a sphere with volume"), Fresh.Hitboxes[0].Shape.Type == EGSTN_ShapeType::Sphere && Fresh.Hitboxes[0].Shape.Extent.X > 0.0);

	// Hurtbox modifier: coloured by the state, labelled with it and its targets
	FGSTN_TimelineTrack_HurtboxModifier Modifier;
	Modifier.Kind = EGSTN_HurtboxModifierKind::Invincible;
	const FLinearColor Invincible = HurtEditor->GetTrackColor(Modifier);
	Modifier.Kind = EGSTN_HurtboxModifierKind::Intangible;
	const FLinearColor Intangible = HurtEditor->GetTrackColor(Modifier);
	Modifier.Kind = EGSTN_HurtboxModifierKind::Armor;
	const FLinearColor Armor = HurtEditor->GetTrackColor(Modifier);
	TestTrue(TEXT("three states, three colours"), !Invincible.Equals(Intangible) && !Intangible.Equals(Armor) && !Invincible.Equals(Armor));
	TestEqual(TEXT("the colour is the kind's, for a track editor that does not vary it"), HitboxEditor->GetTrackColor(Hitbox), HitboxEditor->GetColor());

	TestEqual(TEXT("armor with no threshold says so"), HurtEditor->GetRowLabel(Modifier).ToString(), FString(TEXT("Armor (no threshold): (no hurtbox)")));
	Modifier.ArmorThreshold.Value = 30.f;
	Modifier.AffectedHurtboxes = { TEXT("Torso"), TEXT("Head") };
	TestEqual(TEXT("armor with a threshold and two targets"), HurtEditor->GetRowLabel(Modifier).ToString(), FString(TEXT("Armor >= 30: Torso, Head")));
	Modifier.ArmorThreshold.bInfinite = true;
	TestTrue(TEXT("super armor"), HurtEditor->GetRowLabel(Modifier).ToString().StartsWith(TEXT("Super armor")));
	Modifier.Kind = EGSTN_HurtboxModifierKind::Intangible;
	Modifier.Part.GroupName = TEXT("Feet");
	TestEqual(TEXT("intangible feet"), HurtEditor->GetRowLabel(Modifier).ToString(), FString(TEXT("Intangible: Feet, Torso, Head")));

	// With editors registered, a timeline of these tracks no longer gets the generic-row warning
	FGSTN_CombatTimelineData Data;
	Data.TotalFrames = 30;
	Data.FirstActionableFrame = 20;
	Data.Tracks.Add(FTrackInstance::Make<FGSTN_TimelineTrack_Hitbox>(Hitbox));
	Data.Tracks.Add(FTrackInstance::Make<FGSTN_TimelineTrack_HurtboxModifier>(Modifier));
	FGSTN_ValidationReport Report;
	FGSTN_CombatValidators::CheckUnknownTracks(Data, FGSTN_TimelineRuleContext(), Report);
	TestEqual(TEXT("no UnknownTrack for them"), Report.Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
