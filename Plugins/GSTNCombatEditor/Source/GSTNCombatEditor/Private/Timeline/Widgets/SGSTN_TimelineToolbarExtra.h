// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Widgets/SCompoundWidget.h"

/**
 * What the toolkit adds to the end of the panel's toolbar (combat-m5-editor 6): the loop range of playback, the preview skin and kit
 * combos and Validate.
 *
 * The skin and kit combos are stubs until the preview scene (E6) has a mesh to put a skin on: they are drawn so the toolbar has its
 * final shape and the tooltips are already where a designer will look for them, and they change nothing. Validate runs what the
 * toolkit gives it (the timeline rules, the findings go to the Message Log, E10) and is disabled when nothing is bound.
 */
class SGSTN_TimelineToolbarExtra : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_TimelineToolbarExtra) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineEditorModel>, Model)

		/** What Validate does. Unbound: the button is drawn disabled */
		SLATE_EVENT(FSimpleDelegate, OnValidate)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<FGSTN_TimelineEditorModel> Model;
};
