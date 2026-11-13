// Pre-mission loadout screen: the Slate widget.

#include "SGSTN_LoadoutMenu.h"

#include "Loadout/GSTN_LoadoutDraft.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNLoadoutMenu"

namespace
{
	const FLinearColor TextLight(1.f, 1.f, 1.f);
	const FLinearColor TextDark(0.08f, 0.05f, 0.f);
	const FLinearColor TextRefused(1.f, 0.35f, 0.3f);
}

void SGSTN_LoadoutMenu::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(640.f)
		[
			SNew(SBorder)
			.BorderImage(&PanelBrush)
			.Padding(FMargin(28.f, 24.f))
			[
				SAssignNew(Rows, SVerticalBox)
			]
		]
	];
}

void SGSTN_LoadoutMenu::Refresh(const FGSTN_LoadoutDraft& Draft, const FText& Hint, const FString& Feedback)
{
	if (!Rows.IsValid())
	{
		return;
	}
	Rows->ClearChildren();

	Rows->AddSlot()
	.AutoHeight()
	.Padding(0.f, 0.f, 0.f, 16.f)
	.HAlign(HAlign_Center)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Title", "SCEGLI IL TUO KIT"))
		.Font(FCoreStyle::GetDefaultFontStyle("Bold", 30))
		.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.8f, 0.2f)))
	];

	const TArray<FGSTN_LoadoutChoiceRow>& Choices = Draft.GetRows();
	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		const FGSTN_LoadoutChoiceRow& Row = Choices[Index];
		const bool bCursor = Index == Draft.GetCursor();
		const FGSTN_LoadoutChoiceOption* Current = Row.Options.IsValidIndex(Row.Selected) ? &Row.Options[Row.Selected] : nullptr;
		const FText Value = Current ? Current->Label : LOCTEXT("Custom", "(personalizzato)");
		const bool bArrows = Row.Options.Num() > 1;
		const FLinearColor TextColor = bCursor ? TextDark : TextLight;

		Rows->AddSlot()
		.AutoHeight()
		.Padding(0.f, 4.f)
		[
			SNew(SBorder)
			.BorderImage(bCursor ? &CursorBrush : &CardBrush)
			.Padding(FMargin(18.f, 10.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Row.Label)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
					.ColorAndOpacity(FSlateColor(TextColor))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(bArrows ? FText::Format(LOCTEXT("ValueArrows", "<  {0}  >"), Value) : Value)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
					.ColorAndOpacity(FSlateColor(Current && !Current->bValid ? TextRefused : TextColor))
				]
			]
		];
	}

	if (!Feedback.IsEmpty() || !Draft.IsValid())
	{
		Rows->AddSlot()
		.AutoHeight()
		.Padding(0.f, 12.f, 0.f, 0.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(!Feedback.IsEmpty() ? Feedback : Draft.GetProblem()))
			.AutoWrapText(true)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
			.ColorAndOpacity(FSlateColor(TextRefused))
		];
	}

	Rows->AddSlot()
	.AutoHeight()
	.Padding(0.f, 16.f, 0.f, 0.f)
	.HAlign(HAlign_Center)
	[
		SNew(STextBlock)
		.Text(Hint)
		.AutoWrapText(true)
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 16))
		.ColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.85f, 1.f)))
	];
}

#undef LOCTEXT_NAMESPACE
