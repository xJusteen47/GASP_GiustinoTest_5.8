// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Widgets/SCompoundWidget.h"

/**
 * What the toolkit adds to the end of the panel's toolbar (combat-m5-editor 6): the loop range of playback, the preview skin and kit
 * combos and Validate.
 *
 * The skin and kit combos are stubs until the preview scene (E6) has a mesh to put a skin on, and Validate is disabled until the
 * validators (E10) exist: they are drawn so the toolbar has its final shape and the chords and tooltips are already where a
 * designer will look for them, and they change nothing.
 */
class SGSTN_TimelineToolbarExtra : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_TimelineToolbarExtra) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineEditorModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<FGSTN_TimelineEditorModel> Model;
};
