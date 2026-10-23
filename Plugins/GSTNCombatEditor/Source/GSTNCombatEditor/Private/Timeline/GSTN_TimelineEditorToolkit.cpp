// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_TimelineEditorToolkit.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Containers/Ticker.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GSTNCombatEditorModule.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "Misc/MessageDialog.h"
#include "Misc/PackageName.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_InlineTimeline.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/AppStyle.h"
#include "Timeline/GSTN_TimelineEditorCommands.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineGesture.h"
#include "Timeline/GSTN_TimelineUndo.h"
#include "Timeline/Widgets/GSTN_TimelineWidgetCommon.h"
#include "Timeline/Widgets/SGSTN_FindingsList.h"
#include "Timeline/Widgets/SGSTN_FrameDataSummary.h"
#include "Timeline/Widgets/SGSTN_InlineTimelineBar.h"
#include "Timeline/Widgets/SGSTN_MontageLengthBar.h"
#include "Timeline/Widgets/SGSTN_TimelineDetails.h"
#include "Timeline/Widgets/SGSTN_TimelineLengthFields.h"
#include "Timeline/Widgets/SGSTN_TimelinePanel.h"
#include "Timeline/Widgets/SGSTN_TimelineToolbarExtra.h"
#include "UObject/Object.h"
#include "Validation/GSTN_CombatValidators.h"
#include "Validation/GSTN_ValidationMessageLog.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

const FName FGSTN_TimelineEditorToolkit::LayoutName(TEXT("GSTN_TimelineEditor_Layout_v1"));
const FName FGSTN_TimelineEditorToolkit::ViewportTabId(TEXT("GSTNTimelineEditor_Viewport"));
const FName FGSTN_TimelineEditorToolkit::TimelineTabId(TEXT("GSTNTimelineEditor_Timeline"));
const FName FGSTN_TimelineEditorToolkit::DetailsTabId(TEXT("GSTNTimelineEditor_Details"));
const FName FGSTN_TimelineEditorToolkit::FrameDataTabId(TEXT("GSTNTimelineEditor_FrameData"));

namespace
{
	/** The app identifier the asset editor subsystem files this editor under */
	const FName TimelineEditorAppIdentifier(TEXT("GSTNTimelineEditorApp"));
}

FGSTN_TimelineEditorToolkit::FGSTN_TimelineEditorToolkit() = default;

FGSTN_TimelineEditorToolkit::~FGSTN_TimelineEditorToolkit()
{
	Release();
}

void FGSTN_TimelineEditorToolkit::Release()
{
	if (UBlueprint* Blueprint = ObservedBlueprint.Get())
	{
		Blueprint->OnCompiled().RemoveAll(this);
	}
	ObservedBlueprint.Reset();

	// The client first: it points at the model. The lease last: the target is free for another editor only when this one is gone
	UndoClient.Reset();
	Model.Reset();
	Lease.Reset();
}

TSharedRef<FTabManager::FLayout> FGSTN_TimelineEditorToolkit::MakeDefaultLayout()
{
	// Left: the preview above the timeline. Right: the details of the selection above the frame data
	return FTabManager::NewLayout(LayoutName)
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.75f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.45f)
					->AddTab(ViewportTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.55f)
					->AddTab(TimelineTabId, ETabState::OpenedTab)
				)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.25f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.65f)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.35f)
					->AddTab(FrameDataTabId, ETabState::OpenedTab)
				)
			)
		);
}

bool FGSTN_TimelineEditorToolkit::InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, const FGSTN_TimelineEditContext& InContext)
{
	if (!InContext.IsValid())
	{
		return false;
	}

	// One editor per target. When another one holds it, that editor is asked to come to the front and this one is not opened
	const TWeakPtr<FGSTN_TimelineEditorToolkit> WeakSelf = StaticCastSharedRef<FGSTN_TimelineEditorToolkit>(AsShared());
	Lease = FGSTN_TimelineEditContextRegistry::Acquire(InContext, [WeakSelf]()
	{
		if (const TSharedPtr<FGSTN_TimelineEditorToolkit> Pinned = WeakSelf.Pin())
		{
			Pinned->FocusWindow();
		}
	});
	if (!Lease.IsValid())
	{
		return false;
	}

	Context = InContext;

	// An asset that was not created transactional (an old one, a script) would record nothing and undo would have nothing to restore
	if (UObject* Owner = Context.GetOwner())
	{
		Owner->SetFlags(RF_Transactional);
	}

	Model = MakeShared<FGSTN_TimelineEditorModel>(Context);
	UndoClient = MakeUnique<FGSTN_TimelineUndoClient>(*Model);
	// Findings about the timeline as it was when Validate ran are marked out of date by the next edit (the model, and its delegate, go with this toolkit)
	Model->OnDataChanged.AddSP(this, &FGSTN_TimelineEditorToolkit::HandleDataChanged);

	constexpr bool bCreateDefaultStandaloneMenu = true;
	constexpr bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, TimelineEditorAppIdentifier, MakeDefaultLayout(), bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Context.GetTarget());

	// Inline mode: the compile of the Blueprint replaces the default object the data lives in
	if (UBlueprint* Blueprint = Context.GetBlueprint())
	{
		ObservedBlueprint = Blueprint;
		Blueprint->OnCompiled().AddSP(this, &FGSTN_TimelineEditorToolkit::HandleBlueprintCompiled);
	}

	// The chord of Validate (F7) lives on the toolkit, not on the panel's command list: the toolbar button and the chord do the same thing
	ToolkitCommands->MapAction(FGSTN_TimelineEditorCommands::Get().ValidateNow, FExecuteAction::CreateSPLambda(this, [this]() { ValidateNow(); }));
	return true;
}

void FGSTN_TimelineEditorToolkit::HandleBlueprintCompiled(UBlueprint* Blueprint)
{
	if (!Model.IsValid())
	{
		return;
	}
	if (GSTNInlineTimeline::DecideAfterCompile(Context) == GSTNInlineTimeline::EAfterCompile::Refresh)
	{
		// A new default object with the same values: the model resolves it on every call, the views only need to read again
		Model->HandleUndoRedo();
		return;
	}

	FNotificationInfo Info(FText::Format(LOCTEXT("InlineTimelineGone", "{0} has no inline combat timeline any more (it no longer derives from the combat ability). The timeline editor was closed."), Context.GetDisplayName()));
	Info.ExpireDuration = 8.f;
	FSlateNotificationManager::Get().AddNotification(Info);
	UE_LOG(LogGSTNCombatEditor, Warning, TEXT("The inline timeline of %s is gone after a compile; closing its timeline editor."), *Context.GetDisplayName().ToString());

	// Not from inside the compile: the Blueprint is still being reinstanced around this call
	const TWeakPtr<FGSTN_TimelineEditorToolkit> WeakSelf = StaticCastSharedRef<FGSTN_TimelineEditorToolkit>(AsShared());
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakSelf](float) -> bool
	{
		if (const TSharedPtr<FGSTN_TimelineEditorToolkit> Pinned = WeakSelf.Pin())
		{
			Pinned->CloseWindow(EAssetEditorCloseReason::AssetUnloadingOrInvalid);
		}
		return false;
	}));
}

void FGSTN_TimelineEditorToolkit::ExtractToAsset()
{
	if (!Context.IsInline() || !Context.IsValid())
	{
		return;
	}

	// An ability that names a Timeline Asset plays that asset, not the inline data: extracting would switch it to the new one
	if (GSTNInlineTimeline::ExtractWouldReplaceAsset(Context))
	{
		const FText Question = FText::Format(
			LOCTEXT("ExtractReplacesAsset", "{0} already names a Timeline Asset, and that asset is what plays; this inline timeline is not used. Extracting will point the ability at the new asset instead and the current one will no longer play.\n\nExtract anyway?"),
			Context.GetDisplayName());
		if (FMessageDialog::Open(EAppMsgType::YesNo, Question) != EAppReturnType::Yes)
		{
			return;
		}
	}

	FString PackagePath;
	FString AssetName;
	GSTNInlineTimeline::GetExtractDefaults(Context, PackagePath, AssetName);
	FString UniquePackageName;
	FString UniqueAssetName;
	FAssetToolsModule::GetModule().Get().CreateUniqueAssetName(PackagePath / AssetName, FString(), UniquePackageName, UniqueAssetName);

	FSaveAssetDialogConfig Config;
	Config.DialogTitleOverride = LOCTEXT("ExtractDialogTitle", "Extract Timeline Asset");
	Config.DefaultPath = FPackageName::GetLongPackagePath(UniquePackageName);
	Config.DefaultAssetName = UniqueAssetName;
	Config.AssetClassNames.Add(UGSTN_CombatTimeline::StaticClass()->GetClassPathName());
	Config.ExistingAssetPolicy = ESaveAssetDialogExistingAssetPolicy::Disallow;
	const FString ObjectPath = IContentBrowserSingleton::Get().CreateModalSaveAssetDialog(Config);
	if (ObjectPath.IsEmpty())
	{
		return;
	}

	const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
	UPackage* Package = CreatePackage(*PackageName);
	if (Package == nullptr)
	{
		return;
	}
	UGSTN_CombatTimeline* NewAsset = NewObject<UGSTN_CombatTimeline>(Package, FName(*FPackageName::GetLongPackageAssetName(PackageName)), RF_Public | RF_Standalone | RF_Transactional);
	// Replacing the named asset was confirmed above, before the save dialog
	if (NewAsset == nullptr || !GSTNInlineTimeline::ExtractToAsset(Context, NewAsset, [](const FSoftObjectPath&) { return true; }))
	{
		if (NewAsset != nullptr)
		{
			// Nothing refers to it and it was never announced to the asset registry: let it go
			NewAsset->ClearFlags(RF_Public | RF_Standalone);
			NewAsset->MarkAsGarbage();
		}
		return;
	}
	FAssetRegistryModule::AssetCreated(NewAsset);
	Package->MarkPackageDirty();

	// This window edits the inline data, which is empty now: the asset takes over in its own editor
	CloseWindow(EAssetEditorCloseReason::AssetEditorHostClosed);
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(NewAsset);
}

void FGSTN_TimelineEditorToolkit::HandleDataChanged()
{
	Findings->MarkStale();
}

void FGSTN_TimelineEditorToolkit::GoToFinding(const FGSTN_ValidationFinding& Finding)
{
	if (Model.IsValid())
	{
		FGSTN_TimelineActions::GoToFinding(*Model, Finding.Where);
	}
}

FGSTN_ValidationReport FGSTN_TimelineEditorToolkit::ValidateNow()
{
	FGSTN_ValidationReport Report;
	const FGSTN_CombatTimelineData* Data = Model.IsValid() ? Model->GetData() : nullptr;
	if (!Data)
	{
		return Report;
	}

	// The asset's own name on the page, so two validations of two timelines can be told apart in the log
	const FGSTN_TimelineRuleContext RuleContext = FGSTN_CombatValidators::MakeTimelineContext(*Data, Context.GetTarget(), Model->GetTickRate(), true);
	FGSTN_CombatValidators::ValidateTimeline(*Data, RuleContext, Report);
	// The log window pops up only when the Frame Data tab is closed: with it open the findings are already on screen
	const bool bFrameDataOpen = TabManager.IsValid() && TabManager->FindExistingLiveTab(FrameDataTabId).IsValid();
	GSTNValidationLog::WriteReport(Report, FText::Format(LOCTEXT("ValidatePageTitle", "Validate {0}"), Context.GetDisplayName()), true, !bFrameDataOpen);
	// The same report in the Frame Data tab: one run, two places to read it
	Findings->SetReport(Report);
	return Report;
}

void FGSTN_TimelineEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenuTimelineEditor", "GSTN Timeline Editor"));
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(ViewportTabId, FOnSpawnTab::CreateSP(this, &FGSTN_TimelineEditorToolkit::SpawnViewportTab))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));
	InTabManager->RegisterTabSpawner(TimelineTabId, FOnSpawnTab::CreateSP(this, &FGSTN_TimelineEditorToolkit::SpawnTimelineTab))
		.SetDisplayName(LOCTEXT("TimelineTab", "Timeline"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Event"));
	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FGSTN_TimelineEditorToolkit::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
	InTabManager->RegisterTabSpawner(FrameDataTabId, FOnSpawnTab::CreateSP(this, &FGSTN_TimelineEditorToolkit::SpawnFrameDataTab))
		.SetDisplayName(LOCTEXT("FrameDataTab", "Frame Data"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Validate"));
}

void FGSTN_TimelineEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(ViewportTabId);
	InTabManager->UnregisterTabSpawner(TimelineTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(FrameDataTabId);
}

FName FGSTN_TimelineEditorToolkit::GetToolkitFName() const
{
	return TEXT("GSTNTimelineEditor");
}

FText FGSTN_TimelineEditorToolkit::GetBaseToolkitName() const
{
	if (Context.IsInline())
	{
		return LOCTEXT("InlineToolkitName", "Inline Combat Timeline Editor");
	}
	return LOCTEXT("ToolkitName", "Combat Timeline Editor");
}

FString FGSTN_TimelineEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Timeline ").ToString();
}

FLinearColor FGSTN_TimelineEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.9f, 0.4f, 0.15f, 0.5f);
}

void FGSTN_TimelineEditorToolkit::OnClose()
{
	FAssetEditorToolkit::OnClose();
	Release();
}

TSharedRef<SDockTab> FGSTN_TimelineEditorToolkit::SpawnViewportTab(const FSpawnTabArgs& Args)
{
	// The preview scene is E6. The tab exists so the layout has its final shape and a saved layout does not change when the scene arrives
	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTabLabel", "Viewport"))
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ViewportPlaceholder", "The preview is not available yet."))
				.ColorAndOpacity(FSlateColor(GSTNTimelineWidgets::TextColorDim()))
			]
		];
}

TSharedRef<SDockTab> FGSTN_TimelineEditorToolkit::SpawnTimelineTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("TimelineTabLabel", "Timeline"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SGSTN_InlineTimelineBar)
				.Model(Model)
				.OnExtract(FSimpleDelegate::CreateSP(this, &FGSTN_TimelineEditorToolkit::ExtractToAsset))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SGSTN_MontageLengthBar).Model(Model)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SGSTN_TimelinePanel)
				.Model(Model)
				.ToolbarExtra()
				[
					SNew(SGSTN_TimelineToolbarExtra)
					.Model(Model)
					.OnValidate(FSimpleDelegate::CreateSPLambda(this, [this]() { ValidateNow(); }))
				]
			]
		];
}

TSharedRef<SDockTab> FGSTN_TimelineEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTabLabel", "Details"))
		[
			SNew(SGSTN_TimelineDetails).Model(Model)
		];
}

TSharedRef<SDockTab> FGSTN_TimelineEditorToolkit::SpawnFrameDataTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("FrameDataTabLabel", "Frame Data"))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.f, 8.f, 8.f, 4.f))
			[
				SNew(SGSTN_TimelineLengthFields).Model(Model)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.f, 4.f))
			[
				SNew(SGSTN_FrameDataSummary).Model(Model)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(FMargin(8.f, 4.f, 8.f, 8.f))
			[
				SNew(SGSTN_FindingsList)
				.Store(Findings)
				.OnFindingActivated(SGSTN_FindingsList::FOnFindingActivated::CreateSP(this, &FGSTN_TimelineEditorToolkit::GoToFinding))
			]
		];
}

#undef LOCTEXT_NAMESPACE
