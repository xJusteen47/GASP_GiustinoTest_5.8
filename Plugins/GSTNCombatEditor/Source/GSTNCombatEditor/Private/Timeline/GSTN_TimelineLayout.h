// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineEditorModel.h"

/** What a point on a lane touches */
enum class EGSTN_LaneHitKind : uint8
{
	None,
	/** The inside of a window bar (or the diamond of a one-frame track) */
	Body,
	/** The first-frame edge of a window bar */
	StartEdge,
	/** The last-frame edge of a window bar */
	EndEdge,
	/** The diamond of one key */
	Key
};

/** The result of hit-testing one lane */
struct FGSTN_LaneHit
{
	EGSTN_LaneHitKind Kind = EGSTN_LaneHitKind::None;
	int32 TrackIndex = INDEX_NONE;
	int32 KeyIndex = INDEX_NONE;

	bool IsHit() const { return Kind != EGSTN_LaneHitKind::None; }

	/** The selection entry the hit stands for. A miss gives an invalid entry (TrackIndex INDEX_NONE) */
	FGSTN_Selection ToSelection() const;
};

/** Which frames the ruler labels and ticks for the current zoom */
struct FGSTN_RulerTicks
{
	/** First and last integer frame whose tick may be visible (a frame or two of margin) */
	int32 FirstFrame = 0;
	int32 LastFrame = 0;

	/** A label every LabelStep frames: 1, 2, 5, 10, 20, 50... chosen so labels are never closer than the minimum spacing */
	int32 LabelStep = 1;

	/** A thin tick every MinorStep frames (a divisor of LabelStep), never closer than a few pixels */
	int32 MinorStep = 1;
};

/**
 * The geometry of the timeline panel without a widget: hit-testing, box select, ruler tick spacing, playhead stepping and the
 * wheel. The Slate widgets only turn mouse events into calls of these functions, so the maths runs in the automation tests
 * without a window (plan m5 E4). All pixels are in lane space: x from the left edge of the lanes, the frame mapping of the
 * model's view transform; y in "content" space where row i occupies [i * RowHeight, (i + 1) * RowHeight).
 */
class FGSTN_TimelineLayout
{
public:
	static constexpr double RowHeight = 24.0;

	/** How close to a window edge counts as the edge (the grab zone shrinks to a third of a narrow bar so a body stays) */
	static constexpr double EdgeGrabPixels = 5.0;

	/** Half the size of a key diamond, also the grab radius */
	static constexpr double KeyHalfPixels = 6.0;

	/** Labels are never closer than this */
	static constexpr double MinLabelSpacingPixels = 44.0;

	// Hit testing

	/**
	 * What lane x touches on the track. Keys come before edges, edges before the body. A track that does not exist is a miss. LocalY is the
	 * y inside the row [0, RowHeight), for a track that keeps its keys in several lanes (hitboxes): of the keys within reach of x, the one nearest
	 * in y wins. Negative = unknown, nearest in x
	 */
	static FGSTN_LaneHit HitTestTrack(const FGSTN_CombatTimelineData& Data, int32 TrackIndex, const FGSTN_TimelineViewTransform& View, double X, double LocalY = -1.0);

	/** The part of a window [Start, End) at x: edges within the grab zone, the body between. Point windows (one frame) are all body */
	static EGSTN_LaneHitKind HitTestWindow(int32 StartFrame, int32 EndFrame, bool bPoint, const FGSTN_TimelineViewTransform& View, double X);

	/**
	 * What a rectangle selects: on a keyed track the keys whose centre is inside, else the window if the rectangle overlaps it;
	 * rows are the ones the rectangle's y range touches. The corners may come in any order.
	 */
	static TArray<FGSTN_Selection> BoxSelect(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineViewTransform& View, double X0, double Y0, double X1, double Y1);

	// Keys of a track (through the registered adapter)

	/** Number of keys, 0 for a track without keys or an index that does not exist */
	static int32 GetKeyCount(const FGSTN_CombatTimelineData& Data, int32 TrackIndex);

	/** Absolute frame of a key, INDEX_NONE when the key does not exist */
	static int32 GetKeyFrame(const FGSTN_CombatTimelineData& Data, int32 TrackIndex, int32 KeyIndex);

	/** Lanes the track keeps its keys in (1 for most tracks, the number of hitboxes for the hitbox track) */
	static int32 GetLaneCount(const FGSTN_CombatTimelineData& Data, int32 TrackIndex);

	/** The y inside the row of a lane: the middle for a one-lane track */
	static double GetLaneY(const FGSTN_CombatTimelineData& Data, int32 TrackIndex, int32 Lane);

	/** Where a key sits inside its row, y in [0, RowHeight): the middle for a one-lane track, else one sub-lane per lane, spread around the middle */
	static double GetKeyY(const FGSTN_CombatTimelineData& Data, int32 TrackIndex, int32 KeyIndex);

	/** Half the size of the key's diamond as drawn: a little smaller when the row has several lanes */
	static double GetKeyHalfSize(const FGSTN_CombatTimelineData& Data, int32 TrackIndex);

	/** Does the track have keys (an adapter is registered for its struct)? */
	static bool IsKeyedTrack(const FGSTN_CombatTimelineData& Data, int32 TrackIndex);

	/** The earliest frame among the selected keys, INDEX_NONE when none of the selection is a key */
	static int32 GetEarliestSelectedKeyFrame(const FGSTN_CombatTimelineData& Data, TConstArrayView<FGSTN_Selection> Selection);

	// Ruler

	/** Tick and label steps for the visible range of the width */
	static FGSTN_RulerTicks ComputeRulerTicks(const FGSTN_TimelineViewTransform& View, double WidthPixels);

	/** The time of the start of a frame in milliseconds with one decimal ("62.5"), from the frame times the tick rate */
	static FString FormatMilliseconds(int32 Frame, float TickRate);

	// Navigation

	/**
	 * The next (Direction > 0) or previous (Direction < 0) frame that has a key or a window edge (first and last frame of a
	 * window, the frame of a one-frame track) strictly after or before Frame. INDEX_NONE when there is none.
	 */
	static int32 FindAdjacentEdge(const FGSTN_CombatTimelineData& Data, int32 Frame, int32 Direction);

	/** The frames [OutFirst, OutEnd) covered by the selected windows and keys. False when the selection covers nothing */
	static bool GetSelectionSpan(const FGSTN_CombatTimelineData& Data, TConstArrayView<FGSTN_Selection> Selection, int32& OutFirst, int32& OutEnd);

	/** Wheel: scroll, or zoom around the cursor with Ctrl. WheelDelta is in notches (positive = away from the user). The view stays near the timeline */
	static void ApplyWheel(FGSTN_TimelineViewTransform& View, double CursorX, double WheelDelta, bool bZoom, double WidthPixels, int32 TotalFrames);

	/** Keeps the view from scrolling away from the timeline altogether */
	static void ClampView(FGSTN_TimelineViewTransform& View, double WidthPixels, int32 TotalFrames);

	/** Shows the whole timeline with a small margin ("frame all") */
	static void FrameAll(FGSTN_TimelineViewTransform& View, double WidthPixels, int32 TotalFrames);

	/** Shows [First, End) with a small margin ("frame selection") */
	static void FrameRange(FGSTN_TimelineViewTransform& View, double WidthPixels, int32 First, int32 End);
};

/**
 * Plays frames at the real tick rate from the real time that passed, so a slow editor frame skips frames instead of slowing the
 * move down. Holds the fraction between calls. The panel feeds it from an FTSTicker.
 */
struct FGSTN_PlaybackClock
{
	/** Frames not yet handed out, in [0, 1) between calls */
	double Accumulator = 0.0;

	/** The longest real time one call may account for: after a stall (a breakpoint, a long import) playback resumes instead of jumping */
	static constexpr double MaxDeltaSeconds = 0.25;

	void Reset() { Accumulator = 0.0; }

	/** Whole frames elapsed in DeltaSeconds at TickRate frames per second */
	int32 Advance(double DeltaSeconds, float TickRate);

	/** Frame advanced by Steps inside [First, End), wrapping around (loop). An empty range gives First */
	static int32 StepLooped(int32 Frame, int32 Steps, int32 First, int32 End);
};
