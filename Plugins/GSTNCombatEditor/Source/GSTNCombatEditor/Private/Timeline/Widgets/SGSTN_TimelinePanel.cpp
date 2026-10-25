// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_TimelinePanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleDefaults.h"
#include "Timeline/GSTN_TimelineEditorCommands.h"
#include "Timeline/Widgets/SGSTN_FrameDataSummary.h"
#include "Timeline/Widgets/SGSTN_PhaseBar.h"
#include "Timeline/Widgets/SGSTN_TimelineLengthFields.h"
#include "Timeline/Widgets/SGSTN_TimelineTrackRow.h"
#include "Timeline/GSTN_TimelineTrackEditor.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

void SGSTN_TimelinePanel::Construct(const FArguments& InArgs)
{
	using namespace GSTNTimelineWidgets;

	Model = InArgs._Model;
	Findings = InArgs._Findings;
	check(Model.IsValid());
	Gesture = MakeShared<FGSTN_TimelineGesture>();
	PaintStats = MakeShared<FGSTN_TimelinePaintStats>();

	if (!FGSTN_TimelineEditorCommands::IsRegistered())
	{
		FGSTN_TimelineEditorCommands::Register();
	}
	BindCommands();
	RebuildRows();
	DataChangedHandle = Model->OnDataChanged.AddSP(this, &SGSTN_TimelinePanel::HandleDataChanged);

	const TWeakPtr<SWidget> FocusTarget = StaticCastSharedRef<SWidget>(AsShared());

	RowScrollBar = SNew(SScrollBar)
		.Thickness(FVector2D(ScrollbarWidth, ScrollbarWidth));

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeToolbar(InArgs._ToolbarExtra.Widget)
		]

		// The ruler: a cell over the name column, then the frames, then room for the scrollbar of the rows
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(NameColumnWidth)
				.HeightOverride(RulerHeight)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(HeaderBackground())
					.VAlign(VAlign_Center)
					.Padding(FMargin(8.f, 0.f))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("RulerCorner", "Frame / time"))
						.ColorAndOpacity(FSlateColor(TextColorDim()))
					]
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SAssignNew(Ruler, SGSTN_TimelineRuler)
				.Model(Model)
				.PaintStats(PaintStats)
				.FocusTarget(FocusTarget)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox).WidthOverride(ScrollbarWidth)
			]
		]

		// The phase bar, aligned the same way
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(NameColumnWidth)
				.HeightOverride(PhaseBarHeight)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(HeaderBackground())
					.VAlign(VAlign_Center)
					.Padding(FMargin(8.f, 0.f))
					[
						SNew(STextBlock)
						.Text(LOCTEXT("PhaseBarCorner", "Phases / tail"))
						.ColorAndOpacity(FSlateColor(TextColorDim()))
					]
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SGSTN_PhaseBar)
				.Model(Model)
				.Gesture(Gesture)
				.FocusTarget(FocusTarget)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox).WidthOverride(ScrollbarWidth)
			]
		]

		// The rows
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				// A click no row takes (the empty space under the last track) falls through the list to this border and clears the selection
				SNew(SBorder)
				.BorderImage(FStyleDefaults::GetNoBrush())
				.Padding(FMargin(0.f))
				.OnMouseButtonDown_Lambda([this, FocusTarget](const FGeometry&, const FPointerEvent& MouseEvent) -> FReply
				{
					if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
					{
						return FReply::Unhandled();
					}
					FGSTN_TimelineActions::ClickEmptySpace(*Model, MouseEvent.IsShiftDown());
					return WithPanelFocus(FReply::Handled(), FocusTarget);
				})
				[
					SAssignNew(RowList, SListView<TSharedPtr<FGSTN_TimelineRowData>>)
					.ListItemsSource(&Rows)
					.SelectionMode(ESelectionMode::None)
					.ClearSelectionOnClick(false)
					.OnGenerateRow(this, &SGSTN_TimelinePanel::OnGenerateRow)
					.ExternalScrollbar(RowScrollBar)
					.ScrollbarVisibility(EVisibility::Visible)
					.IsFocusable(false)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(ScrollbarWidth)
				[
					RowScrollBar.ToSharedRef()
				]
			]
		]

		// Frame data and, in the test tab, the paint time
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(8.f, 4.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(SGSTN_FrameDataSummary).Model(Model)
			]
			// TotalFrames and the first actionable frame: a new inline timeline has no length and this is where it is set
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(12.f, 0.f))
			[
				SNew(SGSTN_TimelineLengthFields).Model(Model)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Visibility(InArgs._bShowPaintStats ? EVisibility::Visible : EVisibility::Collapsed)
				.Text_Lambda([this]() { return FText::Format(LOCTEXT("PaintTime", "Paint {0} ms"), FText::AsNumber(PaintStats->LastRepaintMilliseconds)); })
			]
		]
	];

	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SGSTN_TimelinePanel::FitInitialView));
}

SGSTN_TimelinePanel::~SGSTN_TimelinePanel()
{
	if (PlaybackHandle.IsValid())
	{
		FTSTicker::RemoveTicker(PlaybackHandle);
	}
	if (Model.IsValid())
	{
		Model->OnDataChanged.Remove(DataChangedHandle);
	}
}

double SGSTN_TimelinePanel::GetLaneWidth() const
{
	return Ruler.IsValid() ? Ruler->GetLaneWidth() : 0.0;
}

EActiveTimerReturnType SGSTN_TimelinePanel::FitInitialView(double CurrentTime, float DeltaTime)
{
	const double Width = GetLaneWidth();
	if (Width <= 1.0)
	{
		return EActiveTimerReturnType::Continue;
	}
	FGSTN_TimelineLayout::FrameAll(Model->ViewTransform, Width, Model->GetPlayableFrames());
	Model->NotifyViewChanged();
	return EActiveTimerReturnType::Stop;
}

// Rows

void SGSTN_TimelinePanel::RebuildRows()
{
	Rows.Reset();
	const FGSTN_CombatTimelineData* Data = Model->GetData();
	const int32 Count = Data ? Data->Tracks.Num() : 0;
	for (int32 TrackIndex = 0; TrackIndex < Count; ++TrackIndex)
	{
		TSharedPtr<FGSTN_TimelineRowData> Row = MakeShared<FGSTN_TimelineRowData>();
		Row->TrackIndex = TrackIndex;
		Row->TrackStruct = Data->Tracks[TrackIndex].GetScriptStruct();
		Rows.Add(Row);
	}
	if (RowList.IsValid())
	{
		RowList->RequestListRefresh();
	}
}

void SGSTN_TimelinePanel::HandleDataChanged()
{
	// Moves and resizes change only what the lanes paint, and those repaint themselves. The rows are rebuilt when a track appeared,
	// disappeared or was replaced (add, delete, and undo or redo of either)
	const FGSTN_CombatTimelineData* Data = Model->GetData();
	const int32 Count = Data ? Data->Tracks.Num() : 0;
	bool bMatches = Count == Rows.Num();
	for (int32 TrackIndex = 0; bMatches && TrackIndex < Count; ++TrackIndex)
	{
		bMatches = Rows[TrackIndex]->TrackStruct == Data->Tracks[TrackIndex].GetScriptStruct();
	}
	if (!bMatches)
	{
		RebuildRows();
	}
}

TSharedRef<ITableRow> SGSTN_TimelinePanel::OnGenerateRow(TSharedPtr<FGSTN_TimelineRowData> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SGSTN_TimelineTrackRow, OwnerTable)
		.Model(Model)
		.Gesture(Gesture)
		.PaintStats(PaintStats)
		.Findings(Findings)
		.FocusTarget(StaticCastSharedRef<SWidget>(AsShared()))
		.TrackIndex(Item.IsValid() ? Item->TrackIndex : INDEX_NONE);
}

// Toolbar

TSharedRef<SWidget> SGSTN_TimelinePanel::MakeCommandButton(const TSharedPtr<FUICommandInfo>& Command)
{
	return SNew(SButton)
		.ButtonStyle(FAppStyle::Get(), "SimpleButton")
		.ToolTipText_Lambda([Command]() { return FText::Format(LOCTEXT("CommandTip", "{0} ({1})"), Command->GetDescription(), Command->GetInputText()); })
		.IsEnabled_Lambda([this, Command]() { return CommandList->CanExecuteAction(Command.ToSharedRef()); })
		.OnClicked_Lambda([this, Command]() -> FReply
		{
			CommandList->ExecuteAction(Command.ToSharedRef());
			return FReply::Handled();
		})
		[
			SNew(STextBlock).Text(Command->GetLabel())
		];
}

TSharedRef<SWidget> SGSTN_TimelinePanel::MakeAddTrackMenu()
{
	FMenuBuilder Menu(true, nullptr);
	const TWeakPtr<FGSTN_TimelineEditorModel> WeakModel = Model;
	const FGSTN_TrackEditorRegistry& Registry = FGSTN_TrackEditorRegistry::Get();

	auto AddEntry = [&Menu, &WeakModel](const UScriptStruct* Struct, const FText& Name, FName IconName, int32 Length)
	{
		Menu.AddMenuEntry(
			Name,
			FText::Format(LOCTEXT("AddTrackTip", "Add a {0} track at the playhead"), Name),
			IconName.IsNone() ? FSlateIcon() : FSlateIcon(FAppStyle::GetAppStyleSetName(), IconName),
			FUIAction(FExecuteAction::CreateLambda([WeakModel, Struct, Length]()
			{
				if (const TSharedPtr<FGSTN_TimelineEditorModel> Pinned = WeakModel.Pin())
				{
					Pinned->AddTrack(Struct, Pinned->GetPlayhead(), Length);
				}
			})));
	};

	// The registered track editors: each knows its name, its icon and the length of a new window
	TArray<TSharedRef<const IGSTN_TrackEditor>> Editors;
	Registry.GetAll(Editors);
	Menu.BeginSection("GSTNTrackEditors", LOCTEXT("AddTrackKnown", "Tracks"));
	for (const TSharedRef<const IGSTN_TrackEditor>& Editor : Editors)
	{
		AddEntry(Editor->GetTrackStruct(), Editor->GetDisplayName(), Editor->GetIconBrushName(), Editor->GetDefaultLength());
	}
	Menu.EndSection();

	// Track structs nobody wrote an editor for are still tracks: they get the generic row and the details panel
	TArray<const UScriptStruct*> Others;
	Registry.GetUnregisteredTrackStructs(Others);
	if (!Others.IsEmpty())
	{
		Menu.BeginSection("GSTNOtherTracks", LOCTEXT("AddTrackOther", "Other track types"));
		for (const UScriptStruct* Struct : Others)
		{
			const TSharedRef<const IGSTN_TrackEditor> Generic = Registry.FindOrGeneric(Struct);
			AddEntry(Struct, Generic->GetDisplayName(), Generic->GetIconBrushName(), Generic->GetDefaultLength());
		}
		Menu.EndSection();
	}
	return Menu.MakeWidget();
}

TSharedRef<SWidget> SGSTN_TimelinePanel::MakeToolbar(const TSharedRef<SWidget>& ToolbarExtra)
{
	const FGSTN_TimelineEditorCommands& Commands = FGSTN_TimelineEditorCommands::Get();

	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(FMargin(4.f, 2.f))
		[
			SNew(SHorizontalBox)

			// Play / pause: the label follows the state, the command is the same
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.ToolTipText_Lambda([&Commands]() { return FText::Format(LOCTEXT("CommandTip", "{0} ({1})"), Commands.PlayPause->GetDescription(), Commands.PlayPause->GetInputText()); })
				.OnClicked_Lambda([this, &Commands]() -> FReply
				{
					CommandList->ExecuteAction(Commands.PlayPause.ToSharedRef());
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text_Lambda([this]() { return bPlaying ? LOCTEXT("PauseLabel", "Pause") : LOCTEXT("PlayLabel", "Play"); })
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ MakeCommandButton(Commands.StepBack) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ MakeCommandButton(Commands.StepForward) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ MakeCommandButton(Commands.PreviousKey) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ MakeCommandButton(Commands.NextKey) ]

			// The current frame, editable, with its time
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(12.f, 0.f, 4.f, 0.f))
			[
				SNew(STextBlock).Text(LOCTEXT("FrameFieldLabel", "Frame"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(64.f)
				[
					SNew(SSpinBox<int32>)
					.MinValue(0)
					.MaxValue_Lambda([this]() { return FMath::Max(Model->GetPlayableFrames() - 1, 0); })
					.MinSliderValue(0)
					.MaxSliderValue_Lambda([this]() { return FMath::Max(Model->GetPlayableFrames() - 1, 0); })
					.Value_Lambda([this]() { return Model->GetPlayhead(); })
					.OnValueChanged_Lambda([this](int32 NewFrame) { Model->SetPlayhead(NewFrame); })
					.OnValueCommitted_Lambda([this](int32 NewFrame, ETextCommit::Type) { Model->SetPlayhead(NewFrame); })
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(6.f, 0.f))
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return FText::Format(LOCTEXT("FrameMilliseconds", "{0} ms"), FText::FromString(FGSTN_TimelineLayout::FormatMilliseconds(Model->GetPlayhead(), Model->GetTickRate()))); })
			]

			// Editing
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(12.f, 0.f, 0.f, 0.f))
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
				.ToolTipText_Lambda([&Commands]() { return FText::Format(LOCTEXT("CommandTip", "{0} ({1})"), Commands.ToggleAutoKey->GetDescription(), Commands.ToggleAutoKey->GetInputText()); })
				.IsChecked_Lambda([this]() { return Model->IsAutoKeyEnabled() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState) { Model->SetAutoKey(!Model->IsAutoKeyEnabled()); })
				[
					SNew(STextBlock).Text(Commands.ToggleAutoKey->GetLabel()).Margin(FMargin(6.f, 2.f))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ MakeCommandButton(Commands.AddKeyAtPlayhead) ]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.HasDownArrow(true)
				.ToolTipText(LOCTEXT("AddTrackMenuTip", "Add a track at the playhead"))
				.OnGetMenuContent(this, &SGSTN_TimelinePanel::MakeAddTrackMenu)
				.ButtonContent()
				[
					SNew(STextBlock).Text(LOCTEXT("AddTrackMenu", "Add Track"))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ MakeCommandButton(Commands.DeleteSelection) ]

			// View and length
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f, 0.f, 0.f)) [ MakeCommandButton(Commands.FrameAll) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ MakeCommandButton(Commands.FrameSelection) ]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center) [ MakeCommandButton(Commands.FitTotalFramesToMontage) ]

			// What the host adds (the toolkit: loop range, skin and kit, validate)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(FMargin(12.f, 0.f, 0.f, 0.f)) [ ToolbarExtra ]
		];
}

// Commands

void SGSTN_TimelinePanel::BindCommands()
{
	const FGSTN_TimelineEditorCommands& Commands = FGSTN_TimelineEditorCommands::Get();
	FGSTN_TimelineEditorModel& M = *Model;

	// Held-down chords repeat for the ones that walk the playhead
	CommandList->MapAction(Commands.StepBack, FExecuteAction::CreateLambda([&M]() { M.StepPlayhead(-1); }), FCanExecuteAction(), EUIActionRepeatMode::RepeatEnabled);
	CommandList->MapAction(Commands.StepForward, FExecuteAction::CreateLambda([&M]() { M.StepPlayhead(1); }), FCanExecuteAction(), EUIActionRepeatMode::RepeatEnabled);
	CommandList->MapAction(Commands.PreviousKey, FExecuteAction::CreateLambda([&M]() { FGSTN_TimelineActions::JumpToAdjacentEdge(M, -1); }), FCanExecuteAction(), EUIActionRepeatMode::RepeatEnabled);
	CommandList->MapAction(Commands.NextKey, FExecuteAction::CreateLambda([&M]() { FGSTN_TimelineActions::JumpToAdjacentEdge(M, 1); }), FCanExecuteAction(), EUIActionRepeatMode::RepeatEnabled);
	CommandList->MapAction(Commands.PlayPause, FExecuteAction::CreateLambda([this]() { SetPlaying(!bPlaying); }));

	CommandList->MapAction(Commands.AddKeyAtPlayhead,
		FExecuteAction::CreateLambda([&M]() { FGSTN_TimelineActions::AddKeyAtPlayhead(M); }),
		FCanExecuteAction::CreateLambda([&M]() { return FGSTN_TimelineActions::FindKeyedSelectionTrack(M) != INDEX_NONE; }));
	CommandList->MapAction(Commands.ToggleAutoKey,
		FExecuteAction::CreateLambda([&M]() { M.SetAutoKey(!M.IsAutoKeyEnabled()); }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([&M]() { return M.IsAutoKeyEnabled(); }));
	CommandList->MapAction(Commands.DeleteSelection,
		FExecuteAction::CreateLambda([&M]() { FGSTN_TimelineActions::DeleteSelection(M); }),
		FCanExecuteAction::CreateLambda([&M]() { return !M.GetSelection().IsEmpty(); }));
	CommandList->MapAction(Commands.CopyKeys,
		FExecuteAction::CreateLambda([&M]() { FGSTN_TimelineActions::CopySelection(M); }),
		FCanExecuteAction::CreateLambda([&M]() { return FGSTN_TimelineActions::FindKeyedSelectionTrack(M) != INDEX_NONE; }));
	CommandList->MapAction(Commands.PasteKeys,
		FExecuteAction::CreateLambda([&M]() { FGSTN_TimelineActions::PasteAtPlayhead(M); }),
		FCanExecuteAction::CreateLambda([&M]() { return FGSTN_TimelineActions::FindKeyedSelectionTrack(M) != INDEX_NONE; }));
	CommandList->MapAction(Commands.DuplicateKeys,
		FExecuteAction::CreateLambda([&M]() { FGSTN_TimelineActions::DuplicateSelectionAtPlayhead(M); }),
		FCanExecuteAction::CreateLambda([&M]() { return FGSTN_TimelineActions::FindKeyedSelectionTrack(M) != INDEX_NONE; }));

	CommandList->MapAction(Commands.FrameAll, FExecuteAction::CreateLambda([this]()
	{
		FGSTN_TimelineLayout::FrameAll(Model->ViewTransform, GetLaneWidth(), Model->GetPlayableFrames());
		Model->NotifyViewChanged();
	}));
	CommandList->MapAction(Commands.FrameSelection,
		FExecuteAction::CreateLambda([this]()
		{
			int32 First = 0;
			int32 End = 0;
			const FGSTN_CombatTimelineData* Data = Model->GetData();
			if (Data != nullptr && FGSTN_TimelineLayout::GetSelectionSpan(*Data, Model->GetSelection(), First, End))
			{
				FGSTN_TimelineLayout::FrameRange(Model->ViewTransform, GetLaneWidth(), First, End);
			}
			else
			{
				FGSTN_TimelineLayout::FrameAll(Model->ViewTransform, GetLaneWidth(), Model->GetPlayableFrames());
			}
			Model->NotifyViewChanged();
		}));

	CommandList->MapAction(Commands.FitTotalFramesToMontage,
		FExecuteAction::CreateLambda([&M]() { M.FitTotalFramesToMontage(); }),
		FCanExecuteAction::CreateLambda([&M]() { const FGSTN_CombatTimelineData* Data = M.GetData(); return Data != nullptr && !Data->Montage.IsNull(); }));

	// Bake sockets (E8), Validate (E10) and the view toggles (E6) are mapped by the toolkit that implements them: unmapped, their chords do nothing here
}

FReply SGSTN_TimelinePanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape && Gesture->IsActive())
	{
		Gesture->Cancel(*Model);
		FSlateApplication::Get().ReleaseAllPointerCapture();
		return FReply::Handled();
	}
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

// Playback

void SGSTN_TimelinePanel::SetPlaying(bool bInPlaying)
{
	if (bPlaying == bInPlaying)
	{
		return;
	}
	bPlaying = bInPlaying;
	if (bPlaying)
	{
		PlaybackClock.Reset();
		PlaybackHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &SGSTN_TimelinePanel::TickPlayback));
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SGSTN_TimelinePanel::KeepAwake));
	}
	else if (PlaybackHandle.IsValid())
	{
		FTSTicker::RemoveTicker(PlaybackHandle);
		PlaybackHandle.Reset();
	}
}

bool SGSTN_TimelinePanel::TickPlayback(float DeltaSeconds)
{
	// The playable length: the timeline's, or its content's while it has none, so Play moves the playhead on a new inline timeline too
	const int32 Total = Model->GetPlayableFrames();
	if (!bPlaying || Total <= 0)
	{
		bPlaying = false;
		return false;
	}
	const int32 Steps = PlaybackClock.Advance(DeltaSeconds, Model->GetTickRate());
	if (Steps > 0)
	{
		// The loop range when one is set, the whole timeline otherwise
		int32 First = 0;
		int32 End = 0;
		Model->GetEffectivePlayRange(First, End);
		Model->SetPlayhead(FGSTN_PlaybackClock::StepLooped(Model->GetPlayhead(), Steps, First, End));
	}
	return true;
}

EActiveTimerReturnType SGSTN_TimelinePanel::KeepAwake(double CurrentTime, float DeltaTime)
{
	return bPlaying ? EActiveTimerReturnType::Continue : EActiveTimerReturnType::Stop;
}

#undef LOCTEXT_NAMESPACE
