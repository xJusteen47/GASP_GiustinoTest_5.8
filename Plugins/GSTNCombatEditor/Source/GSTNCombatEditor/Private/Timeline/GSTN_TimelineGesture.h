// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineLayout.h"

/** What the mouse is doing to the timeline between mouse-down and mouse-up */
enum class EGSTN_GestureKind : uint8
{
	None,
	/** Dragging the selected windows together */
	MoveWindows,
	ResizeStart,
	ResizeEnd,
	MoveKeys,
	/** Ctrl+drag on a key: the copies are what moves */
	DuplicateKeys,
	/** A rectangle in empty lane space */
	BoxSelect,
	/** The FAF marker of the phase bar */
	MoveFirstActionable,
	/** The start handle of the cancellable tail */
	MoveTailStart
};

/**
 * One mouse gesture on the timeline as a state machine over the editor model, with no widget in it: the lanes and the phase
 * bar pass it the mouse position in lane pixels and it calls the model. Each data gesture is one drag of the model (one undo
 * step, Escape restores), opened at mouse-down and closed at mouse-up; the transaction itself only opens at the first change,
 * so a click that moves nothing leaves no step (combat-m5-editor 4.3).
 *
 * Moves are relative: the gesture remembers the frame under the mouse at mouse-down and how far it has actually moved things
 * (the model clamps), and applies the difference each update. Resizes and the markers follow the nearest frame boundary.
 */
class FGSTN_TimelineGesture
{
public:
	bool IsActive() const { return Kind != EGSTN_GestureKind::None; }
	EGSTN_GestureKind GetKind() const { return Kind; }

	/**
	 * Mouse-down on something a lane hit. Updates the selection (Shift toggles the item in the selection; a click on an item that
	 * is already selected keeps the whole selection so it can be dragged as a group) and arms the drag the hit stands for:
	 * the body moves the selected windows, an edge resizes, a key moves the selected keys, or duplicates them with bDuplicate.
	 * Returns whether a drag is armed. X is the lane pixel under the mouse.
	 */
	bool BeginOnHit(FGSTN_TimelineEditorModel& Model, const FGSTN_LaneHit& Hit, double X, bool bExtend, bool bDuplicate);

	/** Mouse-down on empty lane space. The rectangle starts at (X, Y) in content pixels; bAdditive keeps the current selection, otherwise it is cleared */
	void BeginBoxSelect(FGSTN_TimelineEditorModel& Model, double X, double Y, bool bAdditive);

	/** Mouse-down on the FAF marker of the phase bar. False if a drag is already open */
	bool BeginFirstActionable(FGSTN_TimelineEditorModel& Model);

	/** Mouse-down on the tail handle. False when the timeline has no tail or a drag is already open */
	bool BeginTailStart(FGSTN_TimelineEditorModel& Model);

	/** The mouse moved to lane pixel X (content pixel Y, only the box select reads it) */
	void Update(FGSTN_TimelineEditorModel& Model, double X, double Y);

	/** Mouse-up: the gesture becomes one undo step (or nothing, if it changed nothing) */
	void End(FGSTN_TimelineEditorModel& Model);

	/** Escape, or the mouse capture was lost: the data and the selection go back to how they were at mouse-down */
	void Cancel(FGSTN_TimelineEditorModel& Model);

	/** The rectangle of a box select in content pixels, false when no box select runs */
	bool GetBox(FVector2D& OutMin, FVector2D& OutMax) const;

	/** Which track indices a MoveWindows gesture moves (for tests and for painting a ghost) */
	const TArray<int32>& GetMovedTracks() const { return MovedTracks; }

private:
	void Reset();

	EGSTN_GestureKind Kind = EGSTN_GestureKind::None;

	/** Frame under the mouse at mouse-down, and the distance things have actually moved since (the model clamps) */
	int32 AnchorFrame = 0;
	int32 AppliedDelta = 0;

	/** The track a resize or a tail/FAF drag addresses */
	int32 ResizedTrack = INDEX_NONE;
	TArray<int32> MovedTracks;

	/** DuplicateKeys: the copies exist */
	bool bDuplicated = false;

	/** The tail handle starts on the frame it resolves to, so a click that does not move does not turn a "follow FAF" tail into an explicit one */
	int32 InitialTailStart = INDEX_NONE;

	/** Box select */
	FVector2D BoxStart = FVector2D::ZeroVector;
	FVector2D BoxEnd = FVector2D::ZeroVector;
	TArray<FGSTN_Selection> BoxPriorSelection;
	bool bBoxAdditive = false;

	/** Something actually changed (the click did not just press and release) */
	bool bMoved = false;

	/** A press on a member of a multiple selection that never moved narrows the selection to that member on release */
	bool bCollapseOnClick = false;
	FGSTN_Selection ClickItem;
};

/**
 * What the keyboard commands and the double-click do to the model, widget-free so the tests can run them. Each is one
 * transaction, and refuses quietly (returns false or 0) when there is nothing to act on.
 */
class FGSTN_TimelineActions
{
public:
	/** Frames of a window added by a double-click on a track that has no keys. E5's track editors replace this with their own default length */
	static constexpr int32 DefaultWindowFrames = 6;

	/** Double-click on empty lane space: a key on a keyed track, else a new track of the same kind at Frame. Returns whether something was added */
	static bool AddAtFrame(FGSTN_TimelineEditorModel& Model, int32 TrackIndex, int32 Frame);

	/** Delete: the selected keys, or, with no key selected, the selected tracks. Returns how many things were removed */
	static int32 DeleteSelection(FGSTN_TimelineEditorModel& Model);

	/** K: a key at the playhead on the track of the selection that has keys. Returns whether one was added */
	static bool AddKeyAtPlayhead(FGSTN_TimelineEditorModel& Model);

	/** Ctrl+C: the selected keys to the clipboard */
	static bool CopySelection(FGSTN_TimelineEditorModel& Model);

	/** Ctrl+V: the clipboard's keys at the playhead, on the track of the selection that has keys. Returns how many keys were pasted */
	static int32 PasteAtPlayhead(FGSTN_TimelineEditorModel& Model);

	/** Ctrl+D: copies of the selected keys with the earliest on the playhead (one frame later if it already is). Returns how many */
	static int32 DuplicateSelectionAtPlayhead(FGSTN_TimelineEditorModel& Model);

	/** Shift+, / Shift+. : the playhead to the previous (Direction < 0) or next key or window edge. Returns whether it moved */
	static bool JumpToAdjacentEdge(FGSTN_TimelineEditorModel& Model, int32 Direction);

	/** The track of the selection that has keys, INDEX_NONE if the selection has none (the target of K and Ctrl+V) */
	static int32 FindKeyedSelectionTrack(const FGSTN_TimelineEditorModel& Model);
};
