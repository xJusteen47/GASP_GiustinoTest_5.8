// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_TimelineEditorToolkit.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Settings/GSTN_CombatEditorSettings.h"
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
#include "Preview/GSTN_PreviewController.h"
#include "Preview/SGSTN_TimelineViewport.h"
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

	/** Every toolkit that holds a target, weakly: a closed window drops out by itself */
	TArray<TWeakPtr<FGSTN_TimelineEditorToolkit>>& GetOpenToolkits()
	{
		static TArray<TWeakPtr<FGSTN_TimelineEditorToolkit>> Open;
		return Open;
	}
}

TSharedPtr<FGSTN_TimelineEditorToolkit> FGSTN_TimelineEditorToolkit::FindOpenFor(const UObject* Target)
{
	TArray<TWeakPtr<FGSTN_TimelineEditorToolkit>>& Open = GetOpenToolkits();
	Open.RemoveAll([](const TWeakPtr<FGSTN_TimelineEditorToolkit>& Toolkit) { return !Toolkit.IsValid(); });
	for (const TWeakPtr<FGSTN_TimelineEditorToolkit>& Weak : Open)
	{
		const TSharedPtr<FGSTN_TimelineEditorToolkit> Toolkit = Weak.Pin();
		if (Toolkit.IsValid() && Toolkit->Model.IsValid() && Toolkit->Context.GetTarget() == Target)
		{
			return Toolkit;
		}
	}
	return nullptr;
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

	CancelFindingsRefresh();
	if (Preview.IsValid())
	{
		Preview->OnSubjectChanged.Remove(SubjectChangedHandle);
	}
	SubjectChangedHandle.Reset();

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
	GetOpenToolkits().Add(StaticCastSharedRef<FGSTN_TimelineEditorToolkit>(AsShared()));

	// An asset that was not created transactional (an old one, a script) would record nothing and undo would have nothing to restore
	if (UObject* Owner = Context.GetOwner())
	{
		Owner->SetFlags(RF_Transactional);
	}

	Model = MakeShared<FGSTN_TimelineEditorModel>(Context);
	// The preview scene follows the model's playhead and montage; its subject is stored per asset in the user's config
	Preview = FGSTN_PreviewController::Create(Model.ToSharedRef());
	// A different mesh is a different bake input: the status and the findings follow the subject
	SubjectChangedHandle = Preview->OnSubjectChanged.AddSP(this, &FGSTN_TimelineEditorToolkit::HandleSubjectChanged);
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

	// The first findings, so the Frame Data tab and the lane markers are not empty until someone presses Validate
	RefreshFindings();

	// The chord of Validate (F7) lives on the toolkit, not on the panel's command list: the toolbar button and the chord do the same thing
	ToolkitCommands->MapAction(FGSTN_TimelineEditorCommands::Get().ValidateNow, FExecuteAction::CreateSPLambda(this, [this]() { ValidateNow(); }));
	ToolkitCommands->MapAction(FGSTN_TimelineEditorCommands::Get().BakeSockets, FExecuteAction::CreateSPLambda(this, [this]() { BakeSockets(); }));
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

	// Debounced: every edit of a drag restarts the wait, so the rules run once when the hand stops, not on each mouse move
	CancelFindingsRefresh();
	const TWeakPtr<FGSTN_TimelineEditorToolkit> WeakSelf = StaticCastSharedRef<FGSTN_TimelineEditorToolkit>(AsShared());
	FindingsTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakSelf](float) -> bool
	{
		if (const TSharedPtr<FGSTN_TimelineEditorToolkit> Pinned = WeakSelf.Pin())
		{
			Pinned->FindingsTickerHandle.Reset();
			Pinned->RefreshFindings();
		}
		return false;
	}), FindingsDebounceSeconds);
}

void FGSTN_TimelineEditorToolkit::CancelFindingsRefresh()
{
	if (FindingsTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FindingsTickerHandle);
		FindingsTickerHandle.Reset();
	}
}

FGSTN_ValidationReport FGSTN_TimelineEditorToolkit::RefreshFindings()
{
	FGSTN_ValidationReport Report;
	const FGSTN_CombatTimelineData* Data = Model.IsValid() ? Model->GetData() : nullptr;
	if (!Data)
	{
		return Report;
	}
	FGSTN_TimelineRuleContext RuleContext = FGSTN_CombatValidators::MakeTimelineContext(*Data, Context.GetTarget(), Model->GetTickRate(), true);
	// The bake is judged against the mesh the preview shows, the one a click on Bake sockets would use
	RefreshBakeStatus();
	RuleContext.BakeStatus = BakeStatus;
	FGSTN_CombatValidators::ValidateTimeline(*Data, RuleContext, Report);
	Findings->SetReport(Report);
	// The lanes paint the markers from the store; they repaint on the model's view notification
	Model->NotifyViewChanged();
	return Report;
}

void FGSTN_TimelineEditorToolkit::HandleSubjectChanged()
{
	// The mesh is an input of the bake (and of its staleness). Re-validating now, not after the debounce: a subject change is one click, not a drag
	if (Model.IsValid())
	{
		RefreshFindings();
	}
}

FGSTN_BakeInputs FGSTN_TimelineEditorToolkit::MakeBakeInputs() const
{
	FGSTN_BakeInputs Inputs;
	const FGSTN_CombatTimelineData* Data = Model.IsValid() ? Model->GetData() : nullptr;
	Inputs.TickRateHz = Model.IsValid() ? Model->GetTickRate() : UGSTN_CombatEditorSettings::DefaultTickRateHz;
	if (Preview.IsValid())
	{
		Inputs.Montage = Preview->GetMontage();
		Inputs.Mesh = Preview->GetMainMesh();
	}
	if (Data != nullptr)
	{
		if (Inputs.Montage == nullptr && !Data->Montage.IsNull())
		{
			Inputs.Montage = Data->Montage.LoadSynchronous();
		}
		// No preview mesh: the status is that of the mesh the last bake used, which is the only one the asset can name
		if (Inputs.Mesh == nullptr && Data->BakeInfo.IsSet())
		{
			Inputs.Mesh = Cast<USkeletalMesh>(Data->BakeInfo.Mesh.TryLoad());
		}
	}
	return Inputs;
}

void FGSTN_TimelineEditorToolkit::RefreshBakeStatus()
{
	const FGSTN_CombatTimelineData* Data = Model.IsValid() ? Model->GetData() : nullptr;
	if (Data == nullptr)
	{
		return;
	}
	BakeStatus = GSTNBake::GetStatus(*Data, MakeBakeInputs());
	Model->NotifyViewChanged();
}

FGSTN_BakeResult FGSTN_TimelineEditorToolkit::BakeSockets()
{
	FGSTN_BakeResult Result;
	const FGSTN_CombatTimelineData* Data = Model.IsValid() ? Model->GetData() : nullptr;
	if (Data == nullptr || !Preview.IsValid())
	{
		return Result;
	}

	FGSTN_BakeInputs Inputs = MakeBakeInputs();
	// The preview mesh, not the recorded one: what is baked is what is shown (a missing preview mesh is the first problem the baker names)
	Inputs.Mesh = Preview->GetMainMesh();
	Result = FGSTN_SocketBaker::Bake(*Data, Preview->GetScene()->GetPose(), Inputs);
	// The baker moved the pose through the frames: put it back on the playhead
	Preview->Refresh();

	if (Result.bBaked)
	{
		Model->ApplyDataEdit([&Result](FGSTN_CombatTimelineData& Edited) { FGSTN_SocketBaker::Apply(Edited, Result); }, LOCTEXT("BakeSocketsTransaction", "Bake sockets"));
		FNotificationInfo Info(Result.Sockets.IsEmpty()
			? LOCTEXT("BakeNothing", "No hitbox uses a baked socket: nothing to bake.")
			: FText::Format(LOCTEXT("BakeDone", "Baked {0} sockets over {1} frames ({2} ms)."), FText::AsNumber(Result.Sockets.Num()), FText::AsNumber(Result.FrameCount), FText::AsNumber(FMath::RoundToInt(Result.ElapsedMs))));
		Info.ExpireDuration = 4.f;
		FSlateNotificationManager::Get().AddNotification(Info);
		for (const FText& Warning : Result.Warnings)
		{
			UE_LOG(LogGSTNCombatEditor, Warning, TEXT("Bake sockets (%s): %s"), *Context.GetDisplayName().ToString(), *Warning.ToString());
		}
	}
	else
	{
		FGSTN_ValidationReport Report;
		for (const FGSTN_BakeProblem& Problem : Result.Problems)
		{
			Report.Add(EGSTN_Severity::Error, GSTNValidationCodes::BakedSocketMissing(), Problem.Message, Context.GetTarget(),
				Problem.TrackIndex != INDEX_NONE ? FGSTN_FindingLocation::OnTrack(Problem.TrackIndex) : FGSTN_FindingLocation());
		}
		GSTNValidationLog::WriteReport(Report, FText::Format(LOCTEXT("BakePageTitle", "Bake sockets {0}"), Context.GetDisplayName()), true, true);
		FNotificationInfo Info(LOCTEXT("BakeFailed", "The sockets were not baked and the previous bake was kept. The Message Log lists why."));
		Info.ExpireDuration = 6.f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}
	RefreshFindings();
	return Result;
}

void FGSTN_TimelineEditorToolkit::SaveAsset_Execute()
{
	// Preference "Bake on save" (off by default): a stale bake is refreshed on the way to disk, so what is saved is current
	if (UGSTN_CombatEditorSettings::Get().bBakeOnSave && Model.IsValid() && Preview.IsValid())
	{
		RefreshBakeStatus();
		if (BakeStatus.NeedsBake() && Preview->GetMainMesh() != nullptr)
		{
			BakeSockets();
		}
	}
	FAssetEditorToolkit::SaveAsset_Execute();
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
	FGSTN_TimelineRuleContext RuleContext = FGSTN_CombatValidators::MakeTimelineContext(*Data, Context.GetTarget(), Model->GetTickRate(), true);
	RefreshBakeStatus();
	RuleContext.BakeStatus = BakeStatus;
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
	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTabLabel", "Viewport"))
		[
			SNew(SGSTN_TimelineViewport).Controller(Preview)
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
				.Findings(Findings)
				.ToolbarExtra()
				[
					SNew(SGSTN_TimelineToolbarExtra)
					.Model(Model)
					.Preview(Preview)
					.OnValidate(FSimpleDelegate::CreateSPLambda(this, [this]() { ValidateNow(); }))
					.OnBake(FSimpleDelegate::CreateSPLambda(this, [this]() { BakeSockets(); }))
					.BakeStatus_Lambda([this]() { return BakeStatus.Status; })
					.BakeReason_Lambda([this]() { return BakeStatus.Reason; })
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
