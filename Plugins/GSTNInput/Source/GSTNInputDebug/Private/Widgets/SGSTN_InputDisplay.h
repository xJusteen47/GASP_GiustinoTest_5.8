// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#pragma once

#include "CoreMinimal.h"
#include "Display/GSTN_InputDisplayLayout.h"
#include "Display/GSTN_InputDisplayScript.h"
#include "GameplayTagContainer.h"
#include "Sources/GSTN_InputSourceRegistry.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

class FGSTN_LocalInputSource;
class SEditableTextBox;
class STextBlock;

/** Fills the data the display shows; false when there is nothing to show yet (no local pawn) */
DECLARE_DELEGATE_RetVal_OneParam(bool, FGSTN_InputDisplayFetch, FGSTN_InputDisplayData&);

/**
 * The strip itself: paints an FGSTN_InputDisplayLayout and answers hover with a tooltip. No state of its own beyond the layout, the
 * button names and the message, so SGSTN_InputDisplay decides what it shows. All sizes come from FGSTN_InputDisplayMetrics.
 *
 * Rows from the top: command markers, the stick as an arrow (eight directions and a dot for neutral), the stick relative to the character
 * as a small arrow where it differs, one row per button with coloured rounded icons that carry the button's letter, and under them the frame
 * gaps. One column per frame, never narrower than 18 px, the newest at the right edge; a window too narrow for the history cuts the oldest
 * frames. Numpad notation is only in the tooltip.
 */
class SGSTN_InputStrip : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_InputStrip)
		: _NumFrames(90)
	{}
		/** Full names of the buttons, by bit; the rows are labelled with the letters A.. and the tooltip gives the name */
		SLATE_ARGUMENT(TArray<FString>, ButtonNames)

		/** Most columns the history can fill (the history keeps 254 frames); how many are drawn depends on the width */
		SLATE_ARGUMENT(int32, NumFrames)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetLayout(const FGSTN_InputDisplayLayout& InLayout);
	void SetMessage(const FText& InMessage);
	void SetButtonNames(const TArray<FString>& InNames);

	/** The tooltip text for the column and row under a local position, empty when over nothing */
	FText DescribeAt(const FVector2D& Local) const;

	/** The oldest column drawn at the current width (0 before the first arrangement) */
	int32 GetFirstVisibleColumn() const;

	//~ SWidget
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

private:
	FGSTN_InputDisplayLayout Layout;
	FText Message;
	TArray<FString> ButtonNames;
	int32 NumFrames = 90;
};

/**
 * The input display: the strip over the history of one local player, newest at the right, plus optionally a box for a scripted history and
 * a picker for which player to follow. A pure view: it reads through a local source or a fetch delegate and writes nothing back.
 *
 * Scripted mode: type a notation of FGSTN_InputScript (combat-m1-core-input 4, e.g. "0:4t 6:6t! 7:+A"), press Enter, and the strip shows that
 * history with the press the winning command used greyed, a marker for the command and its tag in the line under the box ("what does
 * this input do"). The command set and the buttons are the profile of the source it follows (the combat adapter: the standard set and the six
 * combat buttons), or the registry's default profile before there is a source; with no command set known the box only plays the notation.
 * Clearing the box goes back to the live view.
 *
 * Which player the live view shows, in the editor tab: the one of the play window that has the focus, or had it last (so typing in the script
 * box keeps showing the window you were looking at), or the one picked in the combo.
 */
class SGSTN_InputDisplay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_InputDisplay)
		: _NumFrames(90)
		, _bShowScriptBox(false)
		, _bShowPlayerPicker(false)
	{}
		/** Source of the live history, for a host that has its own (a test). Unbound = nothing live to show (only the scripted mode works) */
		SLATE_EVENT(FGSTN_InputDisplayFetch, Fetch)

		/** The local-player source: reads the pawns through the registry, owns the choice of the player, and gives the profile. Wins over Fetch */
		SLATE_ARGUMENT(TSharedPtr<FGSTN_LocalInputSource>, LocalSource)

		/** Most columns the history can fill */
		SLATE_ARGUMENT(int32, NumFrames)

		/** Show the box for scripted histories */
		SLATE_ARGUMENT(bool, bShowScriptBox)

		/** Show the combo that picks the player (needs LocalSource) */
		SLATE_ARGUMENT(bool, bShowPlayerPicker)

		/** Buttons and command set when there is no local source and no registered default (a test) */
		SLATE_ARGUMENT(TOptional<FGSTN_InputDisplayProfile>, Profile)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Shows this notation instead of the live history (empty = back to live). Returns false when it does not parse; the strip then keeps what it showed and the line under the box says why */
	bool SetScriptedNotation(const FString& Notation);

	bool IsScripted() const { return bScripted; }

	/** The command set the scripted notation resolves against (replaces the profile's) */
	void SetScriptSlots(TArray<FGSTN_InputSlotSpec> InSlots);

	/** The result of the last scripted notation */
	const FGSTN_InputDisplayScriptResult& GetScriptResult() const { return ScriptResult; }

	/** What the strip shows now; for tests of the widget glue */
	const FGSTN_InputDisplayLayout& GetLayout() const { return Layout; }

	/** The buttons and command set in use */
	const FGSTN_InputDisplayProfile& GetProfile() const { return Profile; }

	/** Reads the source and rebuilds the layout if the data changed. Tick calls it; public so a test can drive it without a window */
	void Refresh();

	//~ SWidget
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	void HandleNotationCommitted(const FText& Text, ETextCommit::Type CommitType);
	void ApplyData(const FGSTN_InputDisplayData& Data);
	void ApplyProfile(const FGSTN_InputDisplayProfile& InProfile);
	void SetResultText(const FText& Text);
	TSharedRef<SWidget> MakePlayerMenu();
	FText GetPickerLabel() const;
	FText GetNoDataMessage() const;

	FGSTN_InputDisplayFetch FetchDelegate;
	TSharedPtr<FGSTN_LocalInputSource> LocalSource;
	int32 NumFrames = 90;

	/** The buttons and the command set in use; replaced when the source changes */
	FGSTN_InputDisplayProfile Profile;
	const IGSTN_InputHistorySource* ProfileSource = nullptr;
	bool bProfileFixed = false;

	TSharedPtr<SGSTN_InputStrip> Strip;
	TSharedPtr<SEditableTextBox> NotationBox;
	TSharedPtr<STextBlock> ResultText;

	bool bScripted = false;
	FGSTN_InputDisplayScriptResult ScriptResult;

	FGSTN_InputDisplayLayout Layout;

	/** The data the layout was built from, to skip the rebuild (and the repaint) on a frame where nothing changed */
	FGSTN_InputDisplayData LastData;
	bool bHasLastData = false;
};
