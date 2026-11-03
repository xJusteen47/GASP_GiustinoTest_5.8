// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Preview/SGSTN_TimelineViewport.h"

#include "Animation/DebugSkelMeshComponent.h"
#include "Preview/GSTN_HitboxGizmoTarget.h"
#include "Preview/GSTN_PreviewController.h"
#include "Preview/GSTN_TimelineViewportClient.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
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
	GizmoTarget = InArgs._GizmoTarget;
	check(Controller.IsValid());
	// A timeline editor's viewport moves the keys of the hitboxes of its model; a viewport with no timeline (the hurtbox set editor) is given its target
	if (!GizmoTarget.IsValid() && Controller->GetModel().IsValid())
	{
		GizmoTarget = MakeShared<FGSTN_HitboxGizmoTarget>(Controller->GetModel().ToSharedRef(), TWeakPtr<FGSTN_TimelinePreviewScene>(Controller->GetScene()));
	}
	SEditorViewport::Construct(SEditorViewport::FArguments());
	bFramedSubject = Controller->GetScene()->GetMeshComponent() != nullptr;
	if (bFramedSubject && TimelineClient.IsValid())
	{
		TimelineClient->FrameSubject();
	}
	PreviewChangedHandle = Controller->OnPreviewChanged.AddSP(this, &SGSTN_TimelineViewport::HandlePreviewChanged);
	// The gizmo and the thick outline follow the selection, which changes no pose and so does not reach OnPreviewChanged
	if (Controller->GetModel().IsValid())
	{
		SelectionChangedHandle = Controller->GetModel()->OnSelectionChanged.AddSP(this, &SGSTN_TimelineViewport::HandlePreviewChanged);
	}
}

SGSTN_TimelineViewport::~SGSTN_TimelineViewport()
{
	if (Controller.IsValid())
	{
		Controller->OnPreviewChanged.Remove(PreviewChangedHandle);
		if (Controller->GetModel().IsValid())
		{
			Controller->GetModel()->OnSelectionChanged.Remove(SelectionChangedHandle);
		}
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
	TimelineClient->SetGizmoTarget(GizmoTarget);
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
				Toggle(LOCTEXT("Hitboxes", "Hitboxes"), LOCTEXT("HitboxesTip", "Draw the hitboxes that are active at the playhead frame, coloured by hit group. Click one to select it"),
					[](const FGSTN_TimelineViewportClient& Target) { return Target.AreHitboxesShown(); }, [](FGSTN_TimelineViewportClient& Target, bool bOn) { Target.SetHitboxesShown(bOn); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				Toggle(LOCTEXT("Lit", "Lit"), LOCTEXT("LitTip", "Lit or unlit"),
					[](const FGSTN_TimelineViewportClient& Target) { return Target.IsLit(); }, [](FGSTN_TimelineViewportClient& Target, bool bOn) { Target.SetLit(bOn); })
			]
		]
	];

	// The gizmo controls (E7): the mode buttons are W, E and R; the rest is for targets that have keys
	{
		auto ModeButton = [Weak](const FText& Label, const FText& Tip, UE::Widget::EWidgetMode Mode) -> TSharedRef<SWidget>
		{
			return SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.ToolTipText(Tip)
				.IsChecked_Lambda([Weak, Mode]()
				{
					const TSharedPtr<FGSTN_TimelineViewportClient> Client = Weak.Pin();
					return Client.IsValid() && Client->GetWidgetMode() == Mode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([Weak, Mode](ECheckBoxState)
				{
					if (const TSharedPtr<FGSTN_TimelineViewportClient> Client = Weak.Pin())
					{
						Client->SetGizmoMode(Mode);
					}
				})
				[
					SNew(SBox).Padding(FMargin(6.f, 2.f)) [ SNew(STextBlock).Text(Label) ]
				];
		};
		auto KeysOnly = [Weak]() -> EVisibility
		{
			const TSharedPtr<FGSTN_TimelineViewportClient> Client = Weak.Pin();
			return Client.IsValid() && Client->GetGizmoTarget().IsValid() && Client->GetGizmoTarget()->HasKeys() ? EVisibility::Visible : EVisibility::Collapsed;
		};
		auto TargetButton = [Weak, KeysOnly](const FText& Label, const FText& Tip, TFunction<void(IGSTN_GizmoTarget&)> Run) -> TSharedRef<SWidget>
		{
			return SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText(Tip)
				.Visibility_Lambda(KeysOnly)
				.OnClicked_Lambda([Weak, Run]() -> FReply
				{
					if (const TSharedPtr<FGSTN_TimelineViewportClient> Client = Weak.Pin(); Client.IsValid() && Client->GetGizmoTarget().IsValid())
					{
						Run(*Client->GetGizmoTarget());
						Client->Invalidate();
					}
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(Label)
				];
		};

		Overlay->AddSlot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		.Padding(FMargin(8.f, 44.f, 8.f, 8.f))
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("FloatingBorder"))
			.Padding(FMargin(4.f, 2.f))
			.Visibility_Lambda([Weak]() { const TSharedPtr<FGSTN_TimelineViewportClient> Client = Weak.Pin(); return Client.IsValid() && Client->GetGizmoTarget().IsValid() ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth() [ ModeButton(LOCTEXT("GizmoMove", "Move (W)"), LOCTEXT("GizmoMoveTip", "Translate the selected hitbox or hurtbox"), UE::Widget::WM_Translate) ]
				+ SHorizontalBox::Slot().AutoWidth() [ ModeButton(LOCTEXT("GizmoRotate", "Rotate (E)"), LOCTEXT("GizmoRotateTip", "Rotate the selection"), UE::Widget::WM_Rotate) ]
				+ SHorizontalBox::Slot().AutoWidth() [ ModeButton(LOCTEXT("GizmoScale", "Scale (R)"), LOCTEXT("GizmoScaleTip", "Change the size of the shape (its extent, never a transform scale). A sphere scales uniformly"), UE::Widget::WM_Scale) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.f, 0.f, 0.f, 0.f))
				[
					Toggle(LOCTEXT("GizmoLocal", "Local"), LOCTEXT("GizmoLocalTip", "The gizmo's axes follow the key's space instead of the world (Ctrl+Tilde). The scale handle is always local"),
						[](const FGSTN_TimelineViewportClient& Target) { return Target.IsGizmoLocal(); }, [](FGSTN_TimelineViewportClient& Target, bool) { Target.ToggleGizmoLocal(); })
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SCheckBox)
					.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
					.ToolTipText(LOCTEXT("AutoKeyTip", "Auto-key (Shift+K). On: dragging the gizmo on a frame with no key adds one first, with the values of the path. Off: the drag edits the nearest key"))
					.Visibility_Lambda(KeysOnly)
					.IsChecked_Lambda([Weak]()
					{
						const TSharedPtr<FGSTN_TimelineViewportClient> Client = Weak.Pin();
						return Client.IsValid() && Client->GetGizmoTarget().IsValid() && Client->GetGizmoTarget()->IsAutoKeyEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
					})
					.OnCheckStateChanged_Lambda([Weak](ECheckBoxState State)
					{
						if (const TSharedPtr<FGSTN_TimelineViewportClient> Client = Weak.Pin(); Client.IsValid() && Client->GetGizmoTarget().IsValid())
						{
							Client->GetGizmoTarget()->SetAutoKeyEnabled(State == ECheckBoxState::Checked);
							Client->Invalidate();
						}
					})
					[
						SNew(SBox).Padding(FMargin(6.f, 2.f)) [ SNew(STextBlock).Text(LOCTEXT("AutoKey", "Auto-key")) ]
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(6.f, 0.f, 0.f, 0.f))
				[
					TargetButton(LOCTEXT("AddKeyHere", "Add key (K)"), LOCTEXT("AddKeyHereTip", "Add a key to the selected hitbox at the playhead frame, with the values its path has there"),
						[](IGSTN_GizmoTarget& Target) { Target.AddKeyAtPlayhead(); })
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					TargetButton(LOCTEXT("MirrorKey", "Mirror"), LOCTEXT("MirrorKeyTip", "Mirror the selected key across the root's YZ plane (x becomes -x), so a right-hand attack gives its left-hand twin"),
						[](IGSTN_GizmoTarget& Target) { Target.MirrorSelectedKey(); })
				]
			]
		];
	}

	// What is shown, and what is wrong with it
	Overlay->AddSlot()
	.VAlign(VAlign_Bottom)
	.HAlign(HAlign_Left)
	.Padding(FMargin(8.f))
	[
		SNew(STextBlock)
		.ShadowOffset(FVector2D(1.f, 1.f))
		.ColorAndOpacity(FLinearColor(0.85f, 0.85f, 0.85f))
		.Text_Lambda([Control, Weak]()
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
			if (const TSharedPtr<FGSTN_TimelineViewportClient> Client = Weak.Pin(); Client.IsValid() && Client->GetGizmoTarget().IsValid())
			{
				const FText Gizmo = Client->GetGizmoTarget()->GetStatusText();
				if (!Gizmo.IsEmpty())
				{
					Result = FText::Format(LOCTEXT("ViewportGizmoLine", "{0}\n{1}"), Result, Gizmo);
				}
			}
			return Result;
		})
	];
}

#undef LOCTEXT_NAMESPACE
