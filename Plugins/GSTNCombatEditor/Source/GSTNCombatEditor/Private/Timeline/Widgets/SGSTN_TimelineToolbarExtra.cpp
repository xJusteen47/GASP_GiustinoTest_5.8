// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_TimelineToolbarExtra.h"

#include "Styling/AppStyle.h"
#include "Timeline/GSTN_TimelineEditorCommands.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	/** A combo that is drawn and does nothing yet */
	TSharedRef<SWidget> MakeStubCombo(const FText& Label, const FText& Tip)
	{
		return SNew(SComboButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.IsEnabled(false)
			.ToolTipText(Tip)
			.ButtonContent()
			[
				SNew(STextBlock).Text(Label)
			];
	}
}

void SGSTN_TimelineToolbarExtra::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());
	const TSharedRef<FGSTN_TimelineEditorModel> M = Model.ToSharedRef();
	const FGSTN_TimelineEditorCommands& Commands = FGSTN_TimelineEditorCommands::Get();

	// The spin boxes show the range playback loops over: the loop range, or the whole timeline when there is none
	auto RangeBox = [M](bool bFrom) -> TSharedRef<SWidget>
	{
		return SNew(SBox)
			.WidthOverride(58.f)
			[
				SNew(SSpinBox<int32>)
				.MinValue(0)
				.MaxValue_Lambda([M]() { return FMath::Max(M->GetTotalFrames(), 0); })
				.MinSliderValue(0)
				.MaxSliderValue_Lambda([M]() { return FMath::Max(M->GetTotalFrames(), 0); })
				.Value_Lambda([M, bFrom]()
				{
					int32 First = 0;
					int32 End = 0;
					M->GetEffectivePlayRange(First, End);
					return bFrom ? First : End;
				})
				.OnValueCommitted_Lambda([M, bFrom](int32 NewValue, ETextCommit::Type)
				{
					int32 First = 0;
					int32 End = 0;
					M->GetEffectivePlayRange(First, End);
					if (bFrom)
					{
						M->SetPlayRange(NewValue, End);
					}
					else
					{
						M->SetPlayRange(First, NewValue);
					}
				})
			];
	};

	ChildSlot
	[
		SNew(SHorizontalBox)

		// Loop range: from, to (exclusive), and back to the whole timeline
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 4.f, 0.f))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LoopRangeLabel", "Loop"))
			.ToolTipText(LOCTEXT("LoopRangeTip", "The frames playback loops over: from the first frame up to, not including, the last. Playback is not undoable and not saved."))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ RangeBox(true) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(4.f, 0.f)) [ SNew(STextBlock).Text(LOCTEXT("LoopRangeTo", "to")) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ RangeBox(false) ]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(LOCTEXT("LoopRangeClearTip", "Loop the whole timeline"))
			.IsEnabled_Lambda([M]() { return M->HasPlayRange(); })
			.OnClicked_Lambda([M]() -> FReply
			{
				M->ClearPlayRange();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(LOCTEXT("LoopRangeClear", "All"))
			]
		]

		// Preview skin and kit (E6)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f, 0.f, 0.f))
		[
			MakeStubCombo(LOCTEXT("SkinCombo", "Skin"), LOCTEXT("SkinComboTip", "The skin shown in the preview. Available with the preview scene."))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			MakeStubCombo(LOCTEXT("KitCombo", "Kit"), LOCTEXT("KitComboTip", "The kit whose hurtboxes and skins the preview uses. Available with the preview scene."))
		]

		// Validate (E10)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.IsEnabled(false)
			.ToolTipText(LOCTEXT("ValidateTip", "Checks the timeline for errors. Available with the validators."))
			[
				SNew(STextBlock).Text(Commands.ValidateNow->GetLabel())
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
