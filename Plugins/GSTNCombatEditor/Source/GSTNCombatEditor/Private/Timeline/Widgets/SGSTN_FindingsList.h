// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_FindingsStore.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

/**
 * The validation findings of the last Validate as a list, one row per finding with a severity icon and the same "[Code] message" text
 * the Message Log shows. A double click on a row calls OnFindingActivated (the toolkit selects the track and puts the playhead on the
 * frame). The list follows the store: it is rebuilt when Validate runs and says when the findings are out of date.
 */
class SGSTN_FindingsList : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnFindingActivated, const FGSTN_ValidationFinding&);

	SLATE_BEGIN_ARGS(SGSTN_FindingsList) {}
		SLATE_ARGUMENT(TSharedPtr<FGSTN_FindingsStore>, Store)
		SLATE_EVENT(FOnFindingActivated, OnFindingActivated)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SGSTN_FindingsList() override;

	/** Rows currently listed, for a test */
	int32 GetRowCount() const { return Rows.Num(); }

private:
	using FRow = TSharedPtr<FGSTN_ValidationFinding>;

	void Rebuild();
	TSharedRef<ITableRow> OnGenerateRow(FRow Item, const TSharedRef<STableViewBase>& OwnerTable);

	TSharedPtr<FGSTN_FindingsStore> Store;
	FOnFindingActivated OnFindingActivated;
	TArray<FRow> Rows;
	TSharedPtr<SListView<FRow>> List;
	FDelegateHandle ChangedHandle;
};
