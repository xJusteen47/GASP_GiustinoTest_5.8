// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_TimelineToolbarExtra.h"

#include "Preview/GSTN_PreviewController.h"
#include "Styling/AppStyle.h"
#include "Timeline/Widgets/SGSTN_PreviewSubjectPicker.h"
#include "Timeline/GSTN_TimelineEditorCommands.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

void SGSTN_TimelineToolbarExtra::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());
	const FSimpleDelegate OnValidate = InArgs._OnValidate;
	const FSimpleDelegate OnBake = InArgs._OnBake;
	const TAttribute<EGSTN_BakeStatus> BakeStatus = InArgs._BakeStatus;
	const TAttribute<FText> BakeReason = InArgs._BakeReason;
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
				.MaxValue_Lambda([M]() { return M->GetPlayableFrames(); })
				.MinSliderValue(0)
				.MaxSliderValue_Lambda([M]() { return M->GetPlayableFrames(); })
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

		// Preview subject (E6): a kit and skin, or any skeletal mesh, plus extra meshes. Without a controller (a bare panel) the picker is drawn disabled
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f, 0.f, 0.f))
		[
			InArgs._Preview.IsValid()
				? StaticCastSharedRef<SWidget>(SNew(SGSTN_PreviewSubjectPicker).Controller(InArgs._Preview))
				: StaticCastSharedRef<SWidget>(SNew(SComboButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.IsEnabled(false)
					.ToolTipText(LOCTEXT("SubjectUnavailableTip", "The preview subject. Available when the timeline is opened in its editor."))
					.ButtonContent()
					[
						SNew(STextBlock).Text(LOCTEXT("SubjectUnavailable", "Preview"))
					])
		]

		// Bake sockets (E8): a yellow dot when the frames are missing or no longer match their sources, the reason in the tooltip
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.IsEnabled(OnBake.IsBound())
			.ToolTipText_Lambda([BakeReason]()
			{
				const FText Reason = BakeReason.IsSet() ? BakeReason.Get() : FText::GetEmpty();
				return FText::Format(LOCTEXT("BakeTip", "Samples the montage at every frame and stores where each socket used by a baked-socket hitbox is, so the simulation can read it. Shortcut: {0}\n\n{1}"),
					FGSTN_TimelineEditorCommands::Get().BakeSockets->GetInputText(), Reason);
			})
			.OnClicked_Lambda([OnBake]() -> FReply
			{
				OnBake.ExecuteIfBound();
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Commands.BakeSockets->GetLabel())
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(4.f, 0.f, 0.f, 0.f))
				[
					SNew(SBox)
					.WidthOverride(8.f)
					.HeightOverride(8.f)
					.Visibility_Lambda([BakeStatus]()
					{
						// Hidden, not collapsed: the button must not change width when the dot comes and goes
						const EGSTN_BakeStatus Status = BakeStatus.IsSet() ? BakeStatus.Get() : EGSTN_BakeStatus::Unknown;
						return (Status == EGSTN_BakeStatus::NeverBaked || Status == EGSTN_BakeStatus::Stale || Status == EGSTN_BakeStatus::BakedElsewhere) ? EVisibility::Visible : EVisibility::Hidden;
					})
					[
						SNew(SBorder)
						.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor(FLinearColor(0.95f, 0.8f, 0.1f))
					]
				]
			]
		]

		// Validate (E10): the findings go to the Message Log, category GSTN Combat
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.IsEnabled(OnValidate.IsBound())
			.ToolTipText(LOCTEXT("ValidateTip", "Checks the timeline against the rules and lists the findings in the Message Log. Errors make the asset invalid, warnings do not."))
			.OnClicked_Lambda([OnValidate]() -> FReply
			{
				OnValidate.ExecuteIfBound();
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(Commands.ValidateNow->GetLabel())
			]
		]
	];
}

#undef LOCTEXT_NAMESPACE
