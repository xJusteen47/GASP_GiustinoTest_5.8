// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Timeline/GSTN_TimelineEditorToolkit.h"

#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "Timeline/GSTN_TimelineEditorModel.h"
#include "Timeline/GSTN_TimelineUndo.h"
#include "Timeline/Widgets/GSTN_TimelineWidgetCommon.h"
#include "Timeline/Widgets/SGSTN_FrameDataSummary.h"
#include "Timeline/Widgets/SGSTN_MontageLengthBar.h"
#include "Timeline/Widgets/SGSTN_TimelineDetails.h"
#include "Timeline/Widgets/SGSTN_TimelinePanel.h"
#include "Timeline/Widgets/SGSTN_TimelineToolbarExtra.h"
#include "UObject/Object.h"
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

	constexpr bool bCreateDefaultStandaloneMenu = true;
	constexpr bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, TimelineEditorAppIdentifier, MakeDefaultLayout(), bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Context.GetTarget());
	return true;
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
				SNew(SGSTN_MontageLengthBar).Model(Model)
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SGSTN_TimelinePanel)
				.Model(Model)
				.ToolbarExtra()
				[
					SNew(SGSTN_TimelineToolbarExtra).Model(Model)
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
			.Padding(FMargin(8.f, 8.f))
			[
				SNew(SGSTN_FrameDataSummary).Model(Model)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(8.f, 0.f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("FrameDataFindings", "Validation findings will be listed here."))
				.ColorAndOpacity(FSlateColor(GSTNTimelineWidgets::TextColorDim()))
			]
		];
}

#undef LOCTEXT_NAMESPACE
