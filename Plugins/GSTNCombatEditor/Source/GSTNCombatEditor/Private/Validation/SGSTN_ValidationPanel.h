// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Validation/GSTN_ValidationPanelModel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

/**
 * The validation panel (combat-m5-editor 9.1): every finding of every combat asset in one tree, grouped by asset with a severity icon on each
 * row. Validate all runs the validators over the Asset Registry (and writes the Message Log, category GSTNCombat); a double click on a row opens
 * the asset and, for a timeline finding, selects the track and puts the playhead on the frame. Filters for errors, warnings and notes, and a
 * text filter, act on the result already in hand. A view of FGSTN_ValidationPanelModel, which holds all the state.
 */
class SGSTN_ValidationPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGSTN_ValidationPanel) {}
		/** Shared with the host or a test; the panel makes its own when none is given */
		SLATE_ARGUMENT(TSharedPtr<FGSTN_ValidationPanelModel>, Model)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SGSTN_ValidationPanel() override;

	const TSharedRef<FGSTN_ValidationPanelModel>& GetModel() const { return Model; }

	/** Runs Validate all now (what the button does) */
	void ValidateAll();

private:
	using FNodePtr = TSharedPtr<FGSTN_ValidationPanelModel::FNode>;

	TSharedRef<ITableRow> OnGenerateRow(FNodePtr Node, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetChildren(FNodePtr Node, TArray<FNodePtr>& OutChildren);
	void OnActivated(FNodePtr Node);
	void Refresh();
	TSharedRef<SWidget> MakeSeverityToggle(EGSTN_Severity Severity);

	TSharedRef<FGSTN_ValidationPanelModel> Model = MakeShared<FGSTN_ValidationPanelModel>();
	TSharedPtr<STreeView<FNodePtr>> Tree;
	FDelegateHandle ChangedHandle;
};
