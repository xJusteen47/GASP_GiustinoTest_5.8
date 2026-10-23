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

	/** The edge of a button icon inside its row cell, so the icons of two rows or two columns leave a gap */
	static constexpr float IconSize = 14.f;

	/** Space kept between two frame-gap labels, in pixels */
	static constexpr float GapLabelSpacing = 4.f;

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

	/** The colour of a button's icon: six distinct hues for A to F, then the same hues a step darker */
	static FLinearColor GetButtonColor(int32 Button);

	/** Black or white, whichever reads on the button's colour */
	static FLinearColor GetButtonTextColor(int32 Button);
};
