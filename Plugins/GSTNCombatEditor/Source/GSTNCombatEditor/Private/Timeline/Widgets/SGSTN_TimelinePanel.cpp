// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/Widgets/SGSTN_TimelinePanel.h"

#include "CoreGlobals.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
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

	// Under the lanes, like the view range bar of Sequencer: drag it to scroll, the thumb is the visible part of the timeline
	HorizontalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal)
		.AlwaysShowScrollbar(true)
		.Thickness(FVector2D(ScrollbarWidth, ScrollbarWidth))
		.OnUserScrolled(this, &SGSTN_TimelinePanel::HandleHorizontalScroll);

	// The ruler display the user chose last time (a per-user view preference)
	int32 SavedDisplay = static_cast<int32>(EGSTN_RulerDisplay::FramesAndSeconds);
	if (GConfig != nullptr && GConfig->GetInt(TEXT("GSTNTimelineEditor"), TEXT("RulerDisplay"), SavedDisplay, GEditorPerProjectIni))
	{
		Model->RulerDisplay = static_cast<EGSTN_RulerDisplay>(FMath::Clamp(SavedDisplay, 0, static_cast<int32>(EGSTN_RulerDisplay::Seconds)));
	}

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

		// The horizontal scrollbar, under the lanes only
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox).WidthOverride(NameColumnWidth)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				HorizontalScrollBar.ToSharedRef()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBox).WidthOverride(ScrollbarWidth)
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

namespace
{
	/** Where the ruler display mode is kept per user (EditorPerProjectUserSettings): a view preference, never project data */
	const TCHAR* GSTNTimelineConfigSection = TEXT("GSTNTimelineEditor");
	const TCHAR* GSTNRulerDisplayKey = TEXT("RulerDisplay");

	/** Each toolbar icon: the native brushes it may have in this engine version, best first (Starship names moved between 5.x releases) */
	const TMap<FName, TArray<FName>>& IconCandidates()
	{
		static const TMap<FName, TArray<FName>> Candidates = {
			{ TEXT("JumpToStart"), { TEXT("Animation.Backward_End"), TEXT("Icons.Rewind") } },
			{ TEXT("StepBack"), { TEXT("Animation.Backward_Step"), TEXT("Icons.ChevronLeft") } },
			{ TEXT("Play"), { TEXT("Animation.Forward"), TEXT("Icons.Play") } },
			{ TEXT("Pause"), { TEXT("Animation.Pause"), TEXT("Icons.Pause") } },
			{ TEXT("StepForward"), { TEXT("Animation.Forward_Step"), TEXT("Icons.ChevronRight") } },
			{ TEXT("JumpToEnd"), { TEXT("Animation.Forward_End"), TEXT("Icons.FastForward") } },
			{ TEXT("PreviousKey"), { TEXT("Icons.ArrowLeft"), TEXT("Icons.ChevronLeft"), TEXT("Icons.CircleArrowLeft") } },
			{ TEXT("NextKey"), { TEXT("Icons.ArrowRight"), TEXT("Icons.ChevronRight"), TEXT("Icons.CircleArrowRight") } },
			{ TEXT("AutoKey"), { TEXT("Icons.Lock"), TEXT("Icons.Edit") } },
			{ TEXT("Add"), { TEXT("Icons.Plus") } },
			{ TEXT("Delete"), { TEXT("Icons.Delete") } },
			{ TEXT("ZoomIn"), { TEXT("Icons.ZoomIn"), TEXT("Icons.Plus") } },
			{ TEXT("ZoomOut"), { TEXT("Icons.ZoomOut"), TEXT("Icons.Minus") } },
			{ TEXT("FrameAll"), { TEXT("Icons.Fullscreen"), TEXT("Icons.Expand"), TEXT("Icons.Search") } },
			{ TEXT("FrameSelection"), { TEXT("Icons.FrameSelection"), TEXT("Icons.Search") } },
			{ TEXT("View"), { TEXT("Icons.Visible"), TEXT("Icons.Settings") } },
			{ TEXT("FitLength"), { TEXT("Icons.Refresh") } } };
		return Candidates;
	}
}

FName SGSTN_TimelinePanel::ResolveToolbarIcon(FName Role, bool* bOutFound)
{
	const TArray<FName>* Names = IconCandidates().Find(Role);
	if (bOutFound != nullptr)
	{
		*bOutFound = false;
	}
	if (Names == nullptr || Names->IsEmpty())
	{
		return NAME_None;
	}
	for (const FName Name : *Names)
	{
		if (FAppStyle::Get().GetOptionalBrush(Name, nullptr, nullptr) != nullptr)
		{
			if (bOutFound != nullptr)
			{
				*bOutFound = true;
			}
			return Name;
		}
	}
	return Names->Last();
}

TArray<FName> SGSTN_TimelinePanel::GetToolbarIconRoles()
{
	TArray<FName> Roles;
	IconCandidates().GetKeys(Roles);
	return Roles;
}

namespace
{
	/** A native editor icon (the Starship style of FAppStyle) for a toolbar role, so the toolbar reads like the engine's own editors */
	FSlateIcon AppIcon(const TCHAR* Role)
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), SGSTN_TimelinePanel::ResolveToolbarIcon(Role));
	}
}

void SGSTN_TimelinePanel::SetRulerDisplay(EGSTN_RulerDisplay Display)
{
	Model->RulerDisplay = Display;
	if (GConfig != nullptr)
	{
		GConfig->SetInt(GSTNTimelineConfigSection, GSTNRulerDisplayKey, static_cast<int32>(Display), GEditorPerProjectIni);
	}
	Model->NotifyViewChanged();
}

TSharedRef<SWidget> SGSTN_TimelinePanel::MakeViewMenu()
{
	const FGSTN_TimelineEditorCommands& Commands = FGSTN_TimelineEditorCommands::Get();
	FMenuBuilder Menu(true, CommandList);

	Menu.BeginSection("GSTNTimelineZoom", LOCTEXT("ViewMenuZoom", "Zoom"));
	Menu.AddMenuEntry(Commands.ZoomIn, NAME_None, TAttribute<FText>(), TAttribute<FText>(), AppIcon(TEXT("ZoomIn")));
	Menu.AddMenuEntry(Commands.ZoomOut, NAME_None, TAttribute<FText>(), TAttribute<FText>(), AppIcon(TEXT("ZoomOut")));
	Menu.AddMenuEntry(Commands.FrameAll, NAME_None, TAttribute<FText>(), TAttribute<FText>(), AppIcon(TEXT("FrameAll")));
	Menu.AddMenuEntry(Commands.FrameSelection, NAME_None, TAttribute<FText>(), TAttribute<FText>(), AppIcon(TEXT("FrameSelection")));
	Menu.EndSection();

	Menu.BeginSection("GSTNTimelineRuler", LOCTEXT("ViewMenuRuler", "Ruler shows"));
	auto AddMode = [this, &Menu](EGSTN_RulerDisplay Mode, const FText& Label, const FText& Tip)
	{
		Menu.AddMenuEntry(Label, Tip, FSlateIcon(), FUIAction(
			FExecuteAction::CreateSP(this, &SGSTN_TimelinePanel::SetRulerDisplay, Mode),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([this, Mode]() { return Model->RulerDisplay == Mode; })),
			NAME_None, EUserInterfaceActionType::RadioButton);
	};
	AddMode(EGSTN_RulerDisplay::FramesAndSeconds, LOCTEXT("RulerFramesSeconds", "Frames and Seconds"), LOCTEXT("RulerFramesSecondsTip", "Frame numbers, with the time in seconds under them (like the Montage editor)"));
	AddMode(EGSTN_RulerDisplay::FramesAndMilliseconds, LOCTEXT("RulerFramesMs", "Frames and Milliseconds"), LOCTEXT("RulerFramesMsTip", "Frame numbers, with the time in milliseconds under them"));
	AddMode(EGSTN_RulerDisplay::Frames, LOCTEXT("RulerFrames", "Frames"), LOCTEXT("RulerFramesTip", "Frame numbers only"));
	AddMode(EGSTN_RulerDisplay::Seconds, LOCTEXT("RulerSeconds", "Seconds"), LOCTEXT("RulerSecondsTip", "Time in seconds only"));
	Menu.EndSection();

	Menu.BeginSection("GSTNTimelineLength", LOCTEXT("ViewMenuLength", "Timeline"));
	Menu.AddMenuEntry(Commands.FitTotalFramesToMontage, NAME_None, TAttribute<FText>(), TAttribute<FText>(), AppIcon(TEXT("FitLength")));
	Menu.EndSection();
	return Menu.MakeWidget();
}

TSharedRef<SWidget> SGSTN_TimelinePanel::MakeToolbar(const TSharedRef<SWidget>& ToolbarExtra)
{
	const FGSTN_TimelineEditorCommands& Commands = FGSTN_TimelineEditorCommands::Get();

	// The engine's slim toolbar (the one of the asset editors), in sections: transport, the current frame, keys, view. Every button is a command, so its
	// tooltip names the chord the user bound to it
	FSlimHorizontalToolBarBuilder Toolbar(CommandList, FMultiBoxCustomization::None);
	Toolbar.SetStyle(&FAppStyle::Get(), "AssetEditorToolbar");

	Toolbar.BeginSection("GSTNTransport");
	{
		Toolbar.AddToolBarButton(Commands.JumpToStart, NAME_None, FText::GetEmpty(), TAttribute<FText>(), AppIcon(TEXT("JumpToStart")));
		Toolbar.AddToolBarButton(Commands.PreviousKey, NAME_None, FText::GetEmpty(), TAttribute<FText>(), AppIcon(TEXT("PreviousKey")));
		Toolbar.AddToolBarButton(Commands.StepBack, NAME_None, FText::GetEmpty(), TAttribute<FText>(), AppIcon(TEXT("StepBack")));
		// Play and pause are one command; the icon follows the state, as in the animation editors
		Toolbar.AddToolBarButton(Commands.PlayPause, NAME_None, FText::GetEmpty(),
			TAttribute<FText>::CreateLambda([this]() { return bPlaying ? LOCTEXT("PauseTip", "Pause (Space)") : LOCTEXT("PlayTip", "Play (Space)"); }),
			TAttribute<FSlateIcon>::CreateLambda([this]() { return AppIcon(bPlaying ? TEXT("Pause") : TEXT("Play")); }));
		Toolbar.AddToolBarButton(Commands.StepForward, NAME_None, FText::GetEmpty(), TAttribute<FText>(), AppIcon(TEXT("StepForward")));
		Toolbar.AddToolBarButton(Commands.NextKey, NAME_None, FText::GetEmpty(), TAttribute<FText>(), AppIcon(TEXT("NextKey")));
		Toolbar.AddToolBarButton(Commands.JumpToEnd, NAME_None, FText::GetEmpty(), TAttribute<FText>(), AppIcon(TEXT("JumpToEnd")));
	}
	Toolbar.EndSection();

	// The current frame, editable, with its time in the ruler's unit
	Toolbar.BeginSection("GSTNFrame");
	Toolbar.AddWidget(
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(6.f, 0.f, 4.f, 0.f))
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
			SNew(SBox)
			.MinDesiredWidth(56.f)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text_Lambda([this]() { return FText::FromString(FGSTN_TimelineLayout::FormatPlayheadTime(Model->GetPlayhead(), Model->GetTickRate(), Model->RulerDisplay)); })
			]
		]);
	Toolbar.EndSection();

	Toolbar.BeginSection("GSTNKeys");
	{
		// Auto-key: the red-dot toggle of Sequencer is a Sequencer style; the lock of the app style reads as "keys follow the gizmo"
		Toolbar.AddToolBarButton(Commands.ToggleAutoKey, NAME_None, Commands.ToggleAutoKey->GetLabel(), TAttribute<FText>(), AppIcon(TEXT("AutoKey")));
		Toolbar.AddToolBarButton(Commands.AddKeyAtPlayhead, NAME_None, Commands.AddKeyAtPlayhead->GetLabel(), TAttribute<FText>(), AppIcon(TEXT("Add")));
		Toolbar.AddComboButton(FUIAction(), FOnGetContent::CreateSP(this, &SGSTN_TimelinePanel::MakeAddTrackMenu),
			LOCTEXT("AddTrackMenu", "Track"), LOCTEXT("AddTrackMenuTip", "Add a track at the playhead"), AppIcon(TEXT("Add")));
		Toolbar.AddToolBarButton(Commands.DeleteSelection, NAME_None, FText::GetEmpty(), TAttribute<FText>(), AppIcon(TEXT("Delete")));
	}
	Toolbar.EndSection();

	Toolbar.BeginSection("GSTNView");
	{
		Toolbar.AddToolBarButton(Commands.ZoomOut, NAME_None, FText::GetEmpty(), TAttribute<FText>(), AppIcon(TEXT("ZoomOut")));
		Toolbar.AddToolBarButton(Commands.ZoomIn, NAME_None, FText::GetEmpty(), TAttribute<FText>(), AppIcon(TEXT("ZoomIn")));
		Toolbar.AddToolBarButton(Commands.FrameAll, NAME_None, FText::GetEmpty(), TAttribute<FText>(), AppIcon(TEXT("FrameAll")));
		Toolbar.AddComboButton(FUIAction(), FOnGetContent::CreateSP(this, &SGSTN_TimelinePanel::MakeViewMenu),
			LOCTEXT("ViewMenu", "View"), LOCTEXT("ViewMenuTip", "Zoom, what the ruler shows (frames, seconds), timeline length"), AppIcon(TEXT("View")));
	}
	Toolbar.EndSection();

	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(FMargin(2.f, 0.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				Toolbar.MakeWidget()
			]
			// What the host adds (the toolkit: loop range, skin and kit, bake, validate)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(FMargin(8.f, 0.f, 0.f, 0.f))
			[
				ToolbarExtra
			]
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
	CommandList->MapAction(Commands.JumpToStart, FExecuteAction::CreateLambda([&M]()
	{
		int32 First = 0;
		int32 End = 0;
		M.GetEffectivePlayRange(First, End);
		M.SetPlayhead(First);
	}));
	CommandList->MapAction(Commands.JumpToEnd, FExecuteAction::CreateLambda([&M]()
	{
		int32 First = 0;
		int32 End = 0;
		M.GetEffectivePlayRange(First, End);
		M.SetPlayhead(FMath::Max(End - 1, First));
	}));
	CommandList->MapAction(Commands.ZoomIn, FExecuteAction::CreateLambda([this]()
	{
		FGSTN_TimelineLayout::ZoomAroundCenter(Model->ViewTransform, 1.5, GetLaneWidth(), Model->GetPlayableFrames());
		Model->NotifyViewChanged();
	}), FCanExecuteAction(), EUIActionRepeatMode::RepeatEnabled);
	CommandList->MapAction(Commands.ZoomOut, FExecuteAction::CreateLambda([this]()
	{
		FGSTN_TimelineLayout::ZoomAroundCenter(Model->ViewTransform, 1.0 / 1.5, GetLaneWidth(), Model->GetPlayableFrames());
		Model->NotifyViewChanged();
	}), FCanExecuteAction(), EUIActionRepeatMode::RepeatEnabled);

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

// View: pan and scrollbar

void SGSTN_TimelinePanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	const double Width = GetLaneWidth();
	if (!HorizontalScrollBar.IsValid() || Width <= 1.0)
	{
		return;
	}
	double Min = 0.0;
	double Max = 1.0;
	FGSTN_TimelineLayout::GetScrollRange(Model->ViewTransform, Width, Model->GetPlayableFrames(), Min, Max);
	const double Range = FMath::Max(Max - Min, 1e-3);
	const double Visible = Model->ViewTransform.GetVisibleFrameCount(Width);
	HorizontalScrollBar->SetState(static_cast<float>((Model->ViewTransform.FirstVisibleFrame - Min) / Range), static_cast<float>(FMath::Min(Visible / Range, 1.0)));
}

void SGSTN_TimelinePanel::HandleHorizontalScroll(float Offset)
{
	const double Width = GetLaneWidth();
	if (Width <= 1.0)
	{
		return;
	}
	double Min = 0.0;
	double Max = 1.0;
	FGSTN_TimelineLayout::GetScrollRange(Model->ViewTransform, Width, Model->GetPlayableFrames(), Min, Max);
	Model->ViewTransform.FirstVisibleFrame = Min + static_cast<double>(Offset) * (Max - Min);
	FGSTN_TimelineLayout::ClampView(Model->ViewTransform, Width, Model->GetPlayableFrames());
	Model->NotifyViewChanged();
}

FReply SGSTN_TimelinePanel::OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Middle or right drag anywhere on the timeline pans it sideways (Sequencer, the animation editors). Taken before the lanes see it: they use the left button only
	const FKey Button = MouseEvent.GetEffectingButton();
	if (Button == EKeys::MiddleMouseButton || Button == EKeys::RightMouseButton)
	{
		bPanning = true;
		PanMovedPixels = 0.0;
		return FReply::Handled().CaptureMouse(AsShared());
	}
	return FReply::Unhandled();
}

FReply SGSTN_TimelinePanel::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (!bPanning || !HasMouseCapture())
	{
		return FReply::Unhandled();
	}
	const double Delta = MouseEvent.GetCursorDelta().X;
	PanMovedPixels += FMath::Abs(Delta);
	Model->ViewTransform.ScrollByPixels(-Delta);
	FGSTN_TimelineLayout::ClampView(Model->ViewTransform, GetLaneWidth(), Model->GetPlayableFrames());
	Model->NotifyViewChanged();
	return FReply::Handled();
}

FReply SGSTN_TimelinePanel::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bPanning && (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton || MouseEvent.GetEffectingButton() == EKeys::RightMouseButton))
	{
		bPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE
