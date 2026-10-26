// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Bake/GSTN_SocketBaker.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Widgets/SCompoundWidget.h"

class FGSTN_PreviewController;

/**
 * What the toolkit adds to the end of the panel's toolbar (combat-m5-editor 6): the loop range of playback, the preview skin and kit
 * combos and Validate.
 *
 * The subject picker (E6) names what the preview shows (a player kit and skin, or any skeletal mesh, plus extra meshes) and edits it.
 * Validate runs what the toolkit gives it (the timeline rules, the findings go to the Message Log, E10) and is disabled when nothing is bound.
 */
class SGSTN_TimelineToolbarExtra : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_TimelineToolbarExtra) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_TimelineEditorModel>, Model)

		/** What Validate does. Unbound: the button is drawn disabled */
		SLATE_EVENT(FSimpleDelegate, OnValidate)

		/** What Bake sockets does. Unbound: the button is drawn disabled */
		SLATE_EVENT(FSimpleDelegate, OnBake)

		/** Where the bake stands (a yellow dot on the button when it needs a bake) and why, for the tooltip */
		SLATE_ATTRIBUTE(EGSTN_BakeStatus, BakeStatus)
		SLATE_ATTRIBUTE(FText, BakeReason)

		/** The preview the subject picker drives. Unset: the picker is drawn disabled */
		SLATE_ARGUMENT(TSharedPtr<FGSTN_PreviewController>, Preview)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<FGSTN_TimelineEditorModel> Model;
};
