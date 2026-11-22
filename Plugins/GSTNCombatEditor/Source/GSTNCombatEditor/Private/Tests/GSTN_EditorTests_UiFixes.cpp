#include "Editor.h"
#include "Editor/Transactor.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/AutomationTest.h"
#include "Settings/GSTN_CombatEditorSettings.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_FindingsStore.h"
#include "Timeline/GSTN_PhaseBarData.h"
#include "Timeline/GSTN_TimelineColors.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineGesture.h"
#include "Timeline/GSTN_TimelineUndo.h"
#include "Timeline/Widgets/SGSTN_FindingsList.h"
#include "Timeline/Widgets/SGSTN_TimelineLengthFields.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// The fixes of the UI test of 2026-10-23: a playhead that works while TotalFrames is 0, bar colours and the text on them, the findings in
// the Frame Data tab, and a click on empty space that clears the selection. The maths and the model run without a window; the widgets
// are only constructed, and only when the editor has a Slate application.

#define GSTN_UIFIX_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace GSTNUiFixTest
{
	template <typename T>
	TInstancedStruct<FGSTN_TimelineTrack> MakeFixTrack(int32 Start, int32 End)
	{
		T Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		return TInstancedStruct<FGSTN_TimelineTrack>::Make<T>(Track);
	}

	/** A transient timeline with no length (a new inline timeline) and the model and undo client on it */
	struct FNoLengthFixture
	{
		TStrongObjectPtr<UGSTN_CombatTimeline> Timeline;
		TUniquePtr<FGSTN_TimelineEditorModel> Model;
		TUniquePtr<FGSTN_TimelineUndoClient> UndoClient;

		FNoLengthFixture()
		{
			Timeline.Reset(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
			Model = MakeUnique<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
			Model->SetTickRate(62.5f);
			UndoClient = MakeUnique<FGSTN_TimelineUndoClient>(*Model);
		}

		~FNoLengthFixture()
		{
			UndoClient.Reset();
			Model.Reset();
		}

		FGSTN_CombatTimelineData& Data() { return Timeline->Data; }
	};

	int32 AppliedSteps()
	{
		return (GEditor && GEditor->Trans) ? GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() : 0;
	}
}

using namespace GSTNUiFixTest;

// Playhead without a length

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiFixPlayheadNoLengthTest, "GSTNCombat.Editor.UiFixes.PlayheadWorksWithoutLength", GSTN_UIFIX_TEST_FLAGS)
bool FGSTN_UiFixPlayheadNoLengthTest::RunTest(const FString& Parameters)
{
	FNoLengthFixture F;
	FGSTN_TimelineEditorModel& Model = *F.Model;

	// An empty timeline with no length: the minimum ruler
	constexpr int32 Ruler = FGSTN_TimelineEditorModel::MinimumRulerFrames;
	TestEqual(TEXT("an empty timeline plays over the minimum ruler"), Model.GetPlayableFrames(), Ruler);
	TestTrue(TEXT("a ruler click moves the playhead"), Model.SetPlayhead(7));
	TestEqual(TEXT("it is there"), Model.GetPlayhead(), 7);
	TestTrue(TEXT("Step Forward moves it"), Model.StepPlayhead(1));
	TestEqual(TEXT("one frame on"), Model.GetPlayhead(), 8);
	Model.SetPlayhead(500);
	TestEqual(TEXT("it stops on the last frame of the minimum ruler"), Model.GetPlayhead(), Ruler - 1);
	Model.SetPlayhead(-3);
	TestEqual(TEXT("and at zero"), Model.GetPlayhead(), 0);

	// Content past the ruler extends it
	F.Data().Tracks.Add(MakeFixTrack<FGSTN_TimelineTrack_CancelWindow>(30, 46));
	TestEqual(TEXT("the content end is the end of the last window"), FGSTN_TimelineEditorModel::GetContentEnd(F.Data()), 46);
	TestEqual(TEXT("a timeline with content longer than the ruler plays over the content"), Model.GetPlayableFrames(), 46);
	Model.SetPlayhead(500);
	TestEqual(TEXT("the playhead stops on the last frame of the content"), Model.GetPlayhead(), 45);

	// The first actionable frame counts as content, a point track occupies its frame
	F.Data().Tracks.Reset();
	F.Data().FirstActionableFrame = 40;
	TestEqual(TEXT("FAF is content"), FGSTN_TimelineEditorModel::GetContentEnd(F.Data()), 40);
	F.Data().FirstActionableFrame = 0;
	F.Data().Tracks.Add(MakeFixTrack<FGSTN_TimelineTrack_Event>(50, 51));
	TestEqual(TEXT("an event occupies one frame"), FGSTN_TimelineEditorModel::GetContentEnd(F.Data()), 51);

	// The loop range follows the same length
	F.Data().Tracks.Reset();
	int32 First = -1;
	int32 End = -1;
	Model.GetEffectivePlayRange(First, End);
	TestEqual(TEXT("loop from 0"), First, 0);
	TestEqual(TEXT("to the end of the ruler"), End, Ruler);

	// A real length wins, and a pure clamp agrees with the model
	F.Data().TotalFrames = 12;
	TestEqual(TEXT("the playable length is the total once it has one"), Model.GetPlayableFrames(), 12);
	TestEqual(TEXT("pure clamp, past the end"), FGSTN_TimelineEditorModel::ClampPlayheadFrame(F.Data(), 99), 11);
	TestEqual(TEXT("pure clamp, before the start"), FGSTN_TimelineEditorModel::ClampPlayheadFrame(F.Data(), -5), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiFixLengthUndoTest, "GSTNCombat.Editor.UiFixes.TotalFramesAndFafAreUndoable", GSTN_UIFIX_TEST_FLAGS)
bool FGSTN_UiFixLengthUndoTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the editor has a transaction buffer"), GEditor != nullptr && GEditor->Trans != nullptr))
	{
		return false;
	}
	FNoLengthFixture F;
	FGSTN_TimelineEditorModel& Model = *F.Model;

	// What the Length field does: one step per edit, undo returns to "no length"
	const int32 Steps = AppliedSteps();
	TestTrue(TEXT("Length 24"), Model.SetTotalFrames(24));
	TestEqual(TEXT("the timeline has its length"), F.Data().TotalFrames, 24);
	TestEqual(TEXT("one undo step"), AppliedSteps(), Steps + 1);
	TestFalse(TEXT("the same value is no step"), Model.SetTotalFrames(24));
	TestEqual(TEXT("still one"), AppliedSteps(), Steps + 1);
	TestTrue(TEXT("undo"), GEditor->UndoTransaction());
	TestEqual(TEXT("back to no length"), F.Data().TotalFrames, 0);
	TestTrue(TEXT("redo"), GEditor->RedoTransaction());
	TestEqual(TEXT("the length again"), F.Data().TotalFrames, 24);

	// A drag of the spin box (BeginDrag ... EndDrag around many SetTotalFrames) is one step
	const int32 Before = AppliedSteps();
	TestTrue(TEXT("the spin box opens a drag"), Model.BeginDrag(FText::FromString(TEXT("Set timeline length"))));
	for (int32 Value = 25; Value <= 40; ++Value)
	{
		Model.SetTotalFrames(Value);
	}
	Model.EndDrag();
	TestEqual(TEXT("sixteen changes, one step"), AppliedSteps(), Before + 1);
	TestEqual(TEXT("the last value stuck"), F.Data().TotalFrames, 40);
	TestTrue(TEXT("one undo takes the whole drag back"), GEditor->UndoTransaction());
	TestEqual(TEXT("to where the drag started"), F.Data().TotalFrames, 24);

	// The first actionable frame, the same
	const int32 FafBefore = AppliedSteps();
	TestTrue(TEXT("FAF 10"), Model.SetFirstActionableFrame(10));
	TestEqual(TEXT("one step"), AppliedSteps(), FafBefore + 1);
	TestTrue(TEXT("undo"), GEditor->UndoTransaction());
	TestEqual(TEXT("FAF back"), F.Data().FirstActionableFrame, 0);
	return true;
}

// Selection

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiFixEmptyClickTest, "GSTNCombat.Editor.UiFixes.EmptySpaceClickClearsSelection", GSTN_UIFIX_TEST_FLAGS)
bool FGSTN_UiFixEmptyClickTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the editor has a transaction buffer"), GEditor != nullptr && GEditor->Trans != nullptr))
	{
		return false;
	}
	FNoLengthFixture F;
	FGSTN_TimelineEditorModel& Model = *F.Model;
	F.Data().TotalFrames = 30;
	F.Data().Tracks.Add(MakeFixTrack<FGSTN_TimelineTrack_Tags>(0, 12));
	F.Data().Tracks.Add(MakeFixTrack<FGSTN_TimelineTrack_CancelWindow>(8, 14));
	int32 SelectionEvents = 0;
	Model.OnSelectionChanged.AddLambda([&SelectionEvents]() { ++SelectionEvents; });

	Model.SetSelection({ FGSTN_Selection::Window(0), FGSTN_Selection::Window(1) });
	TestEqual(TEXT("two selected"), Model.GetSelection().Num(), 2);
	const int32 Steps = AppliedSteps();
	const int32 EventsBefore = SelectionEvents;

	// Shift keeps the selection, as it does on an empty lane
	TestFalse(TEXT("Shift on empty space changes nothing"), FGSTN_TimelineActions::ClickEmptySpace(Model, true));
	TestEqual(TEXT("still two"), Model.GetSelection().Num(), 2);

	TestTrue(TEXT("a plain click on empty space clears it"), FGSTN_TimelineActions::ClickEmptySpace(Model, false));
	TestEqual(TEXT("nothing selected"), Model.GetSelection().Num(), 0);
	TestEqual(TEXT("announced once"), SelectionEvents, EventsBefore + 1);
	TestEqual(TEXT("it opened no transaction: the undo history is as it was"), AppliedSteps(), Steps);
	TestFalse(TEXT("nothing to clear the second time"), FGSTN_TimelineActions::ClickEmptySpace(Model, false));
	TestEqual(TEXT("so no new event"), SelectionEvents, EventsBefore + 1);

	// Undo-safe: the data edit before the clear still undoes, and the selection stays valid
	Model.SetSelection({ FGSTN_Selection::Window(1) });
	Model.MoveWindow(1, 2);
	FGSTN_TimelineActions::ClickEmptySpace(Model, false);
	TestTrue(TEXT("undo of the move after the clear"), GEditor->UndoTransaction());
	TestEqual(TEXT("the window is back"), F.Data().Tracks[1].Get<FGSTN_TimelineTrack>().StartFrame, 8);
	TestEqual(TEXT("and the selection is still empty"), Model.GetSelection().Num(), 0);
	return true;
}

// Colours

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiFixColoursTest, "GSTNCombat.Editor.UiFixes.TextIsReadableOnEveryBar", GSTN_UIFIX_TEST_FLAGS)
bool FGSTN_UiFixColoursTest::RunTest(const FString& Parameters)
{
	using namespace GSTNTimelineColors;

	TestEqual(TEXT("luminance of white"), RelativeLuminance(FLinearColor::White), 1.f, 1e-4f);
	TestEqual(TEXT("luminance of black"), RelativeLuminance(FLinearColor::Black), 0.f, 1e-4f);
	TestEqual(TEXT("black on white is 21:1"), ContrastRatio(FLinearColor::Black, FLinearColor::White), 21.f, 1e-3f);
	TestTrue(TEXT("green is brighter than blue"), RelativeLuminance(FLinearColor(0.f, 1.f, 0.f)) > RelativeLuminance(FLinearColor(0.f, 0.f, 1.f)));

	// Dark backgrounds get white text, bright ones black
	TestEqual(TEXT("white on a dark bar"), ReadableTextOn(FLinearColor(0.02f, 0.02f, 0.05f)).R, 1.f);
	TestEqual(TEXT("black on a pale bar"), ReadableTextOn(FLinearColor(0.9f, 0.85f, 0.9f)).R, 0.f);

	// Whatever the background, the text reads: a sweep over greys and a grid of colours, 4.5:1 is the readable threshold
	float Worst = 100.f;
	for (int32 Step = 0; Step <= 100; ++Step)
	{
		const float Level = Step / 100.f;
		Worst = FMath::Min(Worst, ContrastRatio(FLinearColor(Level, Level, Level), ReadableTextOn(FLinearColor(Level, Level, Level))));
	}
	for (int32 R = 0; R <= 4; ++R)
	{
		for (int32 G = 0; G <= 4; ++G)
		{
			for (int32 B = 0; B <= 4; ++B)
			{
				const FLinearColor Background(R / 4.f, G / 4.f, B / 4.f);
				Worst = FMath::Min(Worst, ContrastRatio(Background, ReadableTextOn(Background)));
			}
		}
	}
	TestTrue(*FString::Printf(TEXT("the worst text contrast over all backgrounds is %.2f, at least 4.5"), Worst), Worst >= 4.5f);

	// The bars the editor ships: every track colour of the settings, and the generic one
	const UGSTN_CombatEditorSettings& Settings = UGSTN_CombatEditorSettings::Get();
	TArray<FLinearColor> TrackColours;
	TrackColours.Add(Settings.GenericTrackColor);
	for (const FGSTN_TrackColor& Entry : Settings.TrackColors)
	{
		TrackColours.Add(Entry.Color);
	}
	// And the pale ones that made the old bars unreadable
	TrackColours.Add(FLinearColor(1.f, 0.8f, 0.8f));
	TrackColours.Add(FLinearColor(0.8f, 1.f, 1.f));
	TrackColours.Add(FLinearColor::White);
	for (const FLinearColor& Colour : TrackColours)
	{
		const FLinearColor Fill = BarFill(Colour);
		TestTrue(TEXT("a bar fill is opaque"), Fill.A == 1.f);
		TestTrue(TEXT("a bar fill is darker than the track colour it comes from"), RelativeLuminance(Fill) <= RelativeLuminance(Colour) + 1e-4f);
		TestTrue(TEXT("a bar fill is a dark one: at most 0.4 of full luminance, so a pale track colour does not give a pale bar"), RelativeLuminance(Fill) <= 0.4f + 1e-4f);
		TestTrue(TEXT("and the text picked for it reads"), ContrastRatio(Fill, ReadableTextOn(Fill)) >= 4.5f);
		TestTrue(TEXT("the outline is the track colour"), BarOutline(Colour).R == Colour.R && BarOutline(Colour).G == Colour.G && BarOutline(Colour).B == Colour.B);
	}

	// The phase colours: text reads on each, and startup, active and recovery are far apart
	const EGSTN_PhaseKind Kinds[] = { EGSTN_PhaseKind::Startup, EGSTN_PhaseKind::Active, EGSTN_PhaseKind::Recovery, EGSTN_PhaseKind::Actionable, EGSTN_PhaseKind::Busy };
	for (const EGSTN_PhaseKind Kind : Kinds)
	{
		TestTrue(*FString::Printf(TEXT("text reads on the %s phase"), PhaseName(Kind)), ContrastRatio(PhaseFill(Kind), ReadableTextOn(PhaseFill(Kind))) >= 4.5f);
	}
	for (int32 A = 0; A < UE_ARRAY_COUNT(Kinds); ++A)
	{
		for (int32 B = A + 1; B < UE_ARRAY_COUNT(Kinds); ++B)
		{
			const FLinearColor ColourA = PhaseFill(Kinds[A]);
			const FLinearColor ColourB = PhaseFill(Kinds[B]);
			const float Distance = FMath::Sqrt(FMath::Square(ColourA.R - ColourB.R) + FMath::Square(ColourA.G - ColourB.G) + FMath::Square(ColourA.B - ColourB.B));
			TestTrue(*FString::Printf(TEXT("%s and %s are told apart by colour"), PhaseName(Kinds[A]), PhaseName(Kinds[B])), Distance >= 0.2f);
		}
	}

	// The selected row header is dark with white text, not the pale cell of the first test
	TestTrue(TEXT("selected header is dark"), RelativeLuminance(SelectedHeaderFill()) < 0.15f);
	TestEqual(TEXT("with white text"), ReadableTextOn(SelectedHeaderFill()).R, 1.f);
	return true;
}

// Findings in the Frame Data tab

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiFixFindingsTest, "GSTNCombat.Editor.UiFixes.FindingsStoreAndList", GSTN_UIFIX_TEST_FLAGS)
bool FGSTN_UiFixFindingsTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FGSTN_FindingsStore> Store = MakeShared<FGSTN_FindingsStore>();
	int32 Changes = 0;
	Store->OnChanged.AddLambda([&Changes]() { ++Changes; });

	TestFalse(TEXT("nothing ran yet"), Store->HasRun());
	TestTrue(TEXT("the headline says to run Validate"), Store->GetHeadline().ToString().Contains(TEXT("not run")));
	Store->MarkStale();
	TestEqual(TEXT("marking stale before a run is nothing"), Changes, 0);

	// The report of a run, the same one the Message Log gets
	FGSTN_ValidationReport Report;
	Report.Add(EGSTN_Severity::Error, TEXT("Timeline.FrameRange"), FText::FromString(TEXT("TotalFrames is 0")), nullptr, FGSTN_FindingLocation::OnFrame(0));
	Report.Add(EGSTN_Severity::Warning, TEXT("Timeline.UnknownTrack"), FText::FromString(TEXT("Track 0 has no track editor")), nullptr, FGSTN_FindingLocation::OnTrack(0));
	Report.Add(EGSTN_Severity::Info, TEXT("Timeline.Note"), FText::FromString(TEXT("A note")));
	Store->SetReport(Report);
	TestTrue(TEXT("it ran"), Store->HasRun());
	TestEqual(TEXT("three findings kept"), Store->GetReport().Num(), 3);
	TestEqual(TEXT("announced"), Changes, 1);
	const FString Headline = Store->GetHeadline().ToString();
	TestTrue(TEXT("the headline counts errors"), Headline.Contains(TEXT("1 errors")));
	TestTrue(TEXT("and warnings"), Headline.Contains(TEXT("1 warnings")));

	Store->MarkStale();
	TestTrue(TEXT("an edit makes them stale"), Store->IsStale());
	TestEqual(TEXT("announced"), Changes, 2);
	TestEqual(TEXT("the findings stay"), Store->GetReport().Num(), 3);
	TestTrue(TEXT("the headline says they are out of date"), Store->GetHeadline().ToString().Contains(TEXT("Validate again")));
	Store->MarkStale();
	TestEqual(TEXT("marking twice announces once"), Changes, 2);

	Store->SetReport(FGSTN_ValidationReport());
	TestFalse(TEXT("a new run is fresh"), Store->IsStale());
	TestTrue(TEXT("a clean run says so"), Store->GetHeadline().ToString().Contains(TEXT("No problems")));

	// The list shows one row per finding
	if (FSlateApplication::IsInitialized())
	{
		Store->SetReport(Report);
		const TSharedRef<SGSTN_FindingsList> List = SNew(SGSTN_FindingsList).Store(Store);
		TestEqual(TEXT("one row per finding"), List->GetRowCount(), 3);
		Store->SetReport(FGSTN_ValidationReport());
		TestEqual(TEXT("a new report replaces the rows"), List->GetRowCount(), 0);
	}
	else
	{
		AddInfo(TEXT("No Slate application in this run: the list widget was not constructed."));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiFixGoToFindingTest, "GSTNCombat.Editor.UiFixes.GoToFindingSelectsAndMoves", GSTN_UIFIX_TEST_FLAGS)
bool FGSTN_UiFixGoToFindingTest::RunTest(const FString& Parameters)
{
	FNoLengthFixture F;
	FGSTN_TimelineEditorModel& Model = *F.Model;
	F.Data().TotalFrames = 30;
	F.Data().Tracks.Add(MakeFixTrack<FGSTN_TimelineTrack_Tags>(0, 12));
	F.Data().Tracks.Add(MakeFixTrack<FGSTN_TimelineTrack_CancelWindow>(8, 14));

	TestTrue(TEXT("a finding on a track and a frame moves both"), FGSTN_TimelineActions::GoToFinding(Model, FGSTN_FindingLocation::OnTrack(1, 9)));
	TestTrue(TEXT("the track is selected"), Model.IsSelected(FGSTN_Selection::Window(1)));
	TestEqual(TEXT("the playhead is on the frame"), Model.GetPlayhead(), 9);

	TestTrue(TEXT("a finding on a frame only moves the playhead"), FGSTN_TimelineActions::GoToFinding(Model, FGSTN_FindingLocation::OnFrame(20)));
	TestEqual(TEXT("the selection is untouched"), Model.GetSelection().Num(), 1);
	TestEqual(TEXT("the playhead moved"), Model.GetPlayhead(), 20);

	TestFalse(TEXT("a finding about the whole timeline moves nothing"), FGSTN_TimelineActions::GoToFinding(Model, FGSTN_FindingLocation()));
	TestFalse(TEXT("a track that is gone moves nothing"), FGSTN_TimelineActions::GoToFinding(Model, FGSTN_FindingLocation::OnTrack(9)));
	TestEqual(TEXT("the selection is as it was"), Model.GetSelection().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiFixLengthWidgetTest, "GSTNCombat.Editor.UiFixes.LengthFieldsConstruct", GSTN_UIFIX_TEST_FLAGS)
bool FGSTN_UiFixLengthWidgetTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("No Slate application in this run: the widget was not constructed."));
		return true;
	}
	FNoLengthFixture F;
	const TSharedRef<FGSTN_TimelineEditorModel> Shared = MakeShared<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(F.Timeline.Get()));
	const TSharedRef<SGSTN_TimelineLengthFields> Fields = SNew(SGSTN_TimelineLengthFields).Model(Shared);
	TestTrue(TEXT("the length fields were built on a timeline that has no length"), Fields->GetChildren() != nullptr);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
