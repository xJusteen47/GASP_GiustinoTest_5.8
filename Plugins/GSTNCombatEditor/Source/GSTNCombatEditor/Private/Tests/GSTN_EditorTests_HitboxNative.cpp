// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Bake/GSTN_SocketBaker.h"
#include "Misc/AutomationTest.h"
#include "Preview/GSTN_GizmoMath.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_HitboxKeyedAccess.h"
#include "Timeline/GSTN_HitboxOutline.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineLayout.h"
#include "Timeline/GSTN_TimelineSelectionProxy.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "Timeline/Widgets/SGSTN_TimelinePanel.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// The owner's pass on the hitbox editor of 2026-11-16: names instead of numbers, the hitbox and key apart in the Details panel, parenting of the
// hitboxes to their group (and the group to a socket), frames and seconds on the ruler, native icons. Headless: the widgets only call what is tested here.

#define GSTN_HITBOX_NATIVE_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace GSTNHitboxNativeTest
{
	using FTrackInstance = TInstancedStruct<FGSTN_TimelineTrack>;

	FGSTN_HitboxSpec Spec(const FVector& Offset)
	{
		FGSTN_HitboxSpec Result;
		Result.Shape.Type = EGSTN_ShapeType::Sphere;
		Result.Shape.Extent = FVector(10.0);
		FGSTN_HitboxKey Key;
		Key.Frame = 0;
		Key.Offset = Offset;
		Result.Keys.Add(Key);
		return Result;
	}

	TStrongObjectPtr<UGSTN_CombatTimeline> MakeTimeline()
	{
		TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
		Timeline->Data.TotalFrames = 30;
		Timeline->Data.FirstActionableFrame = 20;
		FGSTN_TimelineTrack_Hitbox Track;
		Track.StartFrame = 4;
		Track.EndFrame = 10;
		Track.Hitboxes = { Spec(FVector(10.0, 0.0, 0.0)), Spec(FVector(0.0, 20.0, 0.0)) };
		Timeline->Data.Tracks.Add(FTrackInstance::Make<FGSTN_TimelineTrack_Hitbox>(Track));
		return Timeline;
	}

	const FGSTN_TimelineTrack_Hitbox& Group(const UGSTN_CombatTimeline& Timeline)
	{
		return Timeline.Data.Tracks[0].Get<FGSTN_TimelineTrack_Hitbox>();
	}
}

using namespace GSTNHitboxNativeTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitboxRenameTest, "GSTNCombat.Editor.HitboxNative.RenameAddDuplicateDelete", GSTN_HITBOX_NATIVE_TEST_FLAGS)
bool FGSTN_HitboxRenameTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline = MakeTimeline();
	FGSTN_TimelineEditorModel Model(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
	Model.SetTickRate(62.5f);

	TestEqual(TEXT("an unnamed hitbox shows its number"), GSTNHitboxOutline::GetName(Timeline->Data, { 0, 1 }).ToString(), FString(TEXT("Hitbox 2")));
	TestTrue(TEXT("rename a hitbox"), GSTNHitboxOutline::Rename(Model, { 0, 1 }, FText::FromString(TEXT("  Fist  "))));
	TestEqual(TEXT("the name is stored trimmed"), Group(*Timeline).Hitboxes[1].Name, FName(TEXT("Fist")));
	TestEqual(TEXT("and shown"), GSTNHitboxOutline::GetName(Timeline->Data, { 0, 1 }).ToString(), FString(TEXT("Fist")));
	TestFalse(TEXT("the same name again changes nothing"), GSTNHitboxOutline::Rename(Model, { 0, 1 }, FText::FromString(TEXT("Fist"))));
	TestTrue(TEXT("rename the group"), GSTNHitboxOutline::Rename(Model, { 0, INDEX_NONE }, FText::FromString(TEXT("Jab"))));
	TestEqual(TEXT("the group shows its name"), GSTNHitboxOutline::GetName(Timeline->Data, { 0, INDEX_NONE }).ToString(), FString(TEXT("Jab")));
	TestTrue(TEXT("an empty name clears it"), GSTNHitboxOutline::Rename(Model, { 0, INDEX_NONE }, FText::GetEmpty()));
	TestEqual(TEXT("the number is back"), GSTNHitboxOutline::GetName(Timeline->Data, { 0, INDEX_NONE }).ToString(), FString(TEXT("Group 1")));

	const FGSTN_HitboxOutlineItem Copy = GSTNHitboxOutline::DuplicateHitbox(Model, { 0, 1 });
	TestTrue(TEXT("duplicate appends"), Copy == FGSTN_HitboxOutlineItem(0, 2) && Group(*Timeline).Hitboxes.Num() == 3);
	TestEqual(TEXT("the copy is named after the source"), Group(*Timeline).Hitboxes[2].Name, FName(TEXT("Fist copy")));
	FGSTN_HitboxOutlineItem Selected;
	TestTrue(TEXT("and is focused"), GSTNHitboxOutline::FindSelected(Model, Selected) && Selected == Copy);

	TestTrue(TEXT("delete a hitbox"), GSTNHitboxOutline::DeleteHitbox(Model, { 0, 0 }));
	TestEqual(TEXT("two left"), Group(*Timeline).Hitboxes.Num(), 2);
	TestEqual(TEXT("the others moved up"), Group(*Timeline).Hitboxes[0].Name, FName(TEXT("Fist")));
	TestTrue(TEXT("delete one more"), GSTNHitboxOutline::DeleteHitbox(Model, { 0, 0 }));
	TestFalse(TEXT("the last hitbox of a group stays"), GSTNHitboxOutline::DeleteHitbox(Model, { 0, 0 }));

	const FGSTN_HitboxOutlineItem Added = GSTNHitboxOutline::AddHitbox(Model, 0);
	TestTrue(TEXT("add appends the default hitbox"), Added == FGSTN_HitboxOutlineItem(0, 1) && Group(*Timeline).Hitboxes.Num() == 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitboxParentingTest, "GSTNCombat.Editor.HitboxNative.ParentingToGroupAndSocket", GSTN_HITBOX_NATIVE_TEST_FLAGS)
bool FGSTN_HitboxParentingTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline = MakeTimeline();
	auto NoLiveSocket = [](FName, FTransform&) { return false; };

	// Before parenting: each hitbox where its own keys put it in root space (the old behaviour, unchanged for old assets)
	TArray<GSTNGizmo::FActiveHitbox> Active;
	GSTNGizmo::CollectActiveHitboxes(Timeline->Data, 5, FTransform::Identity, NoLiveSocket, Active);
	TestEqual(TEXT("two hitboxes open"), Active.Num(), 2);
	TestTrue(TEXT("no group transform: hitbox 1 at its key"), (Active[0].Shape.Local * Active[0].Space).GetLocation().Equals(FVector(10.0, 0.0, 0.0), 0.01));

	// The group is moved 100 forward and turned 90 degrees: its children follow, their keys now relative to it
	FGSTN_TimelineTrack_Hitbox& Track = Timeline->Data.Tracks[0].GetMutable<FGSTN_TimelineTrack_Hitbox>();
	Track.bUseGroupTransform = true;
	Track.GroupOffset = FTransform(FRotator(0.0, 90.0, 0.0), FVector(100.0, 0.0, 0.0));
	Track.Hitboxes[1].bAttachToGroup = false;
	Active.Reset();
	GSTNGizmo::CollectActiveHitboxes(Timeline->Data, 5, FTransform::Identity, NoLiveSocket, Active);
	TestTrue(TEXT("a child: key (10,0,0) turned 90 then moved 100 forward"), (Active[0].Shape.Local * Active[0].Space).GetLocation().Equals(FVector(100.0, 10.0, 0.0), 0.01));
	TestTrue(TEXT("a hitbox detached from the group stays where it was"), (Active[1].Shape.Local * Active[1].Space).GetLocation().Equals(FVector(0.0, 20.0, 0.0), 0.01));

	// The group on a socket: its children read the group's socket, and the bake collects it
	Track.GroupSpace = EGSTN_HitboxSpace::BakedSocket;
	Track.GroupSocket = TEXT("hand_r");
	TestEqual(TEXT("a child's space is the group's"), Track.GetEffectiveSpace(Track.Hitboxes[0]), EGSTN_HitboxSpace::BakedSocket);
	TestEqual(TEXT("a child's socket is the group's"), Track.GetEffectiveSocket(Track.Hitboxes[0]), FName(TEXT("hand_r")));
	TestEqual(TEXT("a detached hitbox keeps its own space"), Track.GetEffectiveSpace(Track.Hitboxes[1]), EGSTN_HitboxSpace::Root);
	TArray<FGSTN_BakeSocketUse> Uses;
	FGSTN_SocketBaker::CollectUses(Timeline->Data, Uses);
	TestTrue(TEXT("the bake needs the group's socket"), Uses.Num() == 1 && Uses[0].Socket == FName(TEXT("hand_r")) && Uses[0].HitboxIndex == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitboxDetailsFocusTest, "GSTNCombat.Editor.HitboxNative.DetailsShowOneHitboxAndKey", GSTN_HITBOX_NATIVE_TEST_FLAGS)
bool FGSTN_HitboxDetailsFocusTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline = MakeTimeline();
	TSharedPtr<FGSTN_TimelineEditorModel> Model = MakeShared<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
	Model->SetTickRate(62.5f);
	TStrongObjectPtr<UGSTN_TimelineSelectionProxy> Proxy(NewObject<UGSTN_TimelineSelectionProxy>(GetTransientPackage()));
	Proxy->Bind(Model, 0);
	Proxy->SetFocus(1, 0);
	TestTrue(TEXT("the second hitbox is focused"), Proxy->GetFocusedSpec() != nullptr && Proxy->GetFocusSpec() == 1);
	const TSharedPtr<FStructOnScope> SpecData = Proxy->MakeSpecStructData();
	const TSharedPtr<FStructOnScope> KeyData = Proxy->MakeKeyStructData();
	TestTrue(TEXT("its own struct for the Hitbox view"), SpecData.IsValid() && SpecData->GetStruct() == FGSTN_HitboxSpec::StaticStruct());
	TestTrue(TEXT("its key for the Key view"), KeyData.IsValid() && KeyData->GetStruct() == FGSTN_HitboxKey::StaticStruct());

	// The Key view writes the offset the gizmo could only drag: one commit, one undo step, the right hitbox
	reinterpret_cast<FGSTN_HitboxKey*>(KeyData->GetStructMemory())->Offset = FVector(1.0, 2.0, 3.0);
	TestTrue(TEXT("commit"), Proxy->Commit());
	TestTrue(TEXT("the typed offset reached the second hitbox"), Group(*Timeline).Hitboxes[1].Keys[0].Offset.Equals(FVector(1.0, 2.0, 3.0)));
	TestTrue(TEXT("the first is untouched"), Group(*Timeline).Hitboxes[0].Keys[0].Offset.Equals(FVector(10.0, 0.0, 0.0)));

	Proxy->SetFocus(7, 0);
	TestTrue(TEXT("a hitbox that does not exist clears the focus"), Proxy->GetFocusedSpec() == nullptr && !Proxy->MakeSpecStructData().IsValid());
	Proxy->Unbind();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TimelineRulerAndIconsTest, "GSTNCombat.Editor.HitboxNative.RulerSecondsZoomAndIcons", GSTN_HITBOX_NATIVE_TEST_FLAGS)
bool FGSTN_TimelineRulerAndIconsTest::RunTest(const FString& Parameters)
{
	FString Top;
	FString Bottom;
	FGSTN_TimelineLayout::FormatRulerLabel(24, 62.5f, EGSTN_RulerDisplay::FramesAndSeconds, Top, Bottom);
	TestTrue(TEXT("frames above seconds by default"), Top == TEXT("24") && Bottom == TEXT("0.38 s"));
	FGSTN_TimelineLayout::FormatRulerLabel(24, 62.5f, EGSTN_RulerDisplay::FramesAndMilliseconds, Top, Bottom);
	TestTrue(TEXT("the old ruler is a mode"), Top == TEXT("24") && Bottom == TEXT("384.0 ms"));
	FGSTN_TimelineLayout::FormatRulerLabel(24, 62.5f, EGSTN_RulerDisplay::Seconds, Top, Bottom);
	TestTrue(TEXT("seconds only"), Top == TEXT("0.38 s") && Bottom.IsEmpty());

	FGSTN_TimelineViewTransform View;
	View.PixelsPerFrame = 10.0;
	View.FirstVisibleFrame = 0.0;
	FGSTN_TimelineLayout::ZoomAroundCenter(View, 2.0, 400.0, 60);
	TestEqual(TEXT("zoom in doubles the scale"), View.PixelsPerFrame, 20.0);
	TestEqual(TEXT("around the middle: frame 20 stays at 200 px"), View.FrameToPixel(20.0), 200.0);
	double Min = 0.0;
	double Max = 0.0;
	FGSTN_TimelineLayout::GetScrollRange(View, 400.0, 60, Min, Max);
	TestTrue(TEXT("the scroll range covers the timeline"), Min <= 0.0 && Max >= 60.0);

	// Native icons: every toolbar role resolves to a brush of the app style. A role with none falls back to a generic icon and is listed here, not failed
	for (const FName Role : SGSTN_TimelinePanel::GetToolbarIconRoles())
	{
		bool bFound = false;
		const FName Brush = SGSTN_TimelinePanel::ResolveToolbarIcon(Role, &bFound);
		if (!bFound)
		{
			AddWarning(FString::Printf(TEXT("Toolbar icon %s: none of its brushes exists in this engine (last tried %s)"), *Role.ToString(), *Brush.ToString()));
		}
	}
	return true;
}

#endif
