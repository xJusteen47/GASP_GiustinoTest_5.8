// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_InputCoreTypes.h"
#include "Display/GSTN_InputDisplayLayout.h"

/**
 * A frame gap between two events of the strip, as a number under the strip: the frames from the event at FromColumn to the one at
 * ToColumn, centred between them. Only gaps of more than one frame get one: on adjacent frames the number would be a "1" on every pair.
 */
struct GSTNINPUTDEBUG_API FGSTN_InputGapLabel
{
	int32 FromColumn = INDEX_NONE;
	int32 ToColumn = INDEX_NONE;
	int32 Frames = 0;
	FString Text;

	/** Left edge and width of the text in strip pixels, already inside the strip */
	float X = 0.f;
	float Width = 0.f;
};

/**
 * One drawn slot of the strip: a single frame, or (when idle runs are compressed) a run of frames in which nothing changed, drawn as
 * one narrow column that shows its length. Slots are in frame order; X and Width are strip pixels.
 */
struct GSTNINPUTDEBUG_API FGSTN_InputDisplaySlot
{
	/** First layout column of the slot and how many it covers (1 for a plain frame) */
	int32 FirstColumn = 0;
	int32 NumColumns = 1;
	float X = 0.f;
	float Width = 0.f;

	bool IsCompressed() const { return NumColumns > 1; }
	int32 GetLastColumn() const { return FirstColumn + NumColumns - 1; }
};

/** How a press or tap icon is painted: the fill, an outline (alpha 0 = none) and the letter */
struct GSTNINPUTDEBUG_API FGSTN_InputPressIconStyle
{
	FLinearColor Fill;
	FLinearColor Outline;
	FLinearColor Letter;
};

/**
 * The sizes and the pure geometry of the input display strip: row heights, how many columns fit, where a column is, the arrow of a stick
 * direction, which frame-gap labels fit, the colour of a button. No Slate in this header or its source: the strip only paints what these
 * functions say, so "nothing overlaps", "newest on the right" and "an arrow for each of the nine directions" are tested without a window
 * (GSTNInput.Debug.InputDisplay.*).
 *
 * The strip is laid out left to right as: a gutter with the row names, then the columns. A column is one simulated frame and is never
 * narrower than MinColumnWidth, so a window that is too narrow for the whole history shows the newest frames and cuts the oldest, and
 * a wide one stretches the columns to fill its width. The newest frame is always the rightmost column.
 */
struct GSTNINPUTDEBUG_API FGSTN_InputDisplayMetrics
{
	/** Names of the rows, left of the first column */
	static constexpr float GutterWidth = 26.f;

	/** The narrowest a column gets: wide enough for the button icon (14 px) with a gap, so icons and arrows never touch */
	static constexpr float MinColumnWidth = 18.f;

	// Row heights from the top: command markers, the stick, the stick relative to the character, one row per button, the frame gaps
	static constexpr float MarkerRowHeight = 10.f;
	static constexpr float StickRowHeight = 24.f;
	static constexpr float FacingRowHeight = 10.f;
	static constexpr float ButtonRowHeight = 16.f;
	static constexpr float GapRowHeight = 14.f;

	/** Smallest row heights the strip shrinks to when its height is short, before it hides the relative-direction row */
	static constexpr float MinMarkerRowHeight = 6.f;
	static constexpr float MinStickRowHeight = 16.f;
	static constexpr float MinFacingRowHeight = 8.f;
	static constexpr float MinButtonRowHeight = 12.f;
	static constexpr float MinGapRowHeight = 10.f;

	/** Space kept right of the newest column, so a hold bar or an icon in it does not touch the edge of the strip */
	static constexpr float RightMargin = 4.f;

	/** The edge of a button icon inside its row cell, so the icons of two rows or two columns leave a gap */
	static constexpr float IconSize = 14.f;

	/** Space kept between two frame-gap labels, in pixels */
	static constexpr float GapLabelSpacing = 4.f;

	/** The row heights the strip paints with: the defaults, or smaller ones that fit a given height */
	struct FRowHeights
	{
		float Marker = MarkerRowHeight;
		float Stick = StickRowHeight;
		float Facing = FacingRowHeight;
		float Button = ButtonRowHeight;
		float Gap = GapRowHeight;
		float GetTotal(int32 NumButtons) const { return Marker + Stick + Facing + FMath::Max(NumButtons, 0) * Button + Gap; }
	};

	/**
	 * Row heights that fit Height: the defaults when they fit; otherwise every row shrinks in proportion towards its Min* height; if even
	 * those do not fit, the relative-direction row (Facing = 0) goes. Below that the strip is clipped, the newest content being on top.
	 */
	static FRowHeights FitRowHeights(float Height, int32 NumButtons);

	/** Height of the whole strip for a number of button rows */
	static float GetStripHeight(int32 NumButtons);

	/** Columns that fit a strip of the given width, at least MinColumnWidth each, at least 1 and at most MaxColumns */
	static int32 GetVisibleColumns(float Width, int32 MaxColumns);

	/** The width of one column when VisibleColumns share the width left of the gutter */
	static float GetColumnWidth(float Width, int32 VisibleColumns);

	/**
	 * Left edge of a column (0 = oldest) of a history that has NumColumns of them: the last column ends at the right edge of the strip
	 * whatever the count, so the newest frame is on the right and a short history sits against it
	 */
	static float GetColumnX(float Width, float ColumnWidth, int32 NumColumns, int32 Column);

	/** The oldest column that is drawn: the columns before it were cut off at the left edge */
	static int32 GetFirstVisibleColumn(int32 NumColumns, int32 VisibleColumns);

	/**
	 * The arrow of a stick direction in screen space (x right, y down), unit length for the eight directions and zero for Neutral, which the
	 * strip draws as a dot. Up is (0, -1), UpRight is (0.707, -0.707).
	 */
	static FVector2f GetArrowDirection(EGSTN_StickDirection Direction);

	/**
	 * The frame-gap labels that fit under the strip: for each pair of neighbouring events more than one frame apart whose two columns are
	 * both in view, the gap in frames centred between them. A label that would touch the one before it (GapLabelSpacing) is left out, so
	 * labels never overlap whatever the column width. CharWidth is the width of one digit in the font the strip uses.
	 * @param FirstColumn the oldest column in view (GetFirstVisibleColumn)
	 */
	static TArray<FGSTN_InputGapLabel> ChooseGapLabels(const FGSTN_InputDisplayLayout& Layout, int32 FirstColumn, float Width, float ColumnWidth, float CharWidth);

	/** Width of a compressed idle run: wide enough for a three digit frame count in the small font */
	static constexpr float CompressedColumnWidth = 22.f;

	/** The shortest idle run that is compressed: shorter ones take less room drawn as they are */
	static constexpr int32 MinCompressedRun = 3;

	/**
	 * A column in which nothing happens: no button pressed, released or tapped, no stick event, no command. The held state
	 * carries over from the column before, so a run of such columns is one picture repeated.
	 */
	static bool IsIdleColumn(const FGSTN_InputDisplayLayout& Layout, int32 Column);

	/**
	 * The slots to draw, oldest first, the last ending at the right edge. Without bCompress one slot per frame (GetVisibleColumns,
	 * GetColumnWidth, GetColumnX). With it, every run of at least MinCompressedRun idle columns is one CompressedColumnWidth slot (the
	 * fighting game way of showing "nothing for N frames"); the newest slots that fit the width and cover at most MaxFrames frames are
	 * kept and stretched a little to fill the width. Never fewer than one slot for a non-empty layout.
	 */
	static TArray<FGSTN_InputDisplaySlot> ComputeSlots(const FGSTN_InputDisplayLayout& Layout, float Width, int32 MaxFrames, bool bCompress);

	/**
	 * Same as the other overload with the centre of each column given by CentreOf (column to strip x), for a strip whose columns are not
	 * evenly spaced
	 */
	static TArray<FGSTN_InputGapLabel> ChooseGapLabels(const FGSTN_InputDisplayLayout& Layout, int32 FirstColumn, float Width, TFunctionRef<float(int32)> CentreOf, float CharWidth);

	/**
	 * Paint style of a press or tap icon. A press a command used stays the button's own colour, dimmed, with a bright outline of
	 * that colour and the letter in it: never a grey that reads as a blank square.
	 */
	static FGSTN_InputPressIconStyle GetPressIconStyle(int32 Button, bool bConsumed);

	/** The colour of a button's icon: six distinct hues for A to F, then the same hues a step darker */
	static FLinearColor GetButtonColor(int32 Button);

	/** Black or white, whichever reads on the button's colour */
	static FLinearColor GetButtonTextColor(int32 Button);
};
