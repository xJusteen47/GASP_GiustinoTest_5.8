// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Hurtbox/GSTN_HurtboxSetEditorToolkit.h"

#include "Engine/SkeletalMesh.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GSTNCombatEditorModule.h"
#include "Hurtbox/SGSTN_HurtboxDetails.h"
#include "Hurtbox/SGSTN_HurtboxList.h"
#include "Misc/MessageDialog.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Hurtbox/GSTN_HurtboxGizmoTarget.h"
#include "Preview/GSTN_HitboxHitProxy.h"
#include "Preview/GSTN_PreviewController.h"
#include "SceneManagement.h"
#include "Preview/GSTN_PreviewDrawing.h"
#include "Preview/SGSTN_TimelineViewport.h"
#include "PropertyCustomizationHelpers.h"
#include "Settings/GSTN_CombatEditorSettings.h"
#include "Styling/AppStyle.h"
#include "Timeline/Widgets/SGSTN_FindingsList.h"
#include "Validation/GSTN_CombatValidators.h"
#include "Validation/GSTN_ValidationMessageLog.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

const FName FGSTN_HurtboxSetEditorToolkit::LayoutName(TEXT("GSTN_HurtboxSetEditor_Layout_v1"));
const FName FGSTN_HurtboxSetEditorToolkit::ViewportTabId(TEXT("GSTNHurtboxSetEditor_Viewport"));
const FName FGSTN_HurtboxSetEditorToolkit::HurtboxesTabId(TEXT("GSTNHurtboxSetEditor_Hurtboxes"));
const FName FGSTN_HurtboxSetEditorToolkit::DetailsTabId(TEXT("GSTNHurtboxSetEditor_Details"));
const FName FGSTN_HurtboxSetEditorToolkit::FindingsTabId(TEXT("GSTNHurtboxSetEditor_Findings"));

namespace
{
	const FName HurtboxSetEditorAppIdentifier(TEXT("GSTNHurtboxSetEditorApp"));
}

FGSTN_HurtboxSetEditorToolkit::FGSTN_HurtboxSetEditorToolkit() = default;

FGSTN_HurtboxSetEditorToolkit::~FGSTN_HurtboxSetEditorToolkit()
{
	if (Model.IsValid())
	{
		Model->OnChanged.Remove(ChangedHandle);
	}
	if (Preview.IsValid())
	{
		Preview->OnSubjectChanged.Remove(SubjectHandle);
	}
	// The client first: it points at the model
	UndoClient.Reset();
	Model.Reset();
}

TSharedRef<FTabManager::FLayout> FGSTN_HurtboxSetEditorToolkit::MakeDefaultLayout()
{
	// Left: the preview. Right: the list above the details of the selected hurtbox above the findings
	return FTabManager::NewLayout(LayoutName)
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.55f)
				->AddTab(ViewportTabId, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.45f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.4f)
					->AddTab(HurtboxesTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.4f)
					->AddTab(DetailsTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)
					->AddTab(FindingsTabId, ETabState::OpenedTab)
				)
			)
		);
}

bool FGSTN_HurtboxSetEditorToolkit::InitEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UGSTN_HurtboxSetDefinition* InSet)
{
	if (InSet == nullptr)
	{
		return false;
	}
	Set = InSet;
	Model = MakeShared<FGSTN_HurtboxEditModel>(InSet);
	UndoClient = MakeUnique<FGSTN_HurtboxUndoClient>(*Model);

	// The subject of this set is kept like a timeline's, under the set's path in the user's config; the set's own PreviewMesh (shared with the team) wins
	Preview = FGSTN_PreviewController::CreateWithoutTimeline(FSoftObjectPath(InSet));
	SyncPreviewFromSet();

	ChangedHandle = Model->OnChanged.AddSP(this, &FGSTN_HurtboxSetEditorToolkit::HandleSetChanged);
	Model->OnSelectionChanged.AddSPLambda(this, [this]() { Preview->OnPreviewChanged.Broadcast(); });
	SubjectHandle = Preview->OnSubjectChanged.AddSP(this, &FGSTN_HurtboxSetEditorToolkit::HandleSubjectChanged);

	constexpr bool bCreateDefaultStandaloneMenu = true;
	constexpr bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, HurtboxSetEditorAppIdentifier, MakeDefaultLayout(), bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InSet);

	RefreshMirrorConfig();
	RefreshFindings();
	return true;
}

void FGSTN_HurtboxSetEditorToolkit::SyncPreviewFromSet()
{
#if WITH_EDITORONLY_DATA
	const UGSTN_HurtboxSetDefinition* Resolved = Set.Get();
	if (Resolved != nullptr && Preview.IsValid() && !Resolved->PreviewMesh.IsNull())
	{
		Preview->SetMainMesh(Resolved->PreviewMesh);
	}
#endif
}

void FGSTN_HurtboxSetEditorToolkit::HandleSetChanged()
{
	// An undo of the preview mesh changes the set behind the preview's back
	SyncPreviewFromSet();
	RefreshMirrorConfig();
	RefreshFindings();
	if (Preview.IsValid())
	{
		Preview->OnPreviewChanged.Broadcast();
	}
}

void FGSTN_HurtboxSetEditorToolkit::HandleSubjectChanged()
{
	RefreshMirrorConfig();
	RefreshFindings();
}

void FGSTN_HurtboxSetEditorToolkit::RefreshMirrorConfig()
{
	const UGSTN_HurtboxSetDefinition* Resolved = Set.Get();
	if (Resolved == nullptr || !Preview.IsValid())
	{
		return;
	}
	TArray<FName> BoneNames;
	const USkeletalMesh* Mesh = Preview->GetMainMesh();
	if (Mesh != nullptr)
	{
		const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();
		for (int32 Bone = 0; Bone < RefSkeleton.GetNum(); ++Bone)
		{
			BoneNames.Add(RefSkeleton.GetBoneName(Bone));
		}
	}
	TArray<FName> HurtboxNames;
	for (const FGSTN_Hurtbox& Hurtbox : Resolved->Hurtboxes)
	{
		HurtboxNames.Add(Hurtbox.Name);
	}
	MirrorConfig = GSTNHurtboxMirror::ResolveConfig(UGSTN_CombatEditorSettings::Get(), Mesh != nullptr ? Mesh->GetSkeleton() : nullptr, BoneNames, HurtboxNames);
}

FGSTN_ValidationReport FGSTN_HurtboxSetEditorToolkit::RefreshFindings()
{
	FGSTN_ValidationReport Report;
	UGSTN_HurtboxSetDefinition* Resolved = Set.Get();
	if (Resolved == nullptr)
	{
		return Report;
	}
	FGSTN_CombatValidators::CheckHurtboxSet(*Resolved, Resolved, Report, Preview.IsValid() ? Preview->GetMainMesh() : nullptr);
	Findings->SetReport(Report);
	return Report;
}

void FGSTN_HurtboxSetEditorToolkit::Notify(const FText& Message, float Seconds) const
{
	FNotificationInfo Info(Message);
	Info.ExpireDuration = Seconds;
	FSlateNotificationManager::Get().AddNotification(Info);
}

FGSTN_RegenerateSummary FGSTN_HurtboxSetEditorToolkit::GenerateFromPhysicsAsset(EGSTN_RegenerateMode Mode)
{
	FGSTN_RegenerateSummary Summary;
	USkeletalMesh* Mesh = Preview.IsValid() ? Preview->GetMainMesh() : nullptr;
	if (!Model.IsValid() || Mesh == nullptr)
	{
		Notify(LOCTEXT("GenerateNoMesh", "There is no preview mesh. Choose the mesh of this character in Preview mesh (or a preview subject in the timeline editor) first."));
		return Summary;
	}
	const UPhysicsAsset* PhysicsAsset = Mesh->GetPhysicsAsset();
	if (PhysicsAsset == nullptr)
	{
		Notify(FText::Format(LOCTEXT("GenerateNoPhysicsAsset", "{0} has no Physics Asset. Assign one to the mesh, then generate again."), FText::FromString(Mesh->GetName())));
		return Summary;
	}
	if (Mode == EGSTN_RegenerateMode::ReplaceAll && Model->Num() > 0)
	{
		const FText Question = FText::Format(LOCTEXT("GenerateReplaceAllQuestion", "Replace all {0} hurtboxes with the ones generated from the Physics Asset? Every change made by hand (shapes, tags, priorities, reaction scales) is lost. It can be undone."), FText::AsNumber(Model->Num()));
		if (FMessageDialog::Open(EAppMsgType::YesNo, Question) != EAppReturnType::Yes)
		{
			return Summary;
		}
	}

	FGSTN_HurtboxGenOptions Options = FGSTN_HurtboxGenOptions::FromSettings(UGSTN_CombatEditorSettings::Get());
	Options.MeshToRoot = Preview->GetScene()->GetMeshToRoot();
	const FGSTN_HurtboxGenResult Generation = GSTNHurtboxGenerator::FromPhysicsAsset(*PhysicsAsset, *Mesh, Options);
	if (Generation.Hurtboxes.IsEmpty())
	{
		Notify(LOCTEXT("GenerateNothing", "The Physics Asset has no body that makes a hurtbox."));
		return Summary;
	}

	Summary = Model->ApplyGeneration(Generation, Mode);
	if (!Generation.Warnings.IsEmpty())
	{
		FGSTN_ValidationReport Report;
		for (const FText& Warning : Generation.Warnings)
		{
			Report.Add(EGSTN_Severity::Warning, GSTNValidationCodes::HurtboxGenerate(), Warning, Set.Get());
		}
		GSTNValidationLog::WriteReport(Report, FText::Format(LOCTEXT("GeneratePageTitle", "Generate hurtboxes {0}"), FText::FromString(Set->GetName())), true, true);
	}
	FText Message = FText::Format(LOCTEXT("GenerateDone", "Generated from {0} bodies ({1} primitives): {2} added, {3} kept or updated, {4} removed."),
		FText::AsNumber(Generation.BodyCount), FText::AsNumber(Generation.PrimitiveCount), FText::AsNumber(Summary.Added), FText::AsNumber(Summary.Kept + Summary.Updated), FText::AsNumber(Summary.Removed));
	if (!Summary.NotInGeneration.IsEmpty())
	{
		Message = FText::Format(LOCTEXT("GenerateNotInGeneration", "{0} {1} hurtboxes of the set are not in the Physics Asset and were kept."), Message, FText::AsNumber(Summary.NotInGeneration.Num()));
	}
	Notify(Message);
	return Summary;
}

int32 FGSTN_HurtboxSetEditorToolkit::MirrorSelection()
{
	UGSTN_HurtboxSetDefinition* Resolved = Set.Get();
	if (!Model.IsValid() || Resolved == nullptr)
	{
		return 0;
	}
	RefreshMirrorConfig();
	if (!MirrorConfig.bEnabled)
	{
		Notify(MirrorConfig.Reason, 8.f);
		return 0;
	}
	if (Model->GetSelection().IsEmpty())
	{
		Notify(LOCTEXT("MirrorNothingSelected", "Select the hurtboxes to mirror first."));
		return 0;
	}

	const GSTNHurtboxMirror::FPlan Plan = GSTNHurtboxMirror::MakePlan(Resolved->Hurtboxes, Model->GetSelection(), MirrorConfig, UGSTN_CombatEditorSettings::Get().MirrorForwardAxis);
	if (!Plan.Unpaired.IsEmpty())
	{
		FGSTN_ValidationReport Report;
		for (const FName Name : Plan.Unpaired)
		{
			Report.Add(EGSTN_Severity::Warning, GSTNValidationCodes::HurtboxMirror(),
				FText::Format(LOCTEXT("MirrorUnpaired", "{0} has a left or right marker but no counterpart in the skeleton, so it was not mirrored."), FText::FromName(Name)), Resolved);
		}
		GSTNValidationLog::WriteReport(Report, FText::Format(LOCTEXT("MirrorPageTitle", "Mirror hurtboxes {0}"), FText::FromString(Resolved->GetName())), true, true);
	}
	if (Plan.Entries.IsEmpty())
	{
		Notify(Plan.Centre.Num() == Model->GetSelection().Num()
			? LOCTEXT("MirrorOnlyCentre", "The selected hurtboxes have no left or right in their names, so there is nothing to mirror.")
			: LOCTEXT("MirrorNothingToDo", "Nothing was mirrored."));
		return 0;
	}
	if (Plan.NeedsConfirmation())
	{
		FString Names;
		for (const FName Name : Plan.GetOverwrittenNames(Resolved->Hurtboxes))
		{
			Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + Name.ToString();
		}
		const FText Question = FText::Format(LOCTEXT("MirrorOverwriteQuestion", "Mirroring replaces these existing hurtboxes with the mirror of the selected ones: {0}.\n\nContinue? It can be undone."), FText::FromString(Names));
		if (FMessageDialog::Open(EAppMsgType::YesNo, Question) != EAppReturnType::Yes)
		{
			return 0;
		}
	}
	const int32 Written = Model->ApplyMirror(Plan);
	Notify(FText::Format(LOCTEXT("MirrorDone", "Mirrored {0} hurtboxes ({1} could not be paired)."), FText::AsNumber(Written), FText::AsNumber(Plan.Unpaired.Num())));
	return Written;
}

void FGSTN_HurtboxSetEditorToolkit::AddHurtbox()
{
	if (!Model.IsValid())
	{
		return;
	}
	FGSTN_Hurtbox New;
	New.Name = TEXT("Hurtbox");
	New.Shape.Type = EGSTN_ShapeType::Sphere;
	New.Shape.Extent = FVector(20.0, 20.0, 20.0);
	Model->AddHurtbox(New);
}

void FGSTN_HurtboxSetEditorToolkit::RemoveSelected()
{
	if (!Model.IsValid() || Model->GetSelection().IsEmpty())
	{
		return;
	}
	const FText Question = FText::Format(LOCTEXT("RemoveQuestion", "Remove {0} hurtboxes? Hurtboxes after them move up one position. It can be undone."), FText::AsNumber(Model->GetSelection().Num()));
	if (FMessageDialog::Open(EAppMsgType::YesNo, Question) == EAppReturnType::Yes)
	{
		const TArray<int32> Selected = Model->GetSelection();
		Model->RemoveHurtboxes(Selected);
	}
}

void FGSTN_HurtboxSetEditorToolkit::DrawHurtboxes(FPrimitiveDrawInterface* PDI)
{
	if (!Model.IsValid())
	{
		return;
	}
	const TArray<FGSTN_Hurtbox>* Hurtboxes = Model->GetHurtboxes();
	if (Hurtboxes == nullptr)
	{
		return;
	}
	const TArray<int32>& Selection = Model->GetSelection();
	for (int32 Index = 0; Index < Hurtboxes->Num(); ++Index)
	{
		const bool bSelected = Selection.Contains(Index);
		const FLinearColor Color = bSelected ? FLinearColor(1.f, 0.85f, 0.1f) : FLinearColor(0.15f, 0.8f, 0.75f);
		// A click on the lines selects the hurtbox (HGSTN_HitboxProxy with Track 0 and Lane the position in the set, the hurtbox gizmo target's convention)
		const bool bHitTesting = PDI->IsHitTesting();
		if (bHitTesting)
		{
			PDI->SetHitProxy(new HGSTN_HitboxProxy(0, Index));
		}
		GSTNPreviewDrawing::DrawHitShape(PDI, (*Hurtboxes)[Index].Shape, FTransform::Identity, Color, bSelected ? 2.f : 0.f);
		if (bHitTesting)
		{
			PDI->SetHitProxy(nullptr);
		}
	}
}

void FGSTN_HurtboxSetEditorToolkit::GoToFinding(const FGSTN_ValidationFinding& Finding)
{
	// A finding about one hurtbox carries its position in Where.TrackIndex (there are no tracks in a set)
	if (Model.IsValid() && Finding.Where.TrackIndex != INDEX_NONE)
	{
		const int32 One[] = { Finding.Where.TrackIndex };
		Model->SetSelection(One);
	}
}

void FGSTN_HurtboxSetEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenuHurtboxEditor", "GSTN Hurtbox Set Editor"));
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(ViewportTabId, FOnSpawnTab::CreateSP(this, &FGSTN_HurtboxSetEditorToolkit::SpawnViewportTab))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));
	InTabManager->RegisterTabSpawner(HurtboxesTabId, FOnSpawnTab::CreateSP(this, &FGSTN_HurtboxSetEditorToolkit::SpawnHurtboxesTab))
		.SetDisplayName(LOCTEXT("HurtboxesTab", "Hurtboxes"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Collision"));
	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FGSTN_HurtboxSetEditorToolkit::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
	InTabManager->RegisterTabSpawner(FindingsTabId, FOnSpawnTab::CreateSP(this, &FGSTN_HurtboxSetEditorToolkit::SpawnFindingsTab))
		.SetDisplayName(LOCTEXT("FindingsTab", "Findings"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Validate"));
}

void FGSTN_HurtboxSetEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(ViewportTabId);
	InTabManager->UnregisterTabSpawner(HurtboxesTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(FindingsTabId);
}

FName FGSTN_HurtboxSetEditorToolkit::GetToolkitFName() const
{
	return TEXT("GSTNHurtboxSetEditor");
}

FText FGSTN_HurtboxSetEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Hurtbox Set Editor");
}

FString FGSTN_HurtboxSetEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Hurtboxes ").ToString();
}

FLinearColor FGSTN_HurtboxSetEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.15f, 0.7f, 0.65f, 0.5f);
}

void FGSTN_HurtboxSetEditorToolkit::OnClose()
{
	FAssetEditorToolkit::OnClose();
	if (Model.IsValid())
	{
		Model->EndEdit();
	}
}

TSharedRef<SDockTab> FGSTN_HurtboxSetEditorToolkit::SpawnViewportTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTabLabel", "Viewport"))
		[
			SNew(SGSTN_TimelineViewport)
			.Controller(Preview)
			.OnDrawExtra(FGSTN_ViewportDrawExtra::CreateSP(this, &FGSTN_HurtboxSetEditorToolkit::DrawHurtboxes))
			.GizmoTarget(MakeShared<FGSTN_HurtboxGizmoTarget>(Model.ToSharedRef()))
		];
}

TSharedRef<SDockTab> FGSTN_HurtboxSetEditorToolkit::SpawnHurtboxesTab(const FSpawnTabArgs& Args)
{
	const TWeakObjectPtr<UGSTN_HurtboxSetDefinition> WeakSet = Set;

	auto GenerateMenu = [this]() -> TSharedRef<SWidget>
	{
		FMenuBuilder Menu(true, nullptr);
		Menu.AddMenuEntry(LOCTEXT("GenerateKeep", "Add missing hurtboxes"),
			LOCTEXT("GenerateKeepTip", "Makes a hurtbox for every Physics Asset primitive that the set has no hurtbox of that name for. Hurtboxes already in the set are left exactly as they are."),
			FSlateIcon(), FUIAction(FExecuteAction::CreateSPLambda(this, [this]() { GenerateFromPhysicsAsset(EGSTN_RegenerateMode::KeepEdits); })));
		Menu.AddMenuEntry(LOCTEXT("GenerateUpdate", "Update shapes from the Physics Asset"),
			LOCTEXT("GenerateUpdateTip", "Also puts the Physics Asset shape and bone into the hurtboxes of the same name. Their zone tags, priority and reaction scale are kept."),
			FSlateIcon(), FUIAction(FExecuteAction::CreateSPLambda(this, [this]() { GenerateFromPhysicsAsset(EGSTN_RegenerateMode::UpdateShapes); })));
		Menu.AddMenuEntry(LOCTEXT("GenerateReplace", "Replace all"),
			LOCTEXT("GenerateReplaceTip", "Throws the whole set away and makes it again from the Physics Asset. Everything made by hand is lost (asks first)."),
			FSlateIcon(), FUIAction(FExecuteAction::CreateSPLambda(this, [this]() { GenerateFromPhysicsAsset(EGSTN_RegenerateMode::ReplaceAll); })));
		return Menu.MakeWidget();
	};

	return SNew(SDockTab)
		.Label(LOCTEXT("HurtboxesTabLabel", "Hurtboxes"))
		[
			SNew(SVerticalBox)

			// The tools
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(4.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 4.f, 0.f))
				[
					SNew(SComboButton)
					.OnGetMenuContent_Lambda(GenerateMenu)
					.ToolTipText(LOCTEXT("GenerateTip", "Make hurtboxes from the Physics Asset of the preview mesh: one per primitive, placed in root space."))
					.ButtonContent()
					[
						SNew(STextBlock).Text(LOCTEXT("Generate", "Generate from Physics Asset"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 4.f, 0.f))
				[
					SNew(SButton)
					.IsEnabled_Lambda([this]() { return MirrorConfig.bEnabled && Model.IsValid() && !Model->GetSelection().IsEmpty(); })
					.ToolTipText_Lambda([this]() { return MirrorConfig.Reason; })
					.OnClicked_Lambda([this]() -> FReply { MirrorSelection(); return FReply::Handled(); })
					[
						SNew(STextBlock).Text(LOCTEXT("Mirror", "Mirror selection"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().Padding(FMargin(0.f, 0.f, 4.f, 0.f))
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT("AddTip", "Add a small sphere at the root and select it"))
					.OnClicked_Lambda([this]() -> FReply { AddHurtbox(); return FReply::Handled(); })
					[
						SNew(STextBlock).Text(LOCTEXT("Add", "Add"))
					]
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.IsEnabled_Lambda([this]() { return Model.IsValid() && !Model->GetSelection().IsEmpty(); })
					.ToolTipText(LOCTEXT("RemoveTip", "Remove the selected hurtboxes"))
					.OnClicked_Lambda([this]() -> FReply { RemoveSelected(); return FReply::Handled(); })
					[
						SNew(STextBlock).Text(LOCTEXT("Remove", "Remove"))
					]
				]
			]

			// The mesh the hurtboxes are authored on
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(4.f, 0.f, 4.f, 4.f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(0.f, 0.f, 6.f, 0.f))
				[
					SNew(STextBlock).Text(LOCTEXT("PreviewMesh", "Preview mesh"))
					.ToolTipText(LOCTEXT("PreviewMeshTip", "The skeletal mesh this set was authored on. Saved with the set: the preview, Generate and the bone check use it. Empty: the preview subject of the timeline editor."))
				]
				+ SHorizontalBox::Slot().FillWidth(1.f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(USkeletalMesh::StaticClass())
					.AllowClear(true)
					.DisplayThumbnail(false)
					.ObjectPath_Lambda([WeakSet]() -> FString
					{
#if WITH_EDITORONLY_DATA
						return WeakSet.IsValid() ? WeakSet->PreviewMesh.ToString() : FString();
#else
						return FString();
#endif
					})
					.OnObjectChanged_Lambda([this](const FAssetData& Asset)
					{
						if (Model.IsValid())
						{
							Model->SetPreviewMesh(TSoftObjectPtr<USkeletalMesh>(Asset.GetSoftObjectPath()));
							SyncPreviewFromSet();
						}
					})
				]
			]

			// Why mirroring is off, or what it found
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(4.f, 0.f, 4.f, 4.f))
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
				.Text_Lambda([this]() { return FText::Format(LOCTEXT("MirrorStatus", "Mirror: {0}"), MirrorConfig.Reason); })
			]

			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SGSTN_HurtboxList).Model(Model)
			]
		];
}

TSharedRef<SDockTab> FGSTN_HurtboxSetEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTabLabel", "Details"))
		[
			SNew(SGSTN_HurtboxDetails).Model(Model)
		];
}

TSharedRef<SDockTab> FGSTN_HurtboxSetEditorToolkit::SpawnFindingsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("FindingsTabLabel", "Findings"))
		[
			SNew(SBorder)
			.Padding(FMargin(8.f))
			[
				SNew(SGSTN_FindingsList)
				.Store(Findings)
				.OnFindingActivated(SGSTN_FindingsList::FOnFindingActivated::CreateSP(this, &FGSTN_HurtboxSetEditorToolkit::GoToFinding))
			]
		];
}

#undef LOCTEXT_NAMESPACE
