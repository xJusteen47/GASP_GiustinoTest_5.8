// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_InlineTimelineBar.h"

#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

FText SGSTN_InlineTimelineBar::MakeBarText(const FGSTN_TimelineEditContext& Context)
{
	if (!Context.IsInline())
	{
		return FText::GetEmpty();
	}
	const FText Name = Context.GetDisplayName();
	if (Context.IsInlineShadowedByAsset())
	{
		return FText::Format(LOCTEXT("InlineShadowed", "Inline timeline of {0}. The ability also names a Timeline Asset, which wins: this inline timeline does not play."), Name);
	}
	return FText::Format(LOCTEXT("InlineBar", "Inline timeline of {0}. Edits go to the Blueprint's class defaults."), Name);
}

void SGSTN_InlineTimelineBar::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor_Lambda([this]() { return Model->GetContext().IsInlineShadowedByAsset() ? FLinearColor(0.35f, 0.26f, 0.05f) : FLinearColor(0.08f, 0.16f, 0.26f); })
		.Padding(FMargin(8.f, 3.f))
		.Visibility_Lambda([this]() { return Model->GetContext().IsInline() ? EVisibility::Visible : EVisibility::Collapsed; })
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return MakeBarText(Model->GetContext()); })
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("ExtractTip", "Creates a timeline asset with this data, points the ability's Timeline Asset at it and clears the inline timeline. One undo step. Closes this editor and opens the asset."))
				.OnClicked_Lambda([Delegate = InArgs._OnExtract]() -> FReply
				{
					Delegate.ExecuteIfBound();
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(LOCTEXT("ExtractButton", "Extract to timeline asset..."))
				]
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
