// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class FGSTN_HurtboxEditModel;

/**
 * The hurtboxes of a set as a list (combat-m5-editor 8): position (the index a hit carries), name, bone, tags and priority, one row each. It is a view
 * over the model: a click selects (several with Ctrl/Shift) and the selection is the model's, so the viewport highlights the same hurtboxes and the
 * details panel shows the one selected. The list never edits; the details panel and the toolbar buttons do.
 */
class SGSTN_HurtboxList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_HurtboxList) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_HurtboxEditModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SGSTN_HurtboxList() override;

	/** Rows listed, for a test */
	int32 GetRowCount() const { return Rows.Num(); }

private:
	using FRow = TSharedPtr<int32>;

	void Rebuild();
	void SyncSelectionToList();
	void HandleSelectionChanged(FRow Item, ESelectInfo::Type SelectInfo);
	TSharedRef<ITableRow> OnGenerateRow(FRow Item, const TSharedRef<STableViewBase>& OwnerTable);

	TSharedPtr<FGSTN_HurtboxEditModel> Model;
	TArray<FRow> Rows;
	TSharedPtr<SListView<FRow>> List;
	FDelegateHandle ChangedHandle;
	FDelegateHandle SelectionHandle;

	/** The list is being set from the model: its own selection callback must not write back */
	bool bSyncing = false;
};
