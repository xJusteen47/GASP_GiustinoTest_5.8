// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Widgets/SCompoundWidget.h"

/**
 * The strip above the timeline in inline mode (combat-m5-editor 2.1 item 3): whose timeline this is (the ability Blueprint), a note when
 * the ability also names a TimelineAsset, which wins at run time so what is being edited does not play, and the Extract to timeline
 * asset button, the fallback of the spec for a Blueprint whose inline editing is not wanted. Collapsed when the editor is on an asset.
 */
class SGSTN_InlineTimelineBar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_InlineTimelineBar) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineEditorModel>, Model)

		/** The Extract to timeline asset button */
		SLATE_EVENT(FSimpleDelegate, OnExtract)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** The text of the strip, for tests of the wording */
	static FText MakeBarText(const FGSTN_TimelineEditContext& Context);

private:
	TSharedPtr<FGSTN_TimelineEditorModel> Model;
};
