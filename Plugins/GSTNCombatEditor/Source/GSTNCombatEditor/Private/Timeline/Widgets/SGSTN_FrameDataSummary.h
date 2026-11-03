// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Timeline/GSTN_TimelineEditorModel.h"

/**
 * The frame data of the timeline on one line: "Startup 7 | Active 3 | Recovery 14 | FAF 24 | Total 30 (480 ms)", plus the tail
 * when there is one. Read-only, derived from the data on every read, so it can never disagree with the phase bar. The Frame Data
 * tab of the toolkit hosts it, with the validation findings once the validators exist.
 */
class SGSTN_FrameDataSummary : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_FrameDataSummary) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineEditorModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** The text shown, also for tests of the wording */
	static FText MakeSummaryText(const FGSTN_TimelineEditorModel& Model);

private:
	TSharedPtr<FGSTN_TimelineEditorModel> Model;
};
