// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_InputSlot.h"
#include "Widgets/GSTN_InputDisplayLayout.h"
#include "Widgets/GSTN_InputDisplayScript.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

class SEditableTextBox;
class STextBlock;

/** Fills the data the display shows; false when there is nothing to show yet (no local pawn) */
DECLARE_DELEGATE_RetVal_OneParam(bool, FGSTN_InputDisplayFetch, FGSTN_InputDisplayData&);

/**
 * The strip itself: paints an FGSTN_InputDisplayLayout and answers hover with a tooltip. No state of its own beyond the layout,
 * the hover and the colours, so SGSTN_InputDisplay decides what it shows.
 *
 * Rows from the top: command markers, one row per button, the physical stick (numpad key and zone), the stick relative to the
 * character, and the events with the frame gap in notation. One column per frame, newest at the right edge.
 */
class SGSTN_InputStrip : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_InputStrip)
		: _NumFrames(90)
	{}
		/** Full names of the buttons, by bit; the rows are labelled with the letters A.. and the tooltip gives the name */
		SLATE_ARGUMENT(TArray<FString>, ButtonNames)

		/** Columns the strip is sized for: the width of a column is the widget width over this, so it does not change while the history is still short */
		SLATE_ARGUMENT(int32, NumFrames)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetLayout(const FGSTN_InputDisplayLayout& InLayout);
	void SetMessage(const FText& InMessage);

	/** The tooltip text for the column and row under a local position, empty when over nothing */
	FText DescribeAt(const FVector2D& Local) const;

	//~ SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	/** Width of a column for a widget of the given width (never below 3 pixels: a narrow window scrolls the oldest frames out of view) */
	float GetColumnWidth(float Width) const;

	/** The first column drawn, and the x of column 0 (can be negative: the oldest columns are cut off) */
	void GetVisibleRange(float Width, float ColumnWidth, int32& OutFirstColumn, float& OutOriginX) const;

	FGSTN_InputDisplayLayout Layout;
	FText Message;
	TArray<FString> ButtonNames;
	int32 NumFrames = 90;
};

/**
 * The input display (combat-m5-editor 10.1): the strip over a history of the combat component, newest at the right, plus
 * an optional box for a scripted history. A pure view: it reads through the Fetch delegate (the debug module feeds it from the
 * local pawn) and writes nothing back.
 *
 * Scripted mode: type a notation of FGSTN_InputScript (combat-m1-core-input 4, e.g. "0:4t 6:6t! 7:+A"), press Enter, and the strip
 * shows that history with the press the winning command used greyed, a marker for the command and its tag in the line under the
 * box ("what does this input do"). The slots it resolves against are SetScriptSlots; the default is the plugin's standard command
 * set. Clearing the box goes back to the live view.
 */
class SGSTN_InputDisplay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_InputDisplay)
		: _NumFrames(90)
		, _bShowScriptBox(false)
	{}
		/** Source of the live history. Unbound = nothing live to show (only the scripted mode works) */
		SLATE_EVENT(FGSTN_InputDisplayFetch, Fetch)

		/** Most frames shown */
		SLATE_ARGUMENT(int32, NumFrames)

		/** Show the box for scripted histories */
		SLATE_ARGUMENT(bool, bShowScriptBox)

		/** Button bit order of the notation and the rows (letters A..). Default: the six standard buttons of the combat plugin */
		SLATE_ARGUMENT(TArray<FGameplayTag>, ButtonTags)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Shows this notation instead of the live history (empty = back to live). Returns false when it does not parse; the strip then keeps what it showed and the line under the box says why */
	bool SetScriptedNotation(const FString& Notation);

	bool IsScripted() const { return bScripted; }

	/** The slots the scripted notation resolves against (a kit's composed command set) */
	void SetScriptSlots(TArray<FGSTN_InputSlotSpec> InSlots);

	/** The result of the last scripted notation */
	const FGSTN_InputDisplayScriptResult& GetScriptResult() const { return ScriptResult; }

	/** What the strip shows now; for tests of the widget glue */
	const FGSTN_InputDisplayLayout& GetLayout() const { return Layout; }

	/** Reads the source and rebuilds the layout if the data changed. Tick calls it; public so a test can drive it without a window */
	void Refresh();

	//~ SWidget
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	void HandleNotationCommitted(const FText& Text, ETextCommit::Type CommitType);
	void ApplyData(const FGSTN_InputDisplayData& Data);
	void SetResultText(const FText& Text);

	FGSTN_InputDisplayFetch FetchDelegate;
	int32 NumFrames = 90;
	TArray<FGameplayTag> ButtonTags;

	TSharedPtr<SGSTN_InputStrip> Strip;
	TSharedPtr<SEditableTextBox> NotationBox;
	TSharedPtr<STextBlock> ResultText;

	bool bScripted = false;
	TArray<FGSTN_InputSlotSpec> ScriptSlots;
	FGSTN_InputDisplayScriptResult ScriptResult;

	FGSTN_InputDisplayLayout Layout;

	/** The data the layout was built from, to skip the rebuild (and the repaint) on a frame where nothing changed */
	FGSTN_InputDisplayData LastData;
	bool bHasLastData = false;
};
