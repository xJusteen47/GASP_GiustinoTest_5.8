#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "Styling/AppStyle.h"
#include "Tests/GSTN_VisualSnapshot.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_FindingsStore.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineLayout.h"
#include "Timeline/Widgets/SGSTN_FindingsList.h"
#include "Timeline/Widgets/SGSTN_FrameDataSummary.h"
#include "Timeline/Widgets/SGSTN_TimelineLengthFields.h"
#include "Timeline/Widgets/SGSTN_TimelinePanel.h"
#include "UObject/StrongObjectPtr.h"
#include "Validation/GSTN_ValidationReport.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

#if WITH_DEV_AUTOMATION_TESTS

// Offscreen snapshots of the timeline editor's widgets (GSTN.Visual.Timeline.*), so a change to the look can be inspected without driving the
// editor window. They write PNGs to Saved/Notes/ui-snapshots/ and assert nothing about the look: see Tests/GSTN_VisualSnapshot.h for the helper
// and how to run them. The widgets are the ones the toolkit hosts, wrapped in the editor's panel brush as a dock tab does.

namespace GSTNVisualTimeline
{
	template <typename T>
	TInstancedStruct<FGSTN_TimelineTrack> MakeTrack(int32 Start, int32 End, TFunctionRef<void(T&)> Setup = [](T&) {})
	{
		T Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		Setup(Track);
		return TInstancedStruct<FGSTN_TimelineTrack>::Make<T>(Track);
	}

	/** A transient timeline with one track of each kind the generic row can show, and the model on it */
	struct FVisualFixture
	{
		TStrongObjectPtr<UGSTN_CombatTimeline> Timeline;
		TSharedPtr<FGSTN_TimelineEditorModel> Model;

		FVisualFixture()
		{
			Timeline.Reset(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
			Model = MakeShared<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
			Model->SetTickRate(62.5f);

			FGSTN_CombatTimelineData& Data = Timeline->Data;
			Data.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_CancelWindow>(14, 26));
			Data.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_Movement>(2, 16));
			Data.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_Tags>(4, 20));
			Data.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_Event>(8, 9));
			Data.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_WeaponSwitch>(10, 11, [](FGSTN_TimelineTrack_WeaponSwitch& Switch) { Switch.SwapFrame = 10; }));
			Data.Tracks.Add(MakeTrack<FGSTN_TimelineTrack_Hitbox>(6, 12));
		}
	};

	/** The editor's panel colour behind a widget, as the body of a dock tab */
	TSharedRef<SWidget> OnPanel(const TSharedRef<SWidget>& Content)
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(0.f)
			[
				Content
			];
	}

	/** The Timeline tab: the panel with its toolbar, ruler, phase bar and rows */
	FString RenderTimeline(FAutomationTestBase& Test, FVisualFixture& F, const FString& Name)
	{
		const TSharedRef<SGSTN_TimelinePanel> Panel = SNew(SGSTN_TimelinePanel).Model(F.Model);
		const TSharedRef<SWidget> Root = OnPanel(Panel);
		return GSTNVisualSnapshot::Render(Test, Root, FIntPoint(1200, 420), Name, [&](int32 Pass)
		{
			// What the panel's one-shot timer does once the lane has a width (the timer itself only runs under the application's tick)
			const double LaneWidth = Panel->GetLaneWidth();
			if (LaneWidth > 0.0)
			{
				FGSTN_TimelineLayout::FrameAll(F.Model->ViewTransform, LaneWidth, F.Model->GetPlayableFrames());
				F.Model->NotifyViewChanged();
			}
		});
	}
}

using namespace GSTNVisualTimeline;

// Timeline tab, no length: the playable-frames fallback (a ruler of the content, never less than the minimum)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VisualTimelineNoLengthTest, "GSTN.Visual.Timeline.NoLength", GSTN_VISUAL_TEST_FLAGS)
bool FGSTN_VisualTimelineNoLengthTest::RunTest(const FString& Parameters)
{
	if (!GSTNVisualSnapshot::CanRender(*this))
	{
		return true;
	}
	FVisualFixture F;
	TestEqual(TEXT("the timeline has no length"), F.Timeline->Data.TotalFrames, 0);
	F.Model->SetSelection({ FGSTN_Selection::Window(1) });
	F.Model->SetPlayhead(9);
	TestFalse(TEXT("a snapshot was written"), RenderTimeline(*this, F, TEXT("timeline-no-length")).IsEmpty() );
	return true;
}

// Timeline tab with a length, a first actionable frame and a tail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VisualTimelineFullTest, "GSTN.Visual.Timeline.LengthFafAndTail", GSTN_VISUAL_TEST_FLAGS)
bool FGSTN_VisualTimelineFullTest::RunTest(const FString& Parameters)
{
	if (!GSTNVisualSnapshot::CanRender(*this))
	{
		return true;
	}
	FVisualFixture F;
	FGSTN_CombatTimelineData& Data = F.Timeline->Data;
	Data.TotalFrames = 40;
	Data.FirstActionableFrame = 30;
	Data.Tail.bHasTail = true;
	Data.Tail.StartFrame = INDEX_NONE;
	Data.Tail.BlendOutFrames = 6;
	F.Model->SetSelection({ FGSTN_Selection::Window(0) });
	F.Model->SetPlayhead(18);
	TestFalse(TEXT("a snapshot was written"), RenderTimeline(*this, F, TEXT("timeline-length-faf-tail")).IsEmpty());
	return true;
}

// Frame Data tab: the length fields, the summary line and the findings of a Validate

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_VisualFrameDataTest, "GSTN.Visual.Timeline.FrameDataWithFindings", GSTN_VISUAL_TEST_FLAGS)
bool FGSTN_VisualFrameDataTest::RunTest(const FString& Parameters)
{
	if (!GSTNVisualSnapshot::CanRender(*this))
	{
		return true;
	}
	FVisualFixture F;
	FGSTN_CombatTimelineData& Data = F.Timeline->Data;
	Data.TotalFrames = 40;
	Data.FirstActionableFrame = 30;
	Data.Tail.bHasTail = true;
	Data.Tail.BlendOutFrames = 6;

	const TSharedRef<FGSTN_FindingsStore> Findings = MakeShared<FGSTN_FindingsStore>();
	FGSTN_ValidationReport Report;
	Report.Add(EGSTN_Severity::Error, TEXT("Timeline.WindowOutsideTimeline"), FText::FromString(TEXT("Cancel Window (track 0) ends on frame 46, after the last frame (40)")), nullptr, FGSTN_FindingLocation::OnTrack(0, 46));
	Report.Add(EGSTN_Severity::Warning, TEXT("Timeline.HitboxWithoutPart"), FText::FromString(TEXT("Hitbox (track 5) names no part or group: it cannot be attached to a hurtbox")), nullptr, FGSTN_FindingLocation::OnTrack(5));
	Report.Add(EGSTN_Severity::Warning, TEXT("Timeline.WindowOverlapsTail"), FText::FromString(TEXT("Cancel Window (track 0) runs into the tail (frames 30 to 40): the tail already allows every cancel")), nullptr, FGSTN_FindingLocation::OnFrame(30));
	Report.Add(EGSTN_Severity::Info, TEXT("Timeline.NoStartup"), FText::FromString(TEXT("No active frames yet: the timeline has no hitbox with a frame range")));
	Findings->SetReport(Report);

	const TSharedRef<SWidget> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(8.f, 8.f, 8.f, 4.f))
		[
			SNew(SGSTN_TimelineLengthFields).Model(F.Model)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(8.f, 4.f))
		[
			SNew(SGSTN_FrameDataSummary).Model(F.Model)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(FMargin(8.f, 4.f, 8.f, 8.f))
		[
			SNew(SGSTN_FindingsList).Store(Findings)
		];
	TestFalse(TEXT("a snapshot was written"), GSTNVisualSnapshot::Render(*this, OnPanel(Content), FIntPoint(500, 400), TEXT("frame-data-findings")).IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
