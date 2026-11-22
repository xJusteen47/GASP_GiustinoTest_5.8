// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_InputCoreTypes.h"
#include "Input/GSTN_InputHistory.h"

// The layout of the input display as plain data. This header and its .cpp include no Slate and no engine UI: the widget only
// paints what the layout says, so the columns, the gaps and the greyed presses are tested without a window
// (GSTNInput.Debug.InputDisplay.*; the drawing is SGSTN_InputStrip).

/** What a button does in one column (one column is one simulated frame) */
enum class EGSTN_InputCellKind : uint8
{
	None,
	Press,

	/** Held, between the press and the release */
	Hold,
	Release,

	/** Pressed and released on the same frame */
	Tap,
};

/** One button in one column */
struct GSTNINPUTDEBUG_API FGSTN_InputDisplayCell
{
	EGSTN_InputCellKind Kind = EGSTN_InputCellKind::None;

	/** The press was used by a command (a Hold or Release of that press carries it too): drawn greyed */
	bool bConsumed = false;

	/** For a consumed Press or Tap: index in the layout's Markers of the command that most likely used it, INDEX_NONE when unknown */
	int32 MarkerIndex = INDEX_NONE;
};

/** The stick in one column */
struct GSTNINPUTDEBUG_API FGSTN_InputDisplayStickCell
{
	/** The physical stick and the direction relative to the character: what the strip draws as arrows */
	EGSTN_StickDirection StickDirection = EGSTN_StickDirection::Neutral;
	EGSTN_StickDirection FacingDirection = EGSTN_StickDirection::Neutral;

	/** The same as numpad keys ('5' neutral), the notation of FGSTN_InputScript: kept for the tooltip and ToNotation, not drawn */
	TCHAR StickKey = TEXT('5');
	TCHAR FacingKey = TEXT('5');

	EGSTN_StickZone Zone = EGSTN_StickZone::Neutral;
	bool bFlick = false;

	/** The history has a stick event on this frame (the first column counts as one: it is where the strip starts). */
	bool bEvent = false;
};

/** A command that came out: the Input.Event.CommandExecuted of this display, drawn as a marker above its frame */
struct GSTNINPUTDEBUG_API FGSTN_InputDisplayMarker
{
	int32 Frame = INDEX_NONE;
	FGameplayTag Command;
};

/** An event of the strip in the notation of FGSTN_InputScript (combat-m1-core-input 4) */
struct GSTNINPUTDEBUG_API FGSTN_InputDisplayEvent
{
	int32 Frame = INDEX_NONE;

	/** Index of the column of the frame */
	int32 Column = INDEX_NONE;

	/** Frames since the previous event of the strip, 0 for the first */
	int32 Gap = 0;

	/** The item of the notation: "+A", "-A", "*A", "6t!", "5" */
	FString Item;

	/** "<Gap>:<Item>", the frame gap in the notation, e.g. "7:+A" for an attack press 7 frames after the previous event */
	FString GetLabel() const { return FString::Printf(TEXT("%d:%s"), Gap, *Item); }
};

/** What the display shows: a history and the commands that came out of it */
struct GSTNINPUTDEBUG_API FGSTN_InputDisplayData
{
	FGSTN_InputHistory History;

	/** Commands that were executed, any order; the frames are the frames of the history */
	TArray<FGSTN_InputDisplayMarker> Markers;
};

struct GSTNINPUTDEBUG_API FGSTN_InputDisplayLayoutParams
{
	/** Most frames shown, the newest ones (one column each). Events older than the history keeps (254 frames) cannot be shown anyway */
	int32 MaxColumns = 90;

	/** Rows of buttons (bit i of the held mask is row i) */
	int32 NumButtons = 6;

	/** The newest frame shown. INDEX_NONE = the latest frame of the history */
	int32 NowFrame = INDEX_NONE;

	/** A consumed press is attributed to the first command marker at most this many frames after the press */
	int32 MarkerReachFrames = 16;
};

/**
 * The input display as columns: one per frame, oldest first, so the newest frame is the last column and the strip draws it at
 * the right edge (a frame is a tick of the simulation). Row per button (press / hold / release / tap, consumed presses flagged),
 * the stick as a direction (the strip draws an arrow, 8 directions and neutral) with the facing-relative direction, the flick flag,
 * the events with the frame gap in notation, and the command markers. A pure function of the history: the same history gives
 * the same layout, and nothing here reads the world, the clock or the settings.
 */
struct GSTNINPUTDEBUG_API FGSTN_InputDisplayLayout
{
	/** Frame of column 0 and the number of columns; 0 columns for a history that has had no frame yet */
	int32 FirstFrame = 0;
	int32 NumColumns = 0;

	/** [Button][Column] */
	TArray<TArray<FGSTN_InputDisplayCell>> Buttons;

	/** [Column] */
	TArray<FGSTN_InputDisplayStickCell> Stick;

	/** [Column]: index in Markers of the command executed on that frame, INDEX_NONE if none */
	TArray<int32> ColumnMarker;

	/** The markers that fall inside the columns, oldest first */
	TArray<FGSTN_InputDisplayMarker> Markers;

	/** Stick and button events inside the columns, oldest first; on one frame the stick comes before the buttons (as in the examples of the notation) */
	TArray<FGSTN_InputDisplayEvent> Events;

	/** Column of a frame, INDEX_NONE outside */
	int32 FrameToColumn(int32 Frame) const { return Frame >= FirstFrame && Frame < FirstFrame + NumColumns ? Frame - FirstFrame : INDEX_NONE; }

	/**
	 * The events as a script FGSTN_InputScript can play, frames counted from the first event (which is therefore "0:"). What you
	 * would type into the scripted box to reproduce the strip. A strip whose first event is a flick, or that starts with a
	 * release, is not a legal script (the notation says so too); the text still shows what happened.
	 */
	FString ToNotation() const;

	static FGSTN_InputDisplayLayout Build(const FGSTN_InputDisplayData& Data, const FGSTN_InputDisplayLayoutParams& Params = FGSTN_InputDisplayLayoutParams());

	/** Numpad key of a direction ('5' for neutral) */
	static TCHAR GetNumpadKey(EGSTN_StickDirection Direction);

	/** The item of a stick event in the notation: key, zone letter ('a' aim, 't' tilt, none when neutral), '!' for a flick */
	static FString DescribeStick(const FGSTN_InputDisplayStickCell& Cell);

	/** A, B, C... for button 0, 1, 2... */
	static TCHAR GetButtonLetter(int32 Button) { return static_cast<TCHAR>(TEXT('A') + Button); }

	/** The explanation of a stick cell for the tooltip, the one place the numpad notation is shown: "4t (left, tilt)" */
	static FString DescribeStickForTooltip(const FGSTN_InputDisplayStickCell& Cell);

	/** "left", "up right", "neutral" */
	static FString GetDirectionName(EGSTN_StickDirection Direction);
};
