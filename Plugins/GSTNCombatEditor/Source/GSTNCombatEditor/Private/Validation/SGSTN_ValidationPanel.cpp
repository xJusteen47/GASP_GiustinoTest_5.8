// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Validation/SGSTN_ValidationPanel.h"

#include "Styling/AppStyle.h"
#include "Validation/GSTN_ValidationMessageLog.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
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

	FText SeverityLabel(EGSTN_Severity Severity)
	{
		switch (Severity)
		{
		case EGSTN_Severity::Error: return LOCTEXT("ShowErrors", "Errors");
		case EGSTN_Severity::Warning: return LOCTEXT("ShowWarnings", "Warnings");
		default: return LOCTEXT("ShowNotes", "Notes");
		}
	}
}

void SGSTN_ValidationPanel::Construct(const FArguments& InArgs)
{
	if (InArgs._Model.IsValid())
	{
		Model = InArgs._Model.ToSharedRef();
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(8.f, 8.f, 8.f, 4.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(SButton)
				.Text(LOCTEXT("ValidateAll", "Validate all"))
				.ToolTipText(LOCTEXT("ValidateAllTip", "Check every timeline, ability Blueprint, hurtbox set, kit, weapon and loadout of the project, and write the findings to the Message Log"))
				.OnClicked_Lambda([this]() { ValidateAll(); return FReply::Handled(); })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				MakeSeverityToggle(EGSTN_Severity::Error)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				MakeSeverityToggle(EGSTN_Severity::Warning)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				MakeSeverityToggle(EGSTN_Severity::Info)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(SSearchBox)
				.HintText(LOCTEXT("FilterHint", "Filter by code, message or asset"))
				.OnTextChanged_Lambda([this](const FText& Text)
				{
					FGSTN_ValidationPanelModel::FFilter Filter = Model->GetFilter();
					Filter.Text = Text.ToString();
					Model->SetFilter(Filter);
				})
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(8.f, 2.f))
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return Model->GetSummary(); })
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		.Padding(FMargin(8.f, 4.f, 8.f, 8.f))
		[
			SAssignNew(Tree, STreeView<FNodePtr>)
			.TreeItemsSource(&Model->GetRoots())
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SGSTN_ValidationPanel::OnGenerateRow)
			.OnGetChildren(this, &SGSTN_ValidationPanel::OnGetChildren)
			.OnMouseButtonDoubleClick(this, &SGSTN_ValidationPanel::OnActivated)
		]
	];

	ChangedHandle = Model->OnChanged.AddSP(this, &SGSTN_ValidationPanel::Refresh);
	Refresh();
}

SGSTN_ValidationPanel::~SGSTN_ValidationPanel()
{
	Model->OnChanged.Remove(ChangedHandle);
}

TSharedRef<SWidget> SGSTN_ValidationPanel::MakeSeverityToggle(EGSTN_Severity Severity)
{
	return SNew(SCheckBox)
		.IsChecked_Lambda([this, Severity]()
		{
			const FGSTN_ValidationPanelModel::FFilter& Filter = Model->GetFilter();
			const bool bOn = Severity == EGSTN_Severity::Error ? Filter.bErrors : Severity == EGSTN_Severity::Warning ? Filter.bWarnings : Filter.bInfos;
			return bOn ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		})
		.OnCheckStateChanged_Lambda([this, Severity](ECheckBoxState State)
		{
			FGSTN_ValidationPanelModel::FFilter Filter = Model->GetFilter();
			const bool bOn = State == ECheckBoxState::Checked;
			(Severity == EGSTN_Severity::Error ? Filter.bErrors : Severity == EGSTN_Severity::Warning ? Filter.bWarnings : Filter.bInfos) = bOn;
			Model->SetFilter(Filter);
		})
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(2.f, 0.f, 4.f, 0.f))
			[
				SNew(SImage).Image(SeverityIcon(Severity))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 8.f, 0.f))
			[
				SNew(STextBlock).Text(SeverityLabel(Severity))
			]
		];
}

void SGSTN_ValidationPanel::ValidateAll()
{
	GSTNValidateAll::FOptions Options;
	Options.bShowProgress = true;
	Model->SetResult(GSTNValidateAll::Run(Options));

	// One page in the Message Log for the whole run: every finding keeps its asset link there
	GSTNValidationLog::WriteReport(Model->GetResult().Flatten(), LOCTEXT("ValidateAllPage", "Validate all"), false, false);
}

void SGSTN_ValidationPanel::Refresh()
{
	if (Tree.IsValid())
	{
		Tree->RequestTreeRefresh();
		for (const FNodePtr& Root : Model->GetRoots())
		{
			Tree->SetItemExpansion(Root, true);
		}
	}
}

void SGSTN_ValidationPanel::OnGetChildren(FNodePtr Node, TArray<FNodePtr>& OutChildren)
{
	if (Node.IsValid())
	{
		OutChildren = Node->Children;
	}
}

TSharedRef<ITableRow> SGSTN_ValidationPanel::OnGenerateRow(FNodePtr Node, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (const FGSTN_ValidationFinding* Finding = Model->GetFinding(*Node))
	{
		return SNew(STableRow<FNodePtr>, OwnerTable)
			.Padding(FMargin(2.f, 3.f))
			.ToolTipText(Finding->Detail.IsEmpty() ? LOCTEXT("FindingTip", "Double-click to open the asset and go to the place") : Finding->Detail)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Top)
				.Padding(FMargin(0.f, 1.f, 6.f, 0.f))
				[
					SNew(SImage).Image(SeverityIcon(Finding->Severity))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text(GSTNValidationLog::FormatFinding(*Finding))
				]
			];
	}

	const GSTNValidateAll::FAssetResult& Asset = Model->GetAsset(*Node);
	return SNew(STableRow<FNodePtr>, OwnerTable)
		.Padding(FMargin(2.f, 4.f))
		.ToolTipText(FText::FromString(Asset.AssetData.GetObjectPathString()))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.f, 0.f, 6.f, 0.f))
			[
				SNew(SImage).Image(SeverityIcon(Model->GetWorstSeverity(*Node)))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("AssetRow", "{0}  ({1})"), FText::FromName(Asset.AssetData.AssetName), FText::AsNumber(Node->Children.Num())))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]
		];
}

void SGSTN_ValidationPanel::OnActivated(FNodePtr Node)
{
	if (!Node.IsValid())
	{
		return;
	}
	const GSTNValidateAll::FAssetResult& Asset = Model->GetAsset(*Node);
	UObject* Object = Asset.AssetData.GetAsset();
	const FGSTN_ValidationFinding* Finding = Model->GetFinding(*Node);
	GSTNValidationNavigation::OpenFinding(Object, Finding ? Finding->Where : FGSTN_FindingLocation());
}

#undef LOCTEXT_NAMESPACE
