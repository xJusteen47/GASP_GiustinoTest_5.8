// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Widgets/SCompoundWidget.h"

class FGSTN_HurtboxEditModel;
class IStructureDetailsView;
struct FPropertyChangedEvent;

/**
 * The fields of the selected hurtbox (combat-m5-editor 8): name, bone, shape, zone tags, priority and the reaction scale, shown through a structure details
 * view over a copy, like the timeline's Details tab. An edit goes back to the set through the model (SetHurtbox), so the panel writes nothing itself
 * and an undo is the editor's. A slider dragged here is one undo step (the model's edit session). With nothing selected, or several, it says so.
 */
class SGSTN_HurtboxDetails : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_HurtboxDetails) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_HurtboxEditModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SGSTN_HurtboxDetails() override;

private:
	void Refresh();
	void HandleFinishedChanging(const FPropertyChangedEvent& Event);

	TSharedPtr<FGSTN_HurtboxEditModel> Model;
	TSharedPtr<IStructureDetailsView> View;

	/** The copy the view edits. Heap allocated so the view's pointer stays valid while the widget lives */
	TSharedRef<FGSTN_Hurtbox> Copy = MakeShared<FGSTN_Hurtbox>();
	int32 ShownIndex = INDEX_NONE;

	/** The panel is writing its copy to the model: the model's change broadcast must not reset the view under a slider */
	bool bCommitting = false;

	FDelegateHandle ChangedHandle;
	FDelegateHandle SelectionHandle;
};
