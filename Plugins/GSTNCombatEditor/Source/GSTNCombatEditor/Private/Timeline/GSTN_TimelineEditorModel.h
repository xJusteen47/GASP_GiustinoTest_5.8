// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "Timeline/GSTN_TimelineEditContext.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "Timeline/GSTN_TimelineUndo.h"

class UAnimMontage;
class UScriptStruct;

// This header and its source include no UI, widget or viewport type (checked by grep, plan m5 E2). Widgets, gizmos and the toolkit call the model,
// and only the model writes the data (combat-m5-editor 4.2): one place for clamping and transactions.

/** What part of a track a selection addresses */
enum class EGSTN_SelectionPart : uint8
{
	/** The whole window (or the row) */
	Body,
	/** The first-frame edge of the window */
	Start,
	/** The last-frame edge of the window */
	End,
	/** One key of a keyed track */
	Key
};

/** Which edge of a window a resize moves */
enum class EGSTN_WindowEdge : uint8
{
	Start,
	End
};

/** One selected thing. KeyIndex is INDEX_NONE for the window itself. Indices are positions in the data, revalidated after undo. */
struct FGSTN_Selection
{
	int32 TrackIndex = INDEX_NONE;
	int32 KeyIndex = INDEX_NONE;
	EGSTN_SelectionPart Part = EGSTN_SelectionPart::Body;

	FGSTN_Selection() = default;
	FGSTN_Selection(int32 InTrackIndex, int32 InKeyIndex, EGSTN_SelectionPart InPart)
		: TrackIndex(InTrackIndex), KeyIndex(InKeyIndex), Part(InPart)
	{
	}

	static FGSTN_Selection Window(int32 InTrackIndex, EGSTN_SelectionPart InPart = EGSTN_SelectionPart::Body) { return FGSTN_Selection(InTrackIndex, INDEX_NONE, InPart); }
	static FGSTN_Selection Key(int32 InTrackIndex, int32 InKeyIndex) { return FGSTN_Selection(InTrackIndex, InKeyIndex, EGSTN_SelectionPart::Key); }

	bool IsKey() const { return KeyIndex != INDEX_NONE; }

	bool operator==(const FGSTN_Selection& Other) const
	{
		return TrackIndex == Other.TrackIndex && KeyIndex == Other.KeyIndex && Part == Other.Part;
	}
};

/**
 * Frame to pixel mapping shared by the ruler, the lanes and the phase bar, so they cannot disagree (combat-m5-editor 5.2). Plain
 * numbers: the widgets pass their own width. A frame occupies [f, f + 1) on the ruler.
 */
struct FGSTN_TimelineViewTransform
{
	static constexpr double MinPixelsPerFrame = 1.0;
	static constexpr double MaxPixelsPerFrame = 256.0;

	/** Frame (fractional) at the left edge of the visible area */
	double FirstVisibleFrame = 0.0;

	double PixelsPerFrame = 12.0;

	double FrameToPixel(double Frame) const { return (Frame - FirstVisibleFrame) * PixelsPerFrame; }
	double PixelToFrame(double Pixel) const { return FirstVisibleFrame + Pixel / PixelsPerFrame; }

	/** The integer frame under a pixel: frames are integers, so a click lands on the frame whose span contains it */
	int32 PixelToFrameIndex(double Pixel) const { return FMath::FloorToInt32(PixelToFrame(Pixel)); }

	double GetVisibleFrameCount(double WidthPixels) const { return WidthPixels / PixelsPerFrame; }

	/** Scrolls by a pixel amount (mouse wheel, panning) */
	void ScrollByPixels(double DeltaPixels) { FirstVisibleFrame += DeltaPixels / PixelsPerFrame; }

	/** Zooms by a factor around a pixel: the frame under the cursor stays under the cursor */
	void ZoomAround(double Pixel, double Factor)
	{
		const double FrameUnderCursor = PixelToFrame(Pixel);
		PixelsPerFrame = FMath::Clamp(PixelsPerFrame * Factor, MinPixelsPerFrame, MaxPixelsPerFrame);
		FirstVisibleFrame = FrameUnderCursor - Pixel / PixelsPerFrame;
	}

	/** Shows [FirstFrame, EndFrame) across the width ("frame all", "frame selection") */
	void FitRange(double FirstFrame, double EndFrame, double WidthPixels)
	{
		const double Span = FMath::Max(EndFrame - FirstFrame, 1.0);
		PixelsPerFrame = FMath::Clamp(WidthPixels / Span, MinPixelsPerFrame, MaxPixelsPerFrame);
		FirstVisibleFrame = FirstFrame;
	}
};

DECLARE_MULTICAST_DELEGATE(FGSTN_OnTimelineDataChanged);
DECLARE_MULTICAST_DELEGATE(FGSTN_OnTimelineSelectionChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FGSTN_OnTimelinePlayheadChanged, int32 /*Frame*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FGSTN_OnTimelineKeySelected, const FGSTN_Selection& /*Key*/);
DECLARE_MULTICAST_DELEGATE(FGSTN_OnTimelineViewChanged);

/**
 * The state and the mutations of one open timeline editor (combat-m5-editor 4.2). A plain C++ object: it owns no UObject, only
 * the edit context that points at the data, so the widgets of the timeline, the viewport gizmos and the details panel are all
 * thin views over it.
 *
 * Rules every mutation follows:
 *  - It does nothing, and opens no transaction, when it would change nothing (a drag that hits the clamp creates no empty undo step).
 *  - It opens one scoped transaction with a localized description, records the owner, changes the data, notifies the owner and
 *    broadcasts OnDataChanged. Inside a drag (BeginDrag) that transaction joins the drag's, so a whole drag is one undo step.
 *  - The gestures (add, move, resize, set) clamp to [0, TotalFrames] and keep integer frames; a timeline with TotalFrames 0 has no
 *    length to clamp to and is clamped at the start only. ApplyTrackEdit and ApplyDataEdit write what the caller says,
 *    unclamped: that is the details panel, where an invalid value is allowed and shown by the validator.
 *  - Nothing here changes a field other than the one the method names. Fitting TotalFrames to a montage is an explicit command,
 *    never a side effect (combat-m5-editor 2.1 item 6).
 *
 * The owner of the model (the toolkit, or a test) creates an FGSTN_TimelineUndoClient next to it.
 */
class FGSTN_TimelineEditorModel
{
public:
	explicit FGSTN_TimelineEditorModel(const FGSTN_TimelineEditContext& InContext);
	FGSTN_TimelineEditorModel(const FGSTN_TimelineEditorModel&) = delete;
	FGSTN_TimelineEditorModel& operator=(const FGSTN_TimelineEditorModel&) = delete;

	// Target and read access

	const FGSTN_TimelineEditContext& GetContext() const { return Context; }

	/** The data being edited, null when the context is no longer valid */
	const FGSTN_CombatTimelineData* GetData() const { return Context.Get(); }

	int32 GetTotalFrames() const;
	int32 GetTrackCount() const;

	/** GetPlayableFrames of the data now (MinimumRulerFrames when the context is gone) */
	int32 GetPlayableFrames() const;

	// Frame vocabulary the gestures and the widgets share, so there is one definition of it

	/** Event fires on StartFrame and weapon switch on SwapFrame; both ignore EndFrame, so they occupy one frame and have no edges to move */
	static bool IsPointTrack(const FGSTN_TimelineTrack& Track);

	/** The frames the track occupies: [OutStart, OutEnd). One frame for a point track */
	static void GetTrackSpan(const FGSTN_TimelineTrack& Track, int32& OutStart, int32& OutEnd);

	/** One past the last frame a window may end on: TotalFrames, or a very large number while the timeline has no length yet */
	static int32 GetFrameLimit(const FGSTN_CombatTimelineData& Data);

	/** The ruler of a timeline that has no length yet shows (at least) this many frames: the playhead can go there, and Frame All frames it */
	static constexpr int32 MinimumRulerFrames = 30;

	/** One past the last frame any track occupies, or the first actionable frame if that is later. 0 for an empty timeline */
	static int32 GetContentEnd(const FGSTN_CombatTimelineData& Data);

	/**
	 * How many frames the playhead, the loop range and playback can use: TotalFrames, or, while it is 0 (a new inline timeline), the end
	 * of the content but never less than MinimumRulerFrames. Without it a timeline that has no length yet could not be scrubbed at all.
	 */
	static int32 GetPlayableFrames(const FGSTN_CombatTimelineData& Data);

	/** Frame clamped to the last playable frame [0, GetPlayableFrames - 1] */
	static int32 ClampPlayheadFrame(const FGSTN_CombatTimelineData& Data, int32 Frame);
	const FGSTN_TimelineTrack* GetTrack(int32 TrackIndex) const;

	/** Frames per second of the simulation tick, from the project's Network Prediction settings unless a test sets it */
	float GetTickRate() const { return TickRate; }
	void SetTickRate(float InTickRate) { TickRate = InTickRate > 0.f ? InTickRate : TickRate; }

	/** Gizmo drags create a key on a frame that has none (the preference starts from the editor settings) */
	bool IsAutoKeyEnabled() const { return bAutoKey; }
	void SetAutoKey(bool bInAutoKey) { bAutoKey = bInAutoKey; }

	/** Shared by every widget of the editor. Not part of the data: scrolling and zooming are never undone. After changing it, call NotifyViewChanged so the widgets repaint */
	FGSTN_TimelineViewTransform ViewTransform;

	void NotifyViewChanged() { OnViewChanged.Broadcast(); }

	// Playhead

	int32 GetPlayhead() const { return PlayheadFrame; }

	/** Moves the playhead to a frame clamped to [0, GetPlayableFrames - 1] (the length of the timeline, or its content while it has none). Returns whether it moved */
	bool SetPlayhead(int32 Frame);
	bool StepPlayhead(int32 DeltaFrames) { return SetPlayhead(PlayheadFrame + DeltaFrames); }

	// Play range: the stretch of frames playback loops over. Editor state like the view, not data: never undone, never saved

	/** Is a loop range set? */
	bool HasPlayRange() const { return PlayRangeEnd > PlayRangeStart; }

	/** [First, End) the playback loops over: the loop range cut to the timeline, or the whole timeline when there is none (or nothing of it is left inside) */
	void GetEffectivePlayRange(int32& OutFirst, int32& OutEnd) const;

	/** Sets the loop range [Start, End) cut to [0, TotalFrames]. An empty result clears it. Repaints, nothing else */
	void SetPlayRange(int32 Start, int32 End);
	void ClearPlayRange() { SetPlayRange(0, 0); }
	int32 GetPlayRangeStart() const { return PlayRangeStart; }
	int32 GetPlayRangeEnd() const { return PlayRangeEnd; }

	// Selection

	const TArray<FGSTN_Selection>& GetSelection() const { return Selection; }
	bool IsSelected(const FGSTN_Selection& Item) const { return Selection.Contains(Item); }

	/** Replaces the selection. Entries that do not exist in the data are dropped, duplicates merged */
	void SetSelection(TConstArrayView<FGSTN_Selection> NewSelection);
	void ClearSelection() { SetSelection(TConstArrayView<FGSTN_Selection>()); }
	void AddToSelection(const FGSTN_Selection& Item);
	void ToggleSelection(const FGSTN_Selection& Item);

	/** Selects one key and announces it (the viewport and the details panel follow OnKeySelected) */
	void SelectKey(int32 TrackIndex, int32 KeyIndex);

	/**
	 * After the data changed behind the model's back (undo, redo, a details panel edit that removed an array element): drops
	 * the selection that no longer exists and clamps the playhead. Broadcasts only what actually changed.
	 */
	void Revalidate();

	/** Called by the undo client after an undo or redo that touched the owner: Revalidate, then OnDataChanged */
	void HandleUndoRedo();

	// Window mutations (frames in [0, TotalFrames])

	/**
	 * Appends a track of the given struct, a child of FGSTN_TimelineTrack, with a window of LengthFrames starting at StartFrame, then
	 * lets the struct's track editor fill what a window cannot (FGSTN_TrackEditorRegistry, IGSTN_TrackEditor::InitNewTrack). The callers
	 * ask the track editor for the length (GetDefaultLength). Returns its index, INDEX_NONE if the struct is not a track
	 */
	int32 AddTrack(const UScriptStruct* TrackStruct, int32 StartFrame = 0, int32 LengthFrames = 1);

	/** Removes the tracks, selection included. Returns how many were removed */
	int32 RemoveTracks(TConstArrayView<int32> TrackIndices);

	/** Shifts the window by DeltaFrames, clamped so it stays inside the timeline with its length. Returns whether it moved */
	bool MoveWindow(int32 TrackIndex, int32 DeltaFrames);

	/** Moves one edge of the window to Frame, clamped so the window keeps at least one frame inside the timeline. A track that fires on one frame has no edges to move */
	bool ResizeWindow(int32 TrackIndex, EGSTN_WindowEdge Edge, int32 Frame);

	// Whole-timeline mutations

	bool SetTotalFrames(int32 Frames);
	bool SetFirstActionableFrame(int32 Frame);

	/**
	 * Sets the first frame of the cancellable tail to an explicit frame in [0, TotalFrames] (a tail that followed FirstActionableFrame
	 * with a negative StartFrame stops following it). Does nothing when the timeline has no tail: whether there is one is a details panel edit.
	 */
	bool SetTailStartFrame(int32 Frame);

	/**
	 * Sets TotalFrames to the length of the timeline's montage section at its play rate, in frames of the tick. The montage is
	 * loaded if needed. Does nothing when there is no montage or the length already matches. Never runs by itself.
	 */
	bool FitTotalFramesToMontage();

	/** The montage length in frames at the data's play rate, INDEX_NONE when it has no montage or Montage is null */
	static int32 ComputeMontageFrames(const FGSTN_CombatTimelineData& Data, const UAnimMontage* Montage, float TickRate);

	// Edits that write what the caller says (details panel proxy, E5)

	/** Runs Edit on the track inside one transaction. Not clamped. Returns false when the track does not exist */
	bool ApplyTrackEdit(int32 TrackIndex, TFunctionRef<void(FGSTN_TimelineTrack&)> Edit, const FText& Description);

	/** Runs Edit on the whole data inside one transaction. Not clamped. The selection is revalidated afterwards */
	bool ApplyDataEdit(TFunctionRef<void(FGSTN_CombatTimelineData&)> Edit, const FText& Description);

	// Keys (tracks that have a registered FGSTN_KeyedTrackAccess)

	/**
	 * Adds a key at Frame with values interpolated from its neighbours, and selects it. Returns its index, INDEX_NONE if the track has no keys or the
	 * frame is outside the timeline. Lane is the lane of a track that keeps several (the hitbox track: the hitbox the key belongs to); INDEX_NONE takes the
	 * lane of the first selected key of the track, or the first lane
	 */
	int32 AddKey(int32 TrackIndex, int32 Frame, int32 Lane = INDEX_NONE);

	/** Moves the selected keys together by DeltaFrames, clamped so the earliest and latest stay inside. Returns how many keys moved. The selection follows the keys through the re-sort */
	int32 MoveKeys(TConstArrayView<FGSTN_Selection> Keys, int32 DeltaFrames);

	/** Copies the keys DeltaFrames later (clamped to the timeline) and selects the copies. Returns how many were made */
	int32 DuplicateKeys(TConstArrayView<FGSTN_Selection> Keys, int32 DeltaFrames);

	/** Removes the keys. Returns how many were removed */
	int32 RemoveKeys(TConstArrayView<FGSTN_Selection> Keys);

	/** Sets the pose of one key (the gizmo). False when the track or key does not exist or the key has no pose */
	bool SetKeyTransform(int32 TrackIndex, int32 KeyIndex, const FTransform& NewTransform);

	/**
	 * The keys as text for the clipboard: a header naming the track and key structs, then one line per key with its frame
	 * relative to the earliest and the key struct's ExportText. Only keys of one track are copied (the first key's track).
	 * Empty when nothing can be copied.
	 */
	FString CopyKeysToText(TConstArrayView<FGSTN_Selection> Keys) const;

	/** CopyKeysToText to the system clipboard. Returns whether anything was copied */
	bool CopyKeys(TConstArrayView<FGSTN_Selection> Keys) const;

	/**
	 * Pastes keys from clipboard text onto a track, the earliest at Frame, and selects them. Refused (0 keys, no change) when the
	 * text is not ours, or its track or key struct differs from the target's. Keys that would land outside the timeline are
	 * clamped. Returns how many keys were pasted.
	 */
	int32 PasteKeysFromText(int32 TrackIndex, int32 Frame, const FString& Text);

	/** PasteKeysFromText with the system clipboard */
	int32 PasteKeys(int32 TrackIndex, int32 Frame);

	// Drags (one gesture = one undo step)

	/** Mouse-down: opens the transaction that every mutation until EndDrag joins. False if a drag is already open */
	bool BeginDrag(const FText& Description);

	/** Mouse-up: the drag becomes one undo step */
	void EndDrag();

	/** Escape: restores the data as it was at BeginDrag and leaves no undo step */
	void CancelDrag();

	bool IsDragging() const { return Drag.IsActive(); }

	// Events

	FGSTN_OnTimelineDataChanged OnDataChanged;
	FGSTN_OnTimelineSelectionChanged OnSelectionChanged;
	FGSTN_OnTimelinePlayheadChanged OnPlayheadChanged;
	FGSTN_OnTimelineKeySelected OnKeySelected;

	/** Scrolled or zoomed: repaint only, no data changed */
	FGSTN_OnTimelineViewChanged OnViewChanged;

private:
	/** Opens the transaction of a mutation (or joins the drag's) and ends with the notification */
	class FChangeScope;

	/** The notification every mutation ends with */
	void NotifyDataChanged();

	bool SetTotalFramesInternal(int32 Frames, const FText& Description);

	/** Is the selection entry a real thing in the data now? */
	bool IsSelectionValid(const FGSTN_Selection& Item) const;

	/** After a track's keys were re-sorted: points the selection at the same keys. OldToNew has one entry per old key */
	void RemapSelectionKeys(int32 TrackIndex, const TArray<int32>& OldToNew);

	/** Replaces the selection without validating it and broadcasts if it differs */
	void CommitSelection(TArray<FGSTN_Selection>&& NewSelection);

	FGSTN_TimelineEditContext Context;
	TArray<FGSTN_Selection> Selection;
	int32 PlayheadFrame = 0;
	int32 PlayRangeStart = 0;
	int32 PlayRangeEnd = 0;
	float TickRate = 62.5f;
	bool bAutoKey = true;
	FGSTN_TimelineDragTransaction Drag;
};
