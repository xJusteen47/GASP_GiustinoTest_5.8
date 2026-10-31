// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FGSTN_PreviewController;
class IStructureDetailsView;

/**
 * The subject picker of the timeline toolbar (combat-m5-editor 6, as amended for any skeletal mesh): a combo that names what the preview shows
 * and opens the subject's properties. The first property chooses the kind: a player kit and skin, or any skeletal mesh (an enemy); below it the
 * assets and the list of extra meshes, each with the bone or socket it hangs from and a full transform. Every edit applies at once and is
 * stored for the asset in the user's config.
 */
class SGSTN_PreviewSubjectPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_PreviewSubjectPicker) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_PreviewController>, Controller)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakeMenuContent();

	TSharedPtr<FGSTN_PreviewController> Controller;
	TSharedPtr<IStructureDetailsView> DetailsView;
};
