// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Widgets/SCompoundWidget.h"

/**
 * "Length [n] frames  FAF [n]": the two whole-timeline numbers a designer has to be able to type, TotalFrames and FirstActionableFrame.
 * A new inline timeline has TotalFrames 0 and there was nowhere to set it. Both go through the model, so each edit is one undo step
 * ("Set timeline length", "Move first actionable frame"), and dragging a spin box is one step for the whole drag.
 * The timeline panel shows it under the lanes and the Frame Data tab above the summary.
 */
class SGSTN_TimelineLengthFields : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_TimelineLengthFields) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineEditorModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};
