// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_MontageLengthBar.h"

#include "Animation/AnimMontage.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

void SGSTN_MontageLengthBar::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.35f, 0.26f, 0.05f))
		.Padding(FMargin(8.f, 3.f))
		.Visibility_Lambda([this]() { return IsMismatched() ? EVisibility::Visible : EVisibility::Collapsed; })
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					return FText::Format(LOCTEXT("MontageLengthMismatch", "The montage is {0} frames long at {1} Hz, the timeline {2}."),
						FText::AsNumber(MontageFrames), FText::AsNumber(Model->GetTickRate()), FText::AsNumber(Model->GetTotalFrames()));
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("FitTotalFramesTip", "Sets the length of the timeline to the length of the montage section. One undo step; never done automatically."))
				.OnClicked_Lambda([this]() -> FReply
				{
					Model->FitTotalFramesToMontage();
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(LOCTEXT("FitTotalFramesButton", "Fit Total Frames to montage"))
				]
			]
		]
	];

	DataHandle = Model->OnDataChanged.AddSP(this, &SGSTN_MontageLengthBar::Recompute);
	Recompute();
}

SGSTN_MontageLengthBar::~SGSTN_MontageLengthBar()
{
	if (Model.IsValid())
	{
		Model->OnDataChanged.Remove(DataHandle);
	}
}

bool SGSTN_MontageLengthBar::IsMismatched() const
{
	return MontageFrames != INDEX_NONE && MontageFrames != Model->GetTotalFrames();
}

void SGSTN_MontageLengthBar::Recompute()
{
	const FGSTN_CombatTimelineData* Data = Model->GetData();
	if (Data == nullptr || Data->Montage.IsNull())
	{
		CachedPath.Reset();
		CachedMontage.Reset();
		MontageFrames = INDEX_NONE;
		return;
	}

	// Loaded once per path: an editor that opens a timeline has asked for what the timeline refers to, and a failed load is not retried on every edit
	if (CachedPath != Data->Montage.ToSoftObjectPath())
	{
		CachedPath = Data->Montage.ToSoftObjectPath();
		CachedMontage = Data->Montage.LoadSynchronous();
	}
	MontageFrames = FGSTN_TimelineEditorModel::ComputeMontageFrames(*Data, CachedMontage.Get(), Model->GetTickRate());
}

#undef LOCTEXT_NAMESPACE
