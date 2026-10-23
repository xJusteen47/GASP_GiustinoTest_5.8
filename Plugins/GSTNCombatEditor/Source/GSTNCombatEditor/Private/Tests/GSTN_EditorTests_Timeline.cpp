#include "Editor.h"
#include "Editor/Transactor.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Tests/GSTN_EditorTestTypes.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_KeyedTrackAccess.h"
#include "Timeline/GSTN_PhaseBarData.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineEditorCommands.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineGesture.h"
#include "Timeline/GSTN_TimelineLayout.h"
#include "Timeline/GSTN_TimelinePanelTest.h"
#include "Timeline/GSTN_TimelineUndo.h"
#include "Timeline/Widgets/SGSTN_FrameDataSummary.h"
#include "Timeline/Widgets/SGSTN_TimelinePanel.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// The tests of the timeline panel (plan m5 E4): the maths and the gestures run on the model with no window. The widgets are only
// constructed, and only when the editor has a Slate application; painting and mouse input are the manual smoke steps of the commit.

#define GSTN_UI_TEST_FLAGS (EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

namespace GSTNTimelineUiTest
{
	using FUiTrack = TInstancedStruct<FGSTN_TimelineTrack>;

	template <typename T>
	FUiTrack MakeUiTrack(int32 Start, int32 End)
	{
		T Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		return FUiTrack::Make<T>(Track);
	}

	FGSTN_TestKeyedTrack MakeUiKeyed(int32 Start, int32 End, std::initializer_list<int32> KeyFrames)
	{
		FGSTN_TestKeyedTrack Track;
		Track.StartFrame = Start;
		Track.EndFrame = End;
		for (const int32 Frame : KeyFrames)
		{
			FGSTN_TestKey Key;
			Key.Frame = Frame;
			Key.Location = FVector(Frame * 10.0, 0.0, 0.0);
			Track.Keys.Add(Key);
		}
		return Track;
	}

	/** Track indices of the fixture */
	constexpr int32 UiTags = 0;
	constexpr int32 UiCancel = 1;
	constexpr int32 UiMovement = 2;
	constexpr int32 UiEvent = 3;
	constexpr int32 UiHitbox = 4;
	constexpr int32 UiKeyed = 5;
	constexpr int32 UiKeyedWide = 6;

	/**
	 * 30 frames, actionable from 12, a tail that follows the actionable frame. Tags [0,12), cancel [8,14), movement [4,10), event at 5,
	 * hitbox [6,9), keyed [2,20) with keys at 2, 10 and 18, keyed [0,30) with keys at 5 and 6.
	 */
	void FillUiData(FGSTN_CombatTimelineData& Data)
	{
		Data.TotalFrames = 30;
		Data.FirstActionableFrame = 12;
		Data.Tail.bHasTail = true;
		Data.Tail.StartFrame = INDEX_NONE;
		Data.Tail.BlendOutFrames = 4;
		Data.Tracks.Add(MakeUiTrack<FGSTN_TimelineTrack_Tags>(0, 12));
		Data.Tracks.Add(MakeUiTrack<FGSTN_TimelineTrack_CancelWindow>(8, 14));
		Data.Tracks.Add(MakeUiTrack<FGSTN_TimelineTrack_Movement>(4, 10));
		Data.Tracks.Add(MakeUiTrack<FGSTN_TimelineTrack_Event>(5, 6));
		Data.Tracks.Add(MakeUiTrack<FGSTN_TimelineTrack_Hitbox>(6, 9));
		Data.Tracks.Add(FUiTrack::Make<FGSTN_TestKeyedTrack>(MakeUiKeyed(2, 20, { 2, 10, 18 })));
		Data.Tracks.Add(FUiTrack::Make<FGSTN_TestKeyedTrack>(MakeUiKeyed(0, 30, { 5, 6 })));
	}

	/** A view where frame f starts at pixel 10 * f, so the numbers in the tests can be read off */
	FGSTN_TimelineViewTransform UiView()
	{
		FGSTN_TimelineViewTransform View;
		View.FirstVisibleFrame = 0.0;
		View.PixelsPerFrame = 10.0;
		return View;
	}

	/** x of the middle of a frame at UiView */
	double UiMid(int32 Frame) { return 10.0 * Frame + 5.0; }

	FString UiExport(const FGSTN_CombatTimelineData& Data)
	{
		FString Text;
		FGSTN_CombatTimelineData::StaticStruct()->ExportText(Text, &Data, nullptr, nullptr, PPF_None, nullptr);
		return Text;
	}

	/** Transaction steps currently applied: grows by one per finished transaction, shrinks per undo */
	int32 UiAppliedSteps()
	{
		return (GEditor && GEditor->Trans) ? GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() : 0;
	}

	/** A transient timeline filled with FillUiData, a model on it (with the view of UiView) and the undo client, test keyed adapter registered */
	struct FUiFixture
	{
		TStrongObjectPtr<UGSTN_CombatTimeline> Timeline;
		TSharedPtr<FGSTN_TimelineEditorModel> Model;
		TUniquePtr<FGSTN_TimelineUndoClient> UndoClient;
		FGSTN_TimelineGesture Gesture;

		static inline int32 LiveFixtures = 0;

		FUiFixture()
		{
			Timeline.Reset(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transactional));
			FillUiData(Timeline->Data);
			if (LiveFixtures++ == 0)
			{
				FGSTN_KeyedTrackAccessRegistry::Register(FGSTN_TestKeyedTrack::StaticStruct(), MakeShared<FGSTN_TestKeyedAccess>());
			}
			Model = MakeShared<FGSTN_TimelineEditorModel>(FGSTN_TimelineEditContext::ForAsset(Timeline.Get()));
			Model->SetTickRate(62.5f);
			Model->ViewTransform = UiView();
			UndoClient = MakeUnique<FGSTN_TimelineUndoClient>(*Model);
		}

		~FUiFixture()
		{
			UndoClient.Reset();
			Model.Reset();
			if (--LiveFixtures == 0)
			{
				FGSTN_KeyedTrackAccessRegistry::Unregister(FGSTN_TestKeyedTrack::StaticStruct());
			}
		}

		FGSTN_CombatTimelineData& Data() { return Timeline->Data; }
		FGSTN_TimelineEditorModel& M() { return *Model; }
		FGSTN_TestKeyedTrack& Keyed(int32 Index = UiKeyed) { return Data().Tracks[Index].GetMutable<FGSTN_TestKeyedTrack>(); }

		/** Mouse-down at lane pixel x on a track, as the lane does: hit-test, then arm the gesture or start a box */
		bool Press(int32 TrackIndex, double X, bool bShift = false, bool bCtrl = false)
		{
			const FGSTN_LaneHit Hit = FGSTN_TimelineLayout::HitTestTrack(Data(), TrackIndex, M().ViewTransform, X);
			if (Hit.IsHit())
			{
				return Gesture.BeginOnHit(M(), Hit, X, bShift, bCtrl);
			}
			Gesture.BeginBoxSelect(M(), X, TrackIndex * FGSTN_TimelineLayout::RowHeight + 10.0, bShift);
			return true;
		}
	};
}

using namespace GSTNTimelineUiTest;

// Maths

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiViewTransformTest, "GSTNCombat.Editor.Timeline.ViewTransformRoundTrip", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiViewTransformTest::RunTest(const FString& Parameters)
{
	// Frame to pixel and back at several zooms and scroll offsets: the same frame, and a frame occupies [f, f + 1)
	for (const double PixelsPerFrame : { 1.0, 3.5, 12.0, 64.0, 256.0 })
	{
		for (const double First : { -5.0, 0.0, 7.25, 400.0 })
		{
			FGSTN_TimelineViewTransform View;
			View.PixelsPerFrame = PixelsPerFrame;
			View.FirstVisibleFrame = First;
			for (const int32 Frame : { -3, 0, 1, 29, 1000 })
			{
				const double Pixel = View.FrameToPixel(Frame);
				TestEqual(*FString::Printf(TEXT("round trip of frame %d at %.1f px/frame, first %.2f"), Frame, PixelsPerFrame, First), View.PixelToFrame(Pixel), static_cast<double>(Frame), 1e-9);
				TestEqual(TEXT("a pixel just inside the start of a frame is in that frame"), View.PixelToFrameIndex(Pixel + PixelsPerFrame * 0.01), Frame);
				TestEqual(TEXT("the last pixel before the next frame is still in it"), View.PixelToFrameIndex(Pixel + PixelsPerFrame * 0.999), Frame);
			}
		}
	}

	// Ctrl+wheel keeps the frame under the cursor where it is, and the zoom stays inside its limits
	FGSTN_TimelineViewTransform View = UiView();
	const double UnderCursor = View.PixelToFrame(137.0);
	FGSTN_TimelineLayout::ApplyWheel(View, 137.0, 3.0, true, 800.0, 100);
	TestTrue(TEXT("wheel up zoomed in"), View.PixelsPerFrame > 10.0);
	TestEqual(TEXT("the frame under the cursor stayed"), View.PixelToFrame(137.0), UnderCursor, 1e-6);
	for (int32 i = 0; i < 100; ++i)
	{
		FGSTN_TimelineLayout::ApplyWheel(View, 137.0, -5.0, true, 800.0, 100);
	}
	TestEqual(TEXT("zoom stops at the minimum"), View.PixelsPerFrame, FGSTN_TimelineViewTransform::MinPixelsPerFrame);

	// Plain wheel scrolls and cannot leave the timeline behind (200 px show 20 frames of a 100 frame timeline)
	View = UiView();
	FGSTN_TimelineLayout::ApplyWheel(View, 0.0, -100.0, false, 200.0, 100);
	TestTrue(TEXT("wheel down scrolled right"), View.FirstVisibleFrame > 0.0);
	TestTrue(TEXT("but not past the end of the timeline"), View.FirstVisibleFrame <= 100.0);
	FGSTN_TimelineLayout::ApplyWheel(View, 0.0, 10000.0, false, 200.0, 100);
	TestTrue(TEXT("nor far before its start"), View.FirstVisibleFrame >= -200.0 / View.PixelsPerFrame * 0.25 - 1e-9);

	// Frame all shows the whole timeline
	FGSTN_TimelineLayout::FrameAll(View, 600.0, 30);
	TestTrue(TEXT("frame 0 is visible after frame all"), View.FrameToPixel(0) >= 0.0);
	TestTrue(TEXT("the last frame ends inside the width"), View.FrameToPixel(30) <= 600.0 + 1e-6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiRulerTickSpacingTest, "GSTNCombat.Editor.Timeline.RulerTickSpacing", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiRulerTickSpacingTest::RunTest(const FString& Parameters)
{
	int32 PreviousLabelStep = 0;
	for (double PixelsPerFrame = 256.0; PixelsPerFrame >= 1.0; PixelsPerFrame -= 0.5)
	{
		FGSTN_TimelineViewTransform View;
		View.PixelsPerFrame = PixelsPerFrame;
		View.FirstVisibleFrame = 3.0;
		const FGSTN_RulerTicks Ticks = FGSTN_TimelineLayout::ComputeRulerTicks(View, 900.0);

		TestTrue(*FString::Printf(TEXT("labels are not closer than the minimum at %.1f px/frame"), PixelsPerFrame), Ticks.LabelStep * PixelsPerFrame >= FGSTN_TimelineLayout::MinLabelSpacingPixels);
		TestTrue(TEXT("the minor step divides the label step"), Ticks.LabelStep % Ticks.MinorStep == 0);
		TestTrue(TEXT("minor ticks keep a few pixels apart"), Ticks.MinorStep * PixelsPerFrame >= 4.0);
		TestTrue(TEXT("labels thin out as the view zooms out"), Ticks.LabelStep >= PreviousLabelStep);
		TestTrue(TEXT("the ticks cover the left edge"), Ticks.FirstFrame <= 3);
		TestTrue(TEXT("and the right edge"), Ticks.LastFrame >= View.PixelToFrame(900.0));
		PreviousLabelStep = Ticks.LabelStep;
	}

	FGSTN_TimelineViewTransform Close;
	Close.PixelsPerFrame = 100.0;
	TestEqual(TEXT("every frame is labelled when zoomed in"), FGSTN_TimelineLayout::ComputeRulerTicks(Close, 800.0).LabelStep, 1);
	FGSTN_TimelineViewTransform Far;
	Far.PixelsPerFrame = 1.0;
	TestEqual(TEXT("a label every 50 frames at one pixel per frame"), FGSTN_TimelineLayout::ComputeRulerTicks(Far, 800.0).LabelStep, 50);

	TestEqual(TEXT("milliseconds of frame 3 at 62.5 Hz"), FGSTN_TimelineLayout::FormatMilliseconds(3, 62.5f), FString(TEXT("48.0")));
	TestEqual(TEXT("one decimal at 60 Hz"), FGSTN_TimelineLayout::FormatMilliseconds(1, 60.f), FString(TEXT("16.7")));
	TestEqual(TEXT("no tick rate gives zero, not a division by zero"), FGSTN_TimelineLayout::FormatMilliseconds(5, 0.f), FString(TEXT("0.0")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiHitTestWindowEdgesTest, "GSTNCombat.Editor.Timeline.HitTestWindowEdges", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiHitTestWindowEdgesTest::RunTest(const FString& Parameters)
{
	const FGSTN_TimelineViewTransform View = UiView();
	auto Hit = [&View](int32 Start, int32 End, bool bPoint, double X) { return FGSTN_TimelineLayout::HitTestWindow(Start, End, bPoint, View, X); };

	// Window [4, 10) spans pixels 40..100; the grab zone is 5 px on each side of an edge
	TestTrue(TEXT("on the start edge"), Hit(4, 10, false, 40.0) == EGSTN_LaneHitKind::StartEdge);
	TestTrue(TEXT("just inside the start edge"), Hit(4, 10, false, 44.0) == EGSTN_LaneHitKind::StartEdge);
	TestTrue(TEXT("just outside the start edge"), Hit(4, 10, false, 36.0) == EGSTN_LaneHitKind::StartEdge);
	TestTrue(TEXT("past the grab zone is the body"), Hit(4, 10, false, 46.0) == EGSTN_LaneHitKind::Body);
	TestTrue(TEXT("the middle is the body"), Hit(4, 10, false, 70.0) == EGSTN_LaneHitKind::Body);
	TestTrue(TEXT("before the end grab zone"), Hit(4, 10, false, 94.0) == EGSTN_LaneHitKind::Body);
	TestTrue(TEXT("inside the end grab zone"), Hit(4, 10, false, 96.0) == EGSTN_LaneHitKind::EndEdge);
	TestTrue(TEXT("outside the end edge"), Hit(4, 10, false, 104.0) == EGSTN_LaneHitKind::EndEdge);
	TestTrue(TEXT("beyond the grab zone is nothing"), Hit(4, 10, false, 106.0) == EGSTN_LaneHitKind::None);
	TestTrue(TEXT("left of the window is nothing"), Hit(4, 10, false, 30.0) == EGSTN_LaneHitKind::None);

	// A one-frame bar is 10 px: the grab zone shrinks to a third so the body stays reachable
	TestTrue(TEXT("middle of a one-frame window"), Hit(4, 5, false, 45.0) == EGSTN_LaneHitKind::Body);
	TestTrue(TEXT("its start"), Hit(4, 5, false, 41.0) == EGSTN_LaneHitKind::StartEdge);
	TestTrue(TEXT("its end"), Hit(4, 5, false, 49.0) == EGSTN_LaneHitKind::EndEdge);

	// An inverted window (the validator reports it) still gets a bar to grab
	TestTrue(TEXT("inverted window is grabbable"), Hit(10, 4, false, 105.0) != EGSTN_LaneHitKind::None);

	// A point track (event, weapon switch) has no edges: a diamond at the middle of its frame with a grab radius of at least 6 px
	TestTrue(TEXT("point: centre"), Hit(5, 6, true, 55.0) == EGSTN_LaneHitKind::Body);
	TestTrue(TEXT("point: within reach"), Hit(5, 6, true, 60.0) == EGSTN_LaneHitKind::Body);
	TestTrue(TEXT("point: out of reach"), Hit(5, 6, true, 62.0) == EGSTN_LaneHitKind::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiHitTestTrackTest, "GSTNCombat.Editor.Timeline.HitTestTrackPriority", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiHitTestTrackTest::RunTest(const FString& Parameters)
{
	FUiFixture F;
	const FGSTN_TimelineViewTransform View = UiView();

	// The keyed track [2, 20) has keys at 2, 10, 18, drawn at the middle of their frames: 25, 105, 185
	FGSTN_LaneHit Hit = FGSTN_TimelineLayout::HitTestTrack(F.Data(), UiKeyed, View, 105.0);
	TestTrue(TEXT("on a key"), Hit.Kind == EGSTN_LaneHitKind::Key && Hit.KeyIndex == 1 && Hit.TrackIndex == UiKeyed);
	TestTrue(TEXT("the selection entry of the hit is that key"), Hit.ToSelection() == FGSTN_Selection::Key(UiKeyed, 1));
	Hit = FGSTN_TimelineLayout::HitTestTrack(F.Data(), UiKeyed, View, 111.0);
	TestTrue(TEXT("a key is grabbed from 6 px away"), Hit.Kind == EGSTN_LaneHitKind::Key && Hit.KeyIndex == 1);
	Hit = FGSTN_TimelineLayout::HitTestTrack(F.Data(), UiKeyed, View, 112.0);
	TestTrue(TEXT("and not from further: that is the bar"), Hit.Kind == EGSTN_LaneHitKind::Body);
	Hit = FGSTN_TimelineLayout::HitTestTrack(F.Data(), UiKeyed, View, 21.0);
	TestTrue(TEXT("a key at the start of the bar wins over the bar's edge"), Hit.Kind == EGSTN_LaneHitKind::Key && Hit.KeyIndex == 0);
	Hit = FGSTN_TimelineLayout::HitTestTrack(F.Data(), UiKeyed, View, 18.0);
	TestTrue(TEXT("beside it the edge is reachable"), Hit.Kind == EGSTN_LaneHitKind::StartEdge);
	TestTrue(TEXT("its selection entry names the part"), Hit.ToSelection() == FGSTN_Selection::Window(UiKeyed, EGSTN_SelectionPart::Start));

	// A track without keys: edges and body. A miss reports no track at all
	Hit = FGSTN_TimelineLayout::HitTestTrack(F.Data(), UiTags, View, 118.0);
	TestTrue(TEXT("end edge of the tags window"), Hit.Kind == EGSTN_LaneHitKind::EndEdge && Hit.TrackIndex == UiTags);
	Hit = FGSTN_TimelineLayout::HitTestTrack(F.Data(), UiTags, View, 250.0);
	TestFalse(TEXT("empty lane space is a miss"), Hit.IsHit());
	TestEqual(TEXT("a miss names no track"), Hit.TrackIndex, static_cast<int32>(INDEX_NONE));
	TestFalse(TEXT("a track that does not exist is a miss"), FGSTN_TimelineLayout::HitTestTrack(F.Data(), 99, View, 10.0).IsHit());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiBoxSelectTest, "GSTNCombat.Editor.Timeline.BoxSelect", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiBoxSelectTest::RunTest(const FString& Parameters)
{
	FUiFixture F;
	const FGSTN_TimelineViewTransform View = UiView();
	const double Row = FGSTN_TimelineLayout::RowHeight;

	// Rows 0 and 1 (y 5..30), pixels 0..150: the tags and cancel windows
	TArray<FGSTN_Selection> Picked = FGSTN_TimelineLayout::BoxSelect(F.Data(), View, 0.0, 5.0, 150.0, 30.0);
	TestEqual(TEXT("two rows touched"), Picked.Num(), 2);
	TestTrue(TEXT("tags window"), Picked.Contains(FGSTN_Selection::Window(UiTags)));
	TestTrue(TEXT("cancel window"), Picked.Contains(FGSTN_Selection::Window(UiCancel)));

	// Every row, pixels 0..45: windows that reach into it, and the keys inside it instead of the window of a keyed track
	Picked = FGSTN_TimelineLayout::BoxSelect(F.Data(), View, 0.0, 0.0, 45.0, Row * 7 - 1.0);
	TestEqual(TEXT("four things inside the box"), Picked.Num(), 4);
	TestTrue(TEXT("tags (0..120 px)"), Picked.Contains(FGSTN_Selection::Window(UiTags)));
	TestTrue(TEXT("movement (40..100 px)"), Picked.Contains(FGSTN_Selection::Window(UiMovement)));
	TestTrue(TEXT("the key at frame 2 and not the window of its track"), Picked.Contains(FGSTN_Selection::Key(UiKeyed, 0)) && !Picked.Contains(FGSTN_Selection::Window(UiKeyed)));
	TestTrue(TEXT("a keyed track with no key inside is selected by its window"), Picked.Contains(FGSTN_Selection::Window(UiKeyedWide)));
	TestFalse(TEXT("cancel (80 px on) is outside"), Picked.Contains(FGSTN_Selection::Window(UiCancel)));
	TestFalse(TEXT("so is the hitbox (60 px on)"), Picked.Contains(FGSTN_Selection::Window(UiHitbox)));

	// The corners may come in any order, and rows outside the data are ignored
	const TArray<FGSTN_Selection> Reversed = FGSTN_TimelineLayout::BoxSelect(F.Data(), View, 45.0, Row * 7 - 1.0, 0.0, 0.0);
	TestEqual(TEXT("same box, opposite corners"), Reversed.Num(), Picked.Num());
	TestEqual(TEXT("a box above the first row selects row 0 only"), FGSTN_TimelineLayout::BoxSelect(F.Data(), View, 0.0, -50.0, 10.0, 10.0).Num(), 1);
	TestEqual(TEXT("a box far below the last row stops at the last (tags, and the wide keyed track with no key in reach)"), FGSTN_TimelineLayout::BoxSelect(F.Data(), View, 0.0, 0.0, 10.0, 100000.0).Num(), 2);
	TestEqual(TEXT("a box right of everything selects nothing"), FGSTN_TimelineLayout::BoxSelect(F.Data(), View, 400.0, 0.0, 450.0, 100.0).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiNavigationTest, "GSTNCombat.Editor.Timeline.NavigationAndPlayback", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiNavigationTest::RunTest(const FString& Parameters)
{
	FUiFixture F;

	// Edges and keys of the fixture: 0 2 4 5 6 8 9 10 11 13 18 19 29
	TestEqual(TEXT("next after 0"), FGSTN_TimelineLayout::FindAdjacentEdge(F.Data(), 0, 1), 2);
	TestEqual(TEXT("next after 6"), FGSTN_TimelineLayout::FindAdjacentEdge(F.Data(), 6, 1), 8);
	TestEqual(TEXT("next from between two"), FGSTN_TimelineLayout::FindAdjacentEdge(F.Data(), 14, 1), 18);
	TestEqual(TEXT("nothing after the last"), FGSTN_TimelineLayout::FindAdjacentEdge(F.Data(), 29, 1), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("previous before 8"), FGSTN_TimelineLayout::FindAdjacentEdge(F.Data(), 8, -1), 6);
	TestEqual(TEXT("previous before 2"), FGSTN_TimelineLayout::FindAdjacentEdge(F.Data(), 2, -1), 0);
	TestEqual(TEXT("previous from the end"), FGSTN_TimelineLayout::FindAdjacentEdge(F.Data(), 30, -1), 29);
	TestEqual(TEXT("nothing before the first"), FGSTN_TimelineLayout::FindAdjacentEdge(F.Data(), 0, -1), static_cast<int32>(INDEX_NONE));

	F.M().SetPlayhead(6);
	TestTrue(TEXT("the action moves the playhead to the next edge"), FGSTN_TimelineActions::JumpToAdjacentEdge(F.M(), 1));
	TestEqual(TEXT("playhead after jump"), F.M().GetPlayhead(), 8);
	TestTrue(TEXT("and back"), FGSTN_TimelineActions::JumpToAdjacentEdge(F.M(), -1));
	TestEqual(TEXT("playhead after jump back"), F.M().GetPlayhead(), 6);

	// The span of a selection: movement [4,10) and the key at 18
	int32 First = 0;
	int32 End = 0;
	const TArray<FGSTN_Selection> Selection = { FGSTN_Selection::Window(UiMovement), FGSTN_Selection::Key(UiKeyed, 2) };
	TestTrue(TEXT("a selection has a span"), FGSTN_TimelineLayout::GetSelectionSpan(F.Data(), Selection, First, End));
	TestEqual(TEXT("span start"), First, 4);
	TestEqual(TEXT("span end covers the last key's frame"), End, 19);
	TestFalse(TEXT("an empty selection has none"), FGSTN_TimelineLayout::GetSelectionSpan(F.Data(), TConstArrayView<FGSTN_Selection>(), First, End));

	// Playback hands out whole frames from real time, keeps the remainder, and does not jump after a stall
	FGSTN_PlaybackClock Clock;
	TestEqual(TEXT("0.125 s at 62.5 Hz is 7.8125 frames"), Clock.Advance(0.125, 62.5f), 7);
	TestEqual(TEXT("the remainder is kept: 7.8125 + 0.8125"), Clock.Advance(0.125, 62.5f), 8);
	TestEqual(TEXT("no time, no frames"), Clock.Advance(0.0, 62.5f), 0);
	TestEqual(TEXT("no rate, no frames"), Clock.Advance(0.1, 0.f), 0);
	Clock.Reset();
	TestEqual(TEXT("a stall of ten seconds counts as a quarter of a second"), Clock.Advance(10.0, 62.5f), 15);
	TestEqual(TEXT("stepping wraps around the end"), FGSTN_PlaybackClock::StepLooped(28, 5, 0, 30), 3);
	TestEqual(TEXT("and the start"), FGSTN_PlaybackClock::StepLooped(1, -3, 0, 30), 28);
	TestEqual(TEXT("an empty range gives its start"), FGSTN_PlaybackClock::StepLooped(5, 1, 7, 7), 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiPhaseBarDataTest, "GSTNCombat.Editor.Timeline.PhaseBarData", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiPhaseBarDataTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatTimelineData Data;
	FillUiData(Data);

	// One hitbox [6, 9), actionable from 12
	FGSTN_PhaseBarData Phases = FGSTN_PhaseBarData::Compute(Data);
	TestTrue(TEXT("has an active range"), Phases.bHasActive);
	TestEqual(TEXT("startup"), Phases.StartupFrames, 6);
	TestEqual(TEXT("active"), Phases.ActiveFrames, 3);
	TestEqual(TEXT("recovery"), Phases.RecoveryFrames, 3);
	TestEqual(TEXT("segments: startup, active, recovery, actionable"), Phases.Segments.Num(), 4);
	if (Phases.Segments.Num() == 4)
	{
		TestTrue(TEXT("startup [0,6)"), Phases.Segments[0].Kind == EGSTN_PhaseKind::Startup && Phases.Segments[0].Start == 0 && Phases.Segments[0].End == 6);
		TestTrue(TEXT("active [6,9)"), Phases.Segments[1].Kind == EGSTN_PhaseKind::Active && Phases.Segments[1].Start == 6 && Phases.Segments[1].End == 9);
		TestTrue(TEXT("recovery [9,12)"), Phases.Segments[2].Kind == EGSTN_PhaseKind::Recovery && Phases.Segments[2].Start == 9 && Phases.Segments[2].End == 12);
		TestTrue(TEXT("actionable [12,30)"), Phases.Segments[3].Kind == EGSTN_PhaseKind::Actionable && Phases.Segments[3].Start == 12 && Phases.Segments[3].End == 30);
	}
	TestEqual(TEXT("the summary line"), Phases.FormatSummary(62.5f).ToString(), FString(TEXT("Startup 6 | Active 3 | Recovery 3 | FAF 12 | Total 30 (480 ms)")));
	TestEqual(TEXT("the tail follows the actionable frame"), Phases.TailStart, 12);
	TestEqual(TEXT("blend-out frames that fit"), Phases.TailBlendOutFrames, 4);

	// Overlapping and touching windows merge, disjoint ones stay apart (the gap is recovery-coloured)
	Data.Tracks.Add(MakeUiTrack<FGSTN_TimelineTrack_Hitbox>(8, 11));
	Data.Tracks.Add(MakeUiTrack<FGSTN_TimelineTrack_Hitbox>(11, 12));
	Data.Tracks.Add(MakeUiTrack<FGSTN_TimelineTrack_Hitbox>(20, 22));
	Data.FirstActionableFrame = 24;
	Phases = FGSTN_PhaseBarData::Compute(Data);
	TestEqual(TEXT("two ranges after merging"), Phases.ActiveRanges.Num(), 2);
	if (Phases.ActiveRanges.Num() == 2)
	{
		TestTrue(TEXT("[6,12) merged"), Phases.ActiveRanges[0] == FIntPoint(6, 12));
		TestTrue(TEXT("[20,22) alone"), Phases.ActiveRanges[1] == FIntPoint(20, 22));
	}
	TestEqual(TEXT("active spans first start to last end"), Phases.ActiveFrames, 16);
	TestEqual(TEXT("recovery from the last active frame"), Phases.RecoveryFrames, 2);

	// No hitbox: everything before the actionable frame is busy
	FGSTN_CombatTimelineData Bare;
	Bare.TotalFrames = 20;
	Bare.FirstActionableFrame = 14;
	Phases = FGSTN_PhaseBarData::Compute(Bare);
	TestFalse(TEXT("no active range"), Phases.bHasActive);
	TestEqual(TEXT("busy then actionable"), Phases.Segments.Num(), 2);
	if (Phases.Segments.Num() == 2)
	{
		TestTrue(TEXT("busy [0,14)"), Phases.Segments[0].Kind == EGSTN_PhaseKind::Busy && Phases.Segments[0].End == 14);
	}
	TestEqual(TEXT("no tail"), Phases.TailStart, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("summary without a hitbox"), Phases.FormatSummary(62.5f).ToString(), FString(TEXT("No hitbox | FAF 14 | Total 20 (320 ms)")));

	// An explicit tail start is used as written
	Bare.Tail.bHasTail = true;
	Bare.Tail.StartFrame = 17;
	TestEqual(TEXT("explicit tail start"), FGSTN_PhaseBarData::Compute(Bare).TailStart, 17);
	return true;
}

// Gestures

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiGestureMoveWindowTest, "GSTNCombat.Editor.Timeline.GestureMoveWindowIsOneUndoStep", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiGestureMoveWindowTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->Trans == nullptr)
	{
		AddInfo(TEXT("no transaction buffer in this editor, skipped"));
		return true;
	}
	FUiFixture F;
	const FString Before = UiExport(F.Data());
	const int32 StepsBefore = UiAppliedSteps();

	// Drag the tags window [0,12) from frame 5 (x 55) to frame 8, then 10
	TestTrue(TEXT("press on the body arms the drag"), F.Press(UiTags, 55.0));
	TestTrue(TEXT("a move gesture runs"), F.Gesture.GetKind() == EGSTN_GestureKind::MoveWindows);
	TestTrue(TEXT("the press selected the window"), F.M().IsSelected(FGSTN_Selection::Window(UiTags)));
	F.Gesture.Update(F.M(), 85.0, 0.0);
	TestEqual(TEXT("moved by 3 frames"), F.Data().Tracks[UiTags].GetPtr<FGSTN_TimelineTrack>()->StartFrame, 3);
	F.Gesture.Update(F.M(), 105.0, 0.0);
	TestEqual(TEXT("then by 5"), F.Data().Tracks[UiTags].GetPtr<FGSTN_TimelineTrack>()->StartFrame, 5);
	TestEqual(TEXT("the window keeps its length"), F.Data().Tracks[UiTags].GetPtr<FGSTN_TimelineTrack>()->EndFrame, 17);
	F.Gesture.End(F.M());
	TestFalse(TEXT("the gesture is over"), F.Gesture.IsActive());
	TestEqual(TEXT("the whole drag is one undo step"), UiAppliedSteps(), StepsBefore + 1);

	GEditor->UndoTransaction();
	TestEqual(TEXT("undo restores the data exactly"), UiExport(F.Data()), Before);

	// A drag far past the start clamps at the start and does not drift: coming back moves it again only once the mouse is back
	TestTrue(TEXT("press again"), F.Press(UiTags, 55.0));
	F.Gesture.Update(F.M(), -500.0, 0.0);
	TestEqual(TEXT("clamped at the start"), F.Data().Tracks[UiTags].GetPtr<FGSTN_TimelineTrack>()->StartFrame, 0);
	F.Gesture.Update(F.M(), 155.0, 0.0);
	TestEqual(TEXT("moved forward 10 from the press"), F.Data().Tracks[UiTags].GetPtr<FGSTN_TimelineTrack>()->StartFrame, 10);
	F.Gesture.Update(F.M(), 5000.0, 0.0);
	TestEqual(TEXT("clamped at the end of the timeline"), F.Data().Tracks[UiTags].GetPtr<FGSTN_TimelineTrack>()->EndFrame, 30);
	F.Gesture.End(F.M());
	GEditor->UndoTransaction();
	TestEqual(TEXT("one undo again"), UiExport(F.Data()), Before);

	// A press and release that moves nothing leaves no step in the history
	const int32 Steps = UiAppliedSteps();
	TestTrue(TEXT("press"), F.Press(UiTags, 55.0));
	F.Gesture.Update(F.M(), 55.0, 0.0);
	F.Gesture.End(F.M());
	TestEqual(TEXT("a click is not an undo step"), UiAppliedSteps(), Steps);
	TestEqual(TEXT("and changes nothing"), UiExport(F.Data()), Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiGestureResizeTest, "GSTNCombat.Editor.Timeline.GestureResizeEdges", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiGestureResizeTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->Trans == nullptr)
	{
		AddInfo(TEXT("no transaction buffer in this editor, skipped"));
		return true;
	}
	FUiFixture F;
	const int32 StepsBefore = UiAppliedSteps();

	// Cancel window [8,14): start edge at x 80, end edge at x 140
	TestTrue(TEXT("press on the start edge"), F.Press(UiCancel, 82.0));
	TestTrue(TEXT("a start resize"), F.Gesture.GetKind() == EGSTN_GestureKind::ResizeStart);
	F.Gesture.Update(F.M(), 54.0, 0.0);
	TestEqual(TEXT("the start follows the nearest frame boundary (5.4 rounds to 5)"), F.Data().Tracks[UiCancel].GetPtr<FGSTN_TimelineTrack>()->StartFrame, 5);
	F.Gesture.Update(F.M(), 400.0, 0.0);
	TestEqual(TEXT("the start cannot pass the end: one frame is left"), F.Data().Tracks[UiCancel].GetPtr<FGSTN_TimelineTrack>()->StartFrame, 13);
	F.Gesture.Update(F.M(), 30.0, 0.0);
	F.Gesture.End(F.M());
	TestEqual(TEXT("start resized to 3"), F.Data().Tracks[UiCancel].GetPtr<FGSTN_TimelineTrack>()->StartFrame, 3);
	TestEqual(TEXT("the end did not move"), F.Data().Tracks[UiCancel].GetPtr<FGSTN_TimelineTrack>()->EndFrame, 14);
	TestEqual(TEXT("one undo step"), UiAppliedSteps(), StepsBefore + 1);

	// The end edge is now at x 140 still
	TestTrue(TEXT("press on the end edge"), F.Press(UiCancel, 138.0));
	TestTrue(TEXT("an end resize"), F.Gesture.GetKind() == EGSTN_GestureKind::ResizeEnd);
	F.Gesture.Update(F.M(), 203.0, 0.0);
	F.Gesture.End(F.M());
	TestEqual(TEXT("end resized to 20"), F.Data().Tracks[UiCancel].GetPtr<FGSTN_TimelineTrack>()->EndFrame, 20);
	TestEqual(TEXT("two undo steps in all"), UiAppliedSteps(), StepsBefore + 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiGestureEscapeTest, "GSTNCombat.Editor.Timeline.GestureEscapeRestores", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiGestureEscapeTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->Trans == nullptr)
	{
		AddInfo(TEXT("no transaction buffer in this editor, skipped"));
		return true;
	}
	FUiFixture F;
	const FString Before = UiExport(F.Data());
	const int32 StepsBefore = UiAppliedSteps();

	TestTrue(TEXT("press on a key"), F.Press(UiKeyed, UiMid(10)));
	F.Gesture.Update(F.M(), UiMid(15), 0.0);
	TestEqual(TEXT("the key moved while dragging"), F.Keyed().Keys[1].Frame, 15);
	F.Gesture.Cancel(F.M());
	TestFalse(TEXT("the gesture is over"), F.Gesture.IsActive());
	TestEqual(TEXT("Escape restores the data"), UiExport(F.Data()), Before);
	TestEqual(TEXT("and leaves no undo step"), UiAppliedSteps(), StepsBefore);

	// Window drags and the markers too
	TestTrue(TEXT("press on a window"), F.Press(UiTags, 55.0));
	F.Gesture.Update(F.M(), 155.0, 0.0);
	F.Gesture.Cancel(F.M());
	TestEqual(TEXT("window drag restored"), UiExport(F.Data()), Before);

	TestTrue(TEXT("grab the actionable frame marker"), F.Gesture.BeginFirstActionable(F.M()));
	F.Gesture.Update(F.M(), 200.0, 0.0);
	TestEqual(TEXT("marker moved"), F.Data().FirstActionableFrame, 20);
	F.Gesture.Cancel(F.M());
	TestEqual(TEXT("marker restored"), UiExport(F.Data()), Before);
	TestEqual(TEXT("no steps at all"), UiAppliedSteps(), StepsBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiGestureGroupTest, "GSTNCombat.Editor.Timeline.GestureGroupMoveAndSelection", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiGestureGroupTest::RunTest(const FString& Parameters)
{
	FUiFixture F;

	// Two selected windows move together and keep their distance, whichever reaches the end first: tags [0,12) and cancel [8,14)
	F.M().SetSelection({ FGSTN_Selection::Window(UiTags), FGSTN_Selection::Window(UiCancel) });
	TestTrue(TEXT("press on a member of the group"), F.Press(UiTags, 55.0));
	TestEqual(TEXT("both windows move"), F.Gesture.GetMovedTracks().Num(), 2);
	F.Gesture.Update(F.M(), 355.0, 0.0);
	const FGSTN_TimelineTrack* Tags = F.Data().Tracks[UiTags].GetPtr<FGSTN_TimelineTrack>();
	const FGSTN_TimelineTrack* Cancel = F.Data().Tracks[UiCancel].GetPtr<FGSTN_TimelineTrack>();
	TestEqual(TEXT("the group stops when the cancel window reaches the end"), Cancel->EndFrame, 30);
	TestEqual(TEXT("the tags window moved the same distance"), Tags->StartFrame, 16);
	TestEqual(TEXT("distance between them kept"), Cancel->StartFrame - Tags->StartFrame, 8);
	F.Gesture.End(F.M());
	TestEqual(TEXT("the group is still selected after a real drag"), F.M().GetSelection().Num(), 2);

	// A press and release on a member that does not move narrows the selection to it
	FUiFixture G;
	G.M().SetSelection({ FGSTN_Selection::Window(UiTags), FGSTN_Selection::Window(UiCancel) });
	TestTrue(TEXT("press"), G.Press(UiTags, 55.0));
	G.Gesture.End(G.M());
	TestEqual(TEXT("a click on a member selects only it"), G.M().GetSelection().Num(), 1);
	TestTrue(TEXT("and it is that member"), G.M().IsSelected(FGSTN_Selection::Window(UiTags, EGSTN_SelectionPart::Body)));

	// Shift adds to the selection, and takes an item out again with no drag
	FUiFixture H;
	H.M().SetSelection({ FGSTN_Selection::Window(UiTags) });
	TestTrue(TEXT("shift+press on another window adds it and arms a drag"), H.Press(UiMovement, 70.0, true));
	H.Gesture.End(H.M());
	TestEqual(TEXT("two selected"), H.M().GetSelection().Num(), 2);
	TestFalse(TEXT("shift+press on a selected window removes it and arms nothing"), H.Press(UiMovement, 70.0, true));
	TestEqual(TEXT("one left"), H.M().GetSelection().Num(), 1);
	TestFalse(TEXT("no gesture runs"), H.Gesture.IsActive());

	// Pressing an unselected item replaces the selection
	H.Press(UiMovement, 70.0);
	H.Gesture.End(H.M());
	TestEqual(TEXT("press without shift replaces"), H.M().GetSelection().Num(), 1);
	TestTrue(TEXT("with the pressed window"), H.M().IsSelected(FGSTN_Selection::Window(UiMovement)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiGestureKeysTest, "GSTNCombat.Editor.Timeline.GestureKeysMoveAndDuplicate", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiGestureKeysTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->Trans == nullptr)
	{
		AddInfo(TEXT("no transaction buffer in this editor, skipped"));
		return true;
	}
	FUiFixture F;
	const FString Before = UiExport(F.Data());
	const int32 StepsBefore = UiAppliedSteps();

	// Move: the key at 10 to 13, one step
	TestTrue(TEXT("press on the key at frame 10"), F.Press(UiKeyed, UiMid(10)));
	TestTrue(TEXT("a key move"), F.Gesture.GetKind() == EGSTN_GestureKind::MoveKeys);
	F.Gesture.Update(F.M(), UiMid(13), 0.0);
	TestEqual(TEXT("the key moved by three"), F.Keyed().Keys[1].Frame, 13);
	F.Gesture.Update(F.M(), UiMid(25), 0.0);
	TestEqual(TEXT("past the neighbour it re-sorts and the gesture follows the key"), F.Keyed().Keys.Last().Frame, 25);
	F.Gesture.Update(F.M(), UiMid(11), 0.0);
	F.Gesture.End(F.M());
	TestEqual(TEXT("the key ends at 11"), F.Keyed().Keys[1].Frame, 11);
	TestEqual(TEXT("still three keys"), F.Keyed().Keys.Num(), 3);
	TestEqual(TEXT("one undo step"), UiAppliedSteps(), StepsBefore + 1);
	GEditor->UndoTransaction();
	TestEqual(TEXT("undo restores"), UiExport(F.Data()), Before);

	// Ctrl+drag: nothing is copied until the mouse leaves the frame, then the copy is what moves and the original stays
	TestTrue(TEXT("ctrl+press on the key at frame 10"), F.Press(UiKeyed, UiMid(10), false, true));
	TestTrue(TEXT("a duplicate gesture"), F.Gesture.GetKind() == EGSTN_GestureKind::DuplicateKeys);
	F.Gesture.Update(F.M(), UiMid(10), 0.0);
	TestEqual(TEXT("no copy while the mouse has not moved"), F.Keyed().Keys.Num(), 3);
	F.Gesture.Update(F.M(), UiMid(14), 0.0);
	TestEqual(TEXT("the copy exists"), F.Keyed().Keys.Num(), 4);
	F.Gesture.Update(F.M(), UiMid(16), 0.0);
	F.Gesture.End(F.M());
	TestEqual(TEXT("still four keys"), F.Keyed().Keys.Num(), 4);
	TestEqual(TEXT("the original stayed at 10"), F.Keyed().Keys[1].Frame, 10);
	TestEqual(TEXT("the copy ended at 16"), F.Keyed().Keys[2].Frame, 16);
	TestEqual(TEXT("one undo step"), UiAppliedSteps(), StepsBefore + 1);
	GEditor->UndoTransaction();
	TestEqual(TEXT("undo removes the copy"), UiExport(F.Data()), Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiGestureBoxSelectTest, "GSTNCombat.Editor.Timeline.GestureBoxSelect", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiGestureBoxSelectTest::RunTest(const FString& Parameters)
{
	FUiFixture F;
	F.M().SetSelection({ FGSTN_Selection::Window(UiHitbox) });

	// Empty space right of everything: a box starts and replaces the selection
	TestTrue(TEXT("press on empty lane space starts a box"), F.Press(UiTags, 250.0));
	TestTrue(TEXT("a box select"), F.Gesture.GetKind() == EGSTN_GestureKind::BoxSelect);
	TestTrue(TEXT("the old selection is cleared"), F.M().GetSelection().IsEmpty());
	F.Gesture.Update(F.M(), 5.0, FGSTN_TimelineLayout::RowHeight * 7 - 1.0);
	FVector2D Min;
	FVector2D Max;
	TestTrue(TEXT("the rectangle is available for painting"), F.Gesture.GetBox(Min, Max));
	TestEqual(TEXT("sorted corners"), Min.X, 5.0);
	TestEqual(TEXT("right edge is the start"), Max.X, 250.0);
	TestTrue(TEXT("the box selected windows from several rows"), F.M().GetSelection().Num() >= 5);
	F.Gesture.Update(F.M(), 100.0, 10.0);
	TestEqual(TEXT("shrinking the box shrinks the selection (row 0 only)"), F.M().GetSelection().Num(), 1);
	F.Gesture.End(F.M());
	TestFalse(TEXT("no box after mouse-up"), F.Gesture.GetBox(Min, Max));
	TestEqual(TEXT("the selection stays"), F.M().GetSelection().Num(), 1);

	// Escape puts the previous selection back
	F.M().SetSelection({ FGSTN_Selection::Window(UiHitbox) });
	F.Press(UiTags, 250.0);
	F.Gesture.Update(F.M(), 5.0, 100.0);
	F.Gesture.Cancel(F.M());
	TestEqual(TEXT("cancelled box restores the selection"), F.M().GetSelection().Num(), 1);
	TestTrue(TEXT("the hitbox again"), F.M().IsSelected(FGSTN_Selection::Window(UiHitbox)));

	// Shift keeps the old selection and adds the box
	F.Press(UiTags, 250.0, true);
	F.Gesture.Update(F.M(), 100.0, 10.0);
	F.Gesture.End(F.M());
	TestTrue(TEXT("additive keeps the hitbox"), F.M().IsSelected(FGSTN_Selection::Window(UiHitbox)));
	TestTrue(TEXT("and adds the tags window"), F.M().IsSelected(FGSTN_Selection::Window(UiTags)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiGestureMarkersTest, "GSTNCombat.Editor.Timeline.GestureFirstActionableAndTail", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiGestureMarkersTest::RunTest(const FString& Parameters)
{
	if (GEditor == nullptr || GEditor->Trans == nullptr)
	{
		AddInfo(TEXT("no transaction buffer in this editor, skipped"));
		return true;
	}
	FUiFixture F;
	const int32 StepsBefore = UiAppliedSteps();

	TestTrue(TEXT("grab the actionable frame marker"), F.Gesture.BeginFirstActionable(F.M()));
	F.Gesture.Update(F.M(), 171.0, 0.0);
	TestEqual(TEXT("writes FirstActionableFrame through the model (17.1 rounds to 17)"), F.Data().FirstActionableFrame, 17);
	F.Gesture.Update(F.M(), 5000.0, 0.0);
	TestEqual(TEXT("clamped at the end of the timeline"), F.Data().FirstActionableFrame, 30);
	F.Gesture.Update(F.M(), 150.0, 0.0);
	F.Gesture.End(F.M());
	TestEqual(TEXT("one undo step"), UiAppliedSteps(), StepsBefore + 1);
	TestEqual(TEXT("the tail still follows the actionable frame"), F.Data().GetTailStartFrame(), 15);

	// The tail handle: a tail that follows the actionable frame (StartFrame -1) stays that way until the handle really moves
	F.Data().FirstActionableFrame = 12;
	TestTrue(TEXT("grab the tail handle"), F.Gesture.BeginTailStart(F.M()));
	F.Gesture.Update(F.M(), 121.0, 0.0);
	TestEqual(TEXT("not moved off its frame: still following"), F.Data().Tail.StartFrame, static_cast<int32>(INDEX_NONE));
	F.Gesture.Update(F.M(), 203.0, 0.0);
	TestEqual(TEXT("moved: the start is explicit"), F.Data().Tail.StartFrame, 20);
	F.Gesture.End(F.M());
	TestEqual(TEXT("two undo steps in all"), UiAppliedSteps(), StepsBefore + 2);

	// No tail, nothing to grab
	F.Data().Tail.bHasTail = false;
	TestFalse(TEXT("no tail handle on a timeline without a tail"), F.Gesture.BeginTailStart(F.M()));
	return true;
}

// Actions

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiActionsTest, "GSTNCombat.Editor.Timeline.Actions", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiActionsTest::RunTest(const FString& Parameters)
{
	// Double-click: a key on a keyed track (not twice on one frame), a new track of the same kind elsewhere
	{
		FUiFixture F;
		TestTrue(TEXT("a key at frame 14"), FGSTN_TimelineActions::AddAtFrame(F.M(), UiKeyed, 14));
		TestEqual(TEXT("four keys"), F.Keyed().Keys.Num(), 4);
		TestFalse(TEXT("not a second key on a frame that has one"), FGSTN_TimelineActions::AddAtFrame(F.M(), UiKeyed, 10));
		TestEqual(TEXT("still four"), F.Keyed().Keys.Num(), 4);

		const int32 Tracks = F.M().GetTrackCount();
		TestTrue(TEXT("a movement window at frame 20"), FGSTN_TimelineActions::AddAtFrame(F.M(), UiMovement, 20));
		TestEqual(TEXT("one more track"), F.M().GetTrackCount(), Tracks + 1);
		const FGSTN_TimelineTrack* Added = F.M().GetTrack(Tracks);
		TestTrue(TEXT("of the same kind"), Added != nullptr && Added->GetKind() == EGSTN_TimelineTrackKind::Movement);
		TestEqual(TEXT("starting there"), Added ? Added->StartFrame : -1, 20);
		TestEqual(TEXT("with the default length"), Added ? Added->EndFrame - Added->StartFrame : -1, FGSTN_TimelineActions::DefaultWindowFrames);
		TestTrue(TEXT("an event is one frame"), FGSTN_TimelineActions::AddAtFrame(F.M(), UiEvent, 22));
		const FGSTN_TimelineTrack* Event = F.M().GetTrack(Tracks + 1);
		TestEqual(TEXT("event length"), Event ? Event->EndFrame - Event->StartFrame : -1, 1);
		TestFalse(TEXT("a track that does not exist"), FGSTN_TimelineActions::AddAtFrame(F.M(), 99, 3));
	}

	// Delete: keys first, else tracks
	{
		FUiFixture F;
		F.M().SetSelection({ FGSTN_Selection::Key(UiKeyed, 0), FGSTN_Selection::Key(UiKeyed, 2), FGSTN_Selection::Window(UiTags) });
		TestEqual(TEXT("two keys deleted"), FGSTN_TimelineActions::DeleteSelection(F.M()), 2);
		TestEqual(TEXT("one key left"), F.Keyed().Keys.Num(), 1);
		TestEqual(TEXT("the window was not touched when keys were selected"), F.M().GetTrackCount(), 7);
		F.M().SetSelection({ FGSTN_Selection::Window(UiTags), FGSTN_Selection::Window(UiCancel) });
		TestEqual(TEXT("two tracks deleted"), FGSTN_TimelineActions::DeleteSelection(F.M()), 2);
		TestEqual(TEXT("five tracks left"), F.M().GetTrackCount(), 5);
		TestEqual(TEXT("nothing selected, nothing deleted"), FGSTN_TimelineActions::DeleteSelection(F.M()), 0);
	}

	// K, Ctrl+D
	{
		FUiFixture F;
		F.M().SetSelection({ FGSTN_Selection::Window(UiTags) });
		TestFalse(TEXT("K needs a track with keys"), FGSTN_TimelineActions::AddKeyAtPlayhead(F.M()));
		TestEqual(TEXT("so does paste"), FGSTN_TimelineActions::PasteAtPlayhead(F.M()), 0);
		F.M().SetSelection({ FGSTN_Selection::Window(UiKeyed) });
		F.M().SetPlayhead(14);
		TestTrue(TEXT("K on the selected keyed track"), FGSTN_TimelineActions::AddKeyAtPlayhead(F.M()));
		TestEqual(TEXT("a key at the playhead"), F.Keyed().Keys.Num(), 4);
		TestFalse(TEXT("not twice on the same frame"), FGSTN_TimelineActions::AddKeyAtPlayhead(F.M()));

		F.M().SetSelection({ FGSTN_Selection::Key(UiKeyed, 0) });
		F.M().SetPlayhead(7);
		TestEqual(TEXT("one key duplicated"), FGSTN_TimelineActions::DuplicateSelectionAtPlayhead(F.M()), 1);
		TestEqual(TEXT("five keys"), F.Keyed().Keys.Num(), 5);
		TestEqual(TEXT("the copy sits on the playhead"), F.Keyed().Keys[1].Frame, 7);
		F.M().SetSelection({ FGSTN_Selection::Key(UiKeyed, 1) });
		TestEqual(TEXT("duplicating onto its own frame moves the copy one frame on"), FGSTN_TimelineActions::DuplicateSelectionAtPlayhead(F.M()), 1);
		TestEqual(TEXT("the copy is at 8"), F.Keyed().Keys[2].Frame, 8);
	}
	return true;
}

// Commands and widgets

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiCommandsTest, "GSTNCombat.Editor.Timeline.CommandsRegistered", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiCommandsTest::RunTest(const FString& Parameters)
{
	if (!TestTrue(TEXT("the module registered the command set"), FGSTN_TimelineEditorCommands::IsRegistered()))
	{
		return false;
	}
	const FGSTN_TimelineEditorCommands& Commands = FGSTN_TimelineEditorCommands::Get();
	const TArray<TSharedPtr<FUICommandInfo>> All = {
		Commands.StepBack, Commands.StepForward, Commands.PreviousKey, Commands.NextKey, Commands.PlayPause,
		Commands.AddKeyAtPlayhead, Commands.ToggleAutoKey, Commands.DeleteSelection, Commands.CopyKeys, Commands.PasteKeys, Commands.DuplicateKeys,
		Commands.FrameAll, Commands.FrameSelection, Commands.FitTotalFramesToMontage,
		Commands.BakeSockets, Commands.ValidateNow, Commands.ToggleShowHitboxes, Commands.ToggleShowHurtboxes, Commands.ToggleShowMovementPath };
	for (const TSharedPtr<FUICommandInfo>& Command : All)
	{
		TestTrue(TEXT("every command exists"), Command.IsValid());
	}
	TestTrue(TEXT("step back is ,"), Commands.StepBack->GetFirstValidChord()->Key == EKeys::Comma);
	TestTrue(TEXT("step forward is ."), Commands.StepForward->GetFirstValidChord()->Key == EKeys::Period);
	TestTrue(TEXT("previous key is shift+,"), Commands.PreviousKey->GetFirstValidChord()->Key == EKeys::Comma && Commands.PreviousKey->GetFirstValidChord()->NeedsShift());
	TestTrue(TEXT("play is space"), Commands.PlayPause->GetFirstValidChord()->Key == EKeys::SpaceBar);
	TestTrue(TEXT("frame all is Home"), Commands.FrameAll->GetFirstValidChord()->Key == EKeys::Home);
	TestTrue(TEXT("bake is ctrl+B"), Commands.BakeSockets->GetFirstValidChord()->Key == EKeys::B && Commands.BakeSockets->GetFirstValidChord()->NeedsControl());
	TestTrue(TEXT("copy is ctrl+C"), Commands.CopyKeys->GetFirstValidChord()->Key == EKeys::C && Commands.CopyKeys->GetFirstValidChord()->NeedsControl());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiPanelTestCommandTest, "GSTNCombat.Editor.Timeline.PanelTestCommandRegistered", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiPanelTestCommandTest::RunTest(const FString& Parameters)
{
	IConsoleObject* Object = IConsoleManager::Get().FindConsoleObject(GSTNTimelinePanelTest::OpenCommandName);
	TestNotNull(TEXT("the console command that opens the test tab exists"), Object);
	TestNotNull(TEXT("and is a command"), Object ? Object->AsCommand() : nullptr);

	// The demo content is something the phase bar and the lanes can show
	FGSTN_CombatTimelineData Demo;
	GSTNTimelinePanelTest::FillDemoTimeline(Demo, false);
	TestTrue(TEXT("the demo has tracks of several kinds"), Demo.Tracks.Num() >= 8);
	TestTrue(TEXT("and an active range for the phase bar"), FGSTN_PhaseBarData::Compute(Demo).bHasActive);
	FGSTN_CombatTimelineData Stress;
	GSTNTimelinePanelTest::FillDemoTimeline(Stress, true);
	TestEqual(TEXT("the stress timeline has 12 tracks"), Stress.Tracks.Num(), 12);
	TestEqual(TEXT("of 60 keys"), Stress.Tracks[0].GetPtr<FGSTN_TestKeyedTrack>() ? Stress.Tracks[0].Get<FGSTN_TestKeyedTrack>().Keys.Num() : 0, 60);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_UiPanelConstructsTest, "GSTNCombat.Editor.Timeline.PanelConstructsAndRunsCommands", GSTN_UI_TEST_FLAGS)
bool FGSTN_UiPanelConstructsTest::RunTest(const FString& Parameters)
{
	if (!FSlateApplication::IsInitialized())
	{
		AddInfo(TEXT("no Slate application in this run, widget construction skipped"));
		return true;
	}

	FUiFixture F;
	const TSharedRef<SGSTN_TimelinePanel> Panel = SNew(SGSTN_TimelinePanel).Model(F.Model);
	const FGSTN_TimelineEditorCommands& Commands = FGSTN_TimelineEditorCommands::Get();
	const TSharedRef<FUICommandList>& List = Panel->GetCommandList();

	// The commands the panel implements are mapped to the model, and unmapped ones (bake, validate) are not
	TestTrue(TEXT("step forward is mapped"), List->CanExecuteAction(Commands.StepForward.ToSharedRef()));
	List->ExecuteAction(Commands.StepForward.ToSharedRef());
	TestEqual(TEXT("step forward moved the playhead"), F.M().GetPlayhead(), 1);
	List->ExecuteAction(Commands.StepBack.ToSharedRef());
	TestEqual(TEXT("step back moved it back"), F.M().GetPlayhead(), 0);
	TestNull(TEXT("bake is not mapped by the panel (the toolkit of E8 maps it)"), List->GetActionForCommand(Commands.BakeSockets));
	TestNull(TEXT("nor validate (E10)"), List->GetActionForCommand(Commands.ValidateNow));
	TestNotNull(TEXT("step forward is"), List->GetActionForCommand(Commands.StepForward));
	TestFalse(TEXT("delete needs a selection"), List->CanExecuteAction(Commands.DeleteSelection.ToSharedRef()));
	F.M().SetSelection({ FGSTN_Selection::Window(UiTags) });
	TestTrue(TEXT("and has one"), List->CanExecuteAction(Commands.DeleteSelection.ToSharedRef()));
	List->ExecuteAction(Commands.DeleteSelection.ToSharedRef());
	TestEqual(TEXT("delete removed the track"), F.M().GetTrackCount(), 6);

	const bool bAutoKey = F.M().IsAutoKeyEnabled();
	List->ExecuteAction(Commands.ToggleAutoKey.ToSharedRef());
	TestEqual(TEXT("auto-key toggled"), F.M().IsAutoKeyEnabled(), !bAutoKey);

	// Play starts and stops its ticker without leaving one behind
	Panel->SetPlaying(true);
	TestTrue(TEXT("playing"), Panel->IsPlaying());
	Panel->SetPlaying(false);
	TestFalse(TEXT("paused"), Panel->IsPlaying());

	// The frame data line reads the data
	TestEqual(TEXT("the summary text"), SGSTN_FrameDataSummary::MakeSummaryText(F.M()).ToString().Left(20), FString(TEXT("Startup 6 | Active 3")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
