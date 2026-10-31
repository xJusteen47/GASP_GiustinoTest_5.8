// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_FindingsList.h"

#include "Styling/AppStyle.h"
#include "Validation/GSTN_ValidationMessageLog.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	const FSlateBrush* SeverityIcon(EGSTN_Severity Severity)
	{
		switch (Severity)
		{
		case EGSTN_Severity::Error: return FAppStyle::Get().GetBrush("Icons.ErrorWithColor");
		case EGSTN_Severity::Warning: return FAppStyle::Get().GetBrush("Icons.WarningWithColor");
		default: return FAppStyle::Get().GetBrush("Icons.InfoWithColor");
		}
	}
}

void SGSTN_FindingsList::Construct(const FArguments& InArgs)
{
	Store = InArgs._Store;
	OnFindingActivated = InArgs._OnFindingActivated;
	check(Store.IsValid());

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.f, 0.f, 0.f, 4.f))
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text_Lambda([this]() { return Store->GetHeadline(); })
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SAssignNew(List, SListView<FRow>)
			.ListItemsSource(&Rows)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SGSTN_FindingsList::OnGenerateRow)
			.OnMouseButtonDoubleClick_Lambda([this](FRow Item)
			{
				if (Item.IsValid())
				{
					OnFindingActivated.ExecuteIfBound(*Item);
				}
			})
		]
	];

	ChangedHandle = Store->OnChanged.AddSP(this, &SGSTN_FindingsList::Rebuild);
	Rebuild();
}

SGSTN_FindingsList::~SGSTN_FindingsList()
{
	if (Store.IsValid())
	{
		Store->OnChanged.Remove(ChangedHandle);
	}
}

void SGSTN_FindingsList::Rebuild()
{
	// A stale report keeps its rows (the headline says it is out of date); only a new report replaces them
	if (Store->IsStale())
	{
		return;
	}
	Rows.Reset();
	for (const FGSTN_ValidationFinding& Finding : Store->GetReport().Findings)
	{
		Rows.Add(MakeShared<FGSTN_ValidationFinding>(Finding));
	}
	if (List.IsValid())
	{
		List->RequestListRefresh();
	}
}

TSharedRef<ITableRow> SGSTN_FindingsList::OnGenerateRow(FRow Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FGSTN_ValidationFinding& Finding = *Item;
	const FText Detail = Finding.Detail;
	return SNew(STableRow<FRow>, OwnerTable)
		.Padding(FMargin(2.f, 3.f))
		.ToolTipText(Detail.IsEmpty() ? LOCTEXT("FindingTip", "Double-click to select the track and go to the frame") : Detail)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Top)
			.Padding(FMargin(0.f, 1.f, 6.f, 0.f))
			[
				SNew(SImage).Image(SeverityIcon(Finding.Severity))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(GSTNValidationLog::FormatFinding(Finding))
			]
		];
}

#undef LOCTEXT_NAMESPACE
