// Pre-mission loadout screen: the Slate widget (drawn only, never focused: the subsystem reads the pad).

#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/SCompoundWidget.h"

struct FGSTN_LoadoutDraft;
class SVerticalBox;

/**
 * Toy-like panel: big rounded cards, one per row, the cursor's card highlighted, the current value between arrows. It holds no state of
 * its own; the subsystem calls Refresh after every change. Not focusable and hit-test invisible, so it never steals the pad or the mouse.
 */
class SGSTN_LoadoutMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_LoadoutMenu) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Rebuilds the rows from the draft; Hint is the button line, Feedback a refusal to show (empty for none) */
	void Refresh(const FGSTN_LoadoutDraft& Draft, const FText& Hint, const FString& Feedback);

private:
	TSharedPtr<SVerticalBox> Rows;

	FSlateRoundedBoxBrush PanelBrush = FSlateRoundedBoxBrush(FLinearColor(0.05f, 0.08f, 0.2f, 0.92f), 24.f);
	FSlateRoundedBoxBrush CardBrush = FSlateRoundedBoxBrush(FLinearColor(0.12f, 0.2f, 0.5f, 1.f), 16.f);
	FSlateRoundedBoxBrush CursorBrush = FSlateRoundedBoxBrush(FLinearColor(1.f, 0.75f, 0.1f, 1.f), 16.f);
};
