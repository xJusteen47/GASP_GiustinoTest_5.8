// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Misc/AutomationTest.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_HitboxKeyedAccess.h"
#include "Timeline/GSTN_HitboxOutline.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineEditorToolkit.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// The hitbox outliner of the timeline editor (owner 2026-11-13) and the safe opener of the editor (the crash of a second "Open Timeline Editor").
// The widget is a view; what it lists, what a row click does and which row a viewport click selects is GSTNHitboxOutline, tested here headless.

#define GSTN_HITBOX_OUTLINE_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace GSTNHitboxOutlineTest
{
	using FTrackInstance = TInstancedStruct<FGSTN_TimelineTrack>;

	FGSTN_HitboxSpec Spec(std::initializer_list<int32> KeyFrames)
	{
		FGSTN_HitboxSpec Result;
		for (const int32 Frame : KeyFrames)
		{
			FGSTN_HitboxKey Key;
			Key.Frame = Frame;
			Result.Keys.Add(Key);
		}
		return Result;
	}

	FTrackInstance HitboxTrack(int32 Start, int32 End, std::initializer_list<FGSTN_HitboxSpec> Specs)
	{
		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		Track.Hitboxes = Specs;
		return FTrackInstance::Make<FGSTN_TimelineTrack_Hitbox>(Track);
	}
}

using namespace GSTNHitboxOutlineTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitboxOutlineTest, "GSTNCombat.Editor.HitboxOutline.ListsFocusesAndFollowsSelection", GSTN_HITBOX_OUTLINE_TEST_FLAGS)
bool FGSTN_HitboxOutlineTest::RunTest(const FString& Parameters)
{
	// Track 0 is not a hitbox track; track 1 is group 1 [4, 14) with two hitboxes; track 2 is group 2 [20, 24), one keyed hitbox and one with no key
	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
	Timeline->Data.TotalFrames = 40;
	Timeline->Data.FirstActionableFrame = 30;
	Timeline->Data.Tracks.Add(FTrackInstance::Make<FGSTN_TimelineTrack_Movement>());
	Timeline->Data.Tracks.Add(HitboxTrack(4, 14, { Spec({ 0, 6 }), Spec({ 2 }) }));
	Timeline->Data.Tracks.Add(HitboxTrack(20, 24, { Spec({ 0 }), Spec({}) }));
	FGSTN_TimelineEditorModel Model(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
	Model.SetTickRate(62.5f);

	const TArray<FGSTN_HitboxOutlineItem> Items = GSTNHitboxOutline::Build(Timeline->Data);
	const TArray<FGSTN_HitboxOutlineItem> Expected = { { 1, INDEX_NONE }, { 1, 0 }, { 1, 1 }, { 2, INDEX_NONE }, { 2, 0 }, { 2, 1 } };
	TestTrue(TEXT("groups in data order, each followed by its hitboxes; other tracks left out"), Items == Expected);
	TestEqual(TEXT("groups are numbered among the hitbox tracks"), GSTNHitboxOutline::GetName(Timeline->Data, { 2, INDEX_NONE }).ToString(), FString(TEXT("Group 2")));
	TestEqual(TEXT("hitboxes from 1"), GSTNHitboxOutline::GetName(Timeline->Data, { 1, 1 }).ToString(), FString(TEXT("Hitbox 2")));
	TestTrue(TEXT("a group says its frames (last open frame, inclusive)"), GSTNHitboxOutline::GetSummary(Timeline->Data, { 1, INDEX_NONE }).ToString().Contains(TEXT("frames 4-13")));

	// A row outside the playhead: the playhead goes to its first key, and the key is selected so the gizmo sits on it
	Model.SetPlayhead(0);
	TestTrue(TEXT("focus a hitbox of group 2"), GSTNHitboxOutline::Focus(Model, { 2, 0 }));
	TestEqual(TEXT("the playhead moved into the group's frames"), Model.GetPlayhead(), 20);
	FGSTN_HitboxOutlineItem Selected;
	TestTrue(TEXT("its row is the selected one"), GSTNHitboxOutline::FindSelected(Model, Selected) && Selected == FGSTN_HitboxOutlineItem(2, 0));

	TestTrue(TEXT("focus the second hitbox of group 1"), GSTNHitboxOutline::Focus(Model, { 1, 1 }));
	TestEqual(TEXT("on its first key: start 4 + key 2"), Model.GetPlayhead(), 6);
	TestTrue(TEXT("a key of the second hitbox is selected"), Model.GetSelection().Num() == 1 && Model.GetSelection()[0].TrackIndex == 1
		&& Model.GetSelection()[0].KeyIndex == FGSTN_HitboxKeyedAccess::ToFlat(Timeline->Data.Tracks[1].Get<FGSTN_TimelineTrack_Hitbox>(), 1, 0));
	TestTrue(TEXT("and the outliner finds that row"), GSTNHitboxOutline::FindSelected(Model, Selected) && Selected == FGSTN_HitboxOutlineItem(1, 1));

	// Inside the window the playhead stays where the designer left it
	Model.SetPlayhead(9);
	TestTrue(TEXT("focus the first hitbox of group 1"), GSTNHitboxOutline::Focus(Model, { 1, 0 }));
	TestEqual(TEXT("the playhead was inside: it stays"), Model.GetPlayhead(), 9);

	// The viewport selected the first hitbox's key: that is what a click there does (SelectKey); the outliner follows
	Model.SelectKey(1, FGSTN_HitboxKeyedAccess::ToFlat(Timeline->Data.Tracks[1].Get<FGSTN_TimelineTrack_Hitbox>(), 0, 1));
	TestTrue(TEXT("a viewport pick selects the row of its hitbox"), GSTNHitboxOutline::FindSelected(Model, Selected) && Selected == FGSTN_HitboxOutlineItem(1, 0));

	TestTrue(TEXT("focus a group"), GSTNHitboxOutline::Focus(Model, { 2, INDEX_NONE }));
	TestTrue(TEXT("its window is selected and the group's row with it"), GSTNHitboxOutline::FindSelected(Model, Selected) && Selected == FGSTN_HitboxOutlineItem(2, INDEX_NONE));

	TestTrue(TEXT("a hitbox with no key"), GSTNHitboxOutline::Focus(Model, { 2, 1 }));
	TestTrue(TEXT("selects its group's window (no pose to put a gizmo on)"), GSTNHitboxOutline::FindSelected(Model, Selected) && Selected.IsGroup() && Selected.Track == 2);

	TestFalse(TEXT("a row that no longer exists is refused"), GSTNHitboxOutline::Focus(Model, { 0, INDEX_NONE }));
	TestFalse(TEXT("nor a hitbox past the end"), GSTNHitboxOutline::Focus(Model, { 1, 5 }));

	Model.ClearSelection();
	TestFalse(TEXT("nothing selected: no row"), GSTNHitboxOutline::FindSelected(Model, Selected));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TimelineOpenForTest, "GSTNCombat.Editor.Registry.OpenForBuildsNoToolkitForAHeldTarget", GSTN_HITBOX_OUTLINE_TEST_FLAGS)
bool FGSTN_TimelineOpenForTest::RunTest(const FString& Parameters)
{
	// The crash of 2026-11-13: a second "Open Timeline Editor" built a toolkit that was refused and destroyed without InitAssetEditor. OpenFor checks
	// first: with the target held it returns false at once (a headless run cannot build a window, so only the refusal is exercised here)
	TStrongObjectPtr<UGSTN_CombatTimeline> Asset(NewObject<UGSTN_CombatTimeline>(GetTransientPackage()));
	const FGSTN_TimelineEditContext Context = FGSTN_TimelineEditContext::ForAsset(Asset.Get());
	int32 Focused = 0;
	const TSharedPtr<FGSTN_TimelineContextLease> Held = FGSTN_TimelineEditContextRegistry::Acquire(Context, [&Focused]() { ++Focused; });
	TestTrue(TEXT("the target is held"), Held.IsValid());

	TestFalse(TEXT("OpenFor opens nothing on a held target"), FGSTN_TimelineEditorToolkit::OpenFor(EToolkitMode::Standalone, nullptr, Context));
	TestTrue(TEXT("and the holder keeps it"), FGSTN_TimelineEditContextRegistry::IsOpen(Context));
	TestFalse(TEXT("an invalid context opens nothing"), FGSTN_TimelineEditorToolkit::OpenFor(EToolkitMode::Standalone, nullptr, FGSTN_TimelineEditContext()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
