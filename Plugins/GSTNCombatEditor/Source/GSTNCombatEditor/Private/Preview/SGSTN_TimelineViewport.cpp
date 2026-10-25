// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Preview/SGSTN_TimelineViewport.h"

#include "Animation/DebugSkelMeshComponent.h"
#include "Preview/GSTN_PreviewController.h"
#include "Preview/GSTN_TimelineViewportClient.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

void SGSTN_TimelineViewport::Construct(const FArguments& InArgs)
{
	Controller = InArgs._Controller;
	DrawExtra = InArgs._OnDrawExtra;
	check(Controller.IsValid());
	SEditorViewport::Construct(SEditorViewport::FArguments());
	bFramedSubject = Controller->GetScene()->GetMeshComponent() != nullptr;
	if (bFramedSubject && TimelineClient.IsValid())
	{
		TimelineClient->FrameSubject();
	}
	PreviewChangedHandle = Controller->OnPreviewChanged.AddSP(this, &SGSTN_TimelineViewport::HandlePreviewChanged);
}

SGSTN_TimelineViewport::~SGSTN_TimelineViewport()
{
	if (Controller.IsValid())
	{
		Controller->OnPreviewChanged.Remove(PreviewChangedHandle);
	}
	if (TimelineClient.IsValid())
	{
		TimelineClient->Viewport = nullptr;
	}
}

TSharedRef<FEditorViewportClient> SGSTN_TimelineViewport::MakeEditorViewportClient()
{
	TimelineClient = MakeShared<FGSTN_TimelineViewportClient>(Controller->GetScene(), SharedThis(this), Controller->GetModel());
	TimelineClient->SetExtraDraw(DrawExtra);
	return TimelineClient.ToSharedRef();
}

void SGSTN_TimelineViewport::HandlePreviewChanged()
{
	if (!TimelineClient.IsValid())
	{
		return;
	}
	// A subject that just got its mesh is framed once; after that the designer's camera is left alone
	const bool bHasMesh = Controller->GetScene()->GetMeshComponent() != nullptr;
	if (bHasMesh != bFramedSubject)
	{
		bFramedSubject = bHasMesh;
		TimelineClient->FrameSubject();
	}
	TimelineClient->Invalidate();
}

void SGSTN_TimelineViewport::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	const TWeakPtr<FGSTN_TimelineViewportClient> Weak = TimelineClient;
	const TSharedPtr<FGSTN_PreviewController> Control = Controller;

	auto PresetButton = [Weak](const FText& Label, const FText& Tip, EGSTN_PreviewCameraPreset Preset) -> TSharedRef<SWidget>
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(Tip)
			.OnClicked_Lambda([Weak, Preset]() -> FReply
			{
				if (const TSharedPtr<FGSTN_TimelineViewportClient> Client = Weak.Pin())
				{
					Client->SetCameraPreset(Preset);
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(Label)
			];
	};
	auto Toggle = [Weak](const FText& Label, const FText& Tip, TFunction<bool(const FGSTN_TimelineViewportClient&)> Get, TFunction<void(FGSTN_TimelineViewportClient&, bool)> Set) -> TSharedRef<SWidget>
	{
		return SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.ToolTipText(Tip)
			.IsChecked_Lambda([Weak, Get]()
			{
				const TSharedPtr<FGSTN_TimelineViewportClient> Client = Weak.Pin();
				return Client.IsValid() && Get(*Client) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([Weak, Set](ECheckBoxState State)
			{
				if (const TSharedPtr<FGSTN_TimelineViewportClient> Client = Weak.Pin())
				{
					Set(*Client, State == ECheckBoxState::Checked);
				}
			})
			[
				SNew(SBox).Padding(FMargin(6.f, 2.f)) [ SNew(STextBlock).Text(Label) ]
			];
	};

	Overlay->AddSlot()
	.VAlign(VAlign_Top)
	.HAlign(HAlign_Left)
	.Padding(FMargin(8.f))
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("FloatingBorder"))
		.Padding(FMargin(4.f, 2.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth() [ PresetButton(LOCTEXT("CameraSide", "Side"), LOCTEXT("CameraSideTip", "Look at the character from its side"), EGSTN_PreviewCameraPreset::Side) ]
			+ SHorizontalBox::Slot().AutoWidth() [ PresetButton(LOCTEXT("CameraFront", "Front"), LOCTEXT("CameraFrontTip", "Look at the character from the front"), EGSTN_PreviewCameraPreset::Front) ]
			+ SHorizontalBox::Slot().AutoWidth() [ PresetButton(LOCTEXT("CameraThreeQuarter", "3/4"), LOCTEXT("CameraThreeQuarterTip", "Look from the front, the right and above"), EGSTN_PreviewCameraPreset::ThreeQuarter) ]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.f, 0.f, 0.f, 0.f))
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(LOCTEXT("FrameTip", "Frame the whole subject, the extra meshes included"))
				.OnClicked_Lambda([Weak]() -> FReply
				{
					if (const TSharedPtr<FGSTN_TimelineViewportClient> Client = Weak.Pin())
					{
						Client->FrameSubject();
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(LOCTEXT("Frame", "Frame"))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.f, 0.f, 0.f, 0.f))
			[
				Toggle(LOCTEXT("FollowRoot", "Follow root"), LOCTEXT("FollowRootTip", "The camera moves with the root of the character"),
					[](const FGSTN_TimelineViewportClient& Target) { return Target.IsFollowRoot(); }, [](FGSTN_TimelineViewportClient& Target, bool bOn) { Target.SetFollowRoot(bOn); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Toggle(LOCTEXT("Grid", "Grid"), LOCTEXT("GridTip", "The ground grid, 50 cm cells"),
					[](const FGSTN_TimelineViewportClient& Target) { return Target.IsGridShown(); }, [](FGSTN_TimelineViewportClient& Target, bool bOn) { Target.SetGridShown(bOn); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Toggle(LOCTEXT("Root", "Root"), LOCTEXT("RootTip", "The root capsule and the facing arrow"),
					[](const FGSTN_TimelineViewportClient& Target) { return Target.IsRootShown(); }, [](FGSTN_TimelineViewportClient& Target, bool bOn) { Target.SetRootShown(bOn); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Toggle(LOCTEXT("Lit", "Lit"), LOCTEXT("LitTip", "Lit or unlit"),
					[](const FGSTN_TimelineViewportClient& Target) { return Target.IsLit(); }, [](FGSTN_TimelineViewportClient& Target, bool bOn) { Target.SetLit(bOn); })
			]
		]
	];

	// What is shown, and what is wrong with it
	Overlay->AddSlot()
	.VAlign(VAlign_Bottom)
	.HAlign(HAlign_Left)
	.Padding(FMargin(8.f))
	[
		SNew(STextBlock)
		.ShadowOffset(FVector2D(1.f, 1.f))
		.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f))
		.Text_Lambda([Control]()
		{
			if (!Control.IsValid())
			{
				return FText::GetEmpty();
			}
			FText Result = Control->GetScene()->GetMeshComponent() != nullptr
				? FText::Format(LOCTEXT("ViewportSubject", "Preview: {0}"), Control->GetSubjectLabel())
				: LOCTEXT("ViewportNoSubject", "No preview subject. Pick a kit and skin, or a skeletal mesh, in the toolbar.");
			for (const FText& Problem : Control->GetProblems())
			{
				Result = FText::Format(LOCTEXT("ViewportProblemLine", "{0}\n{1}"), Result, Problem);
			}
			return Result;
		})
	];
}

#undef LOCTEXT_NAMESPACE
