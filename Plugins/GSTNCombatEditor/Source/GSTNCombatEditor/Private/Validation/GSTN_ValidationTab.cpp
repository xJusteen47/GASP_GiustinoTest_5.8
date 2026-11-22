// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#include "Validation/GSTN_ValidationTab.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "GSTNCombatEditorModule.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Validation/GSTN_ValidateAll.h"
#include "Validation/GSTN_ValidationMessageLog.h"
#include "Validation/SGSTN_ValidationPanel.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "GSTNCombatEditor"

namespace
{
	TSharedPtr<FWorkspaceItem> TabGroup;
	bool bTabRegistered = false;
	IConsoleCommand* ValidateAllCommand = nullptr;

	TSharedRef<SDockTab> SpawnValidationTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			.Label(LOCTEXT("ValidationTab", "Validation"))
			[
				SNew(SGSTN_ValidationPanel)
			];
	}
}

FName GSTNValidationTab::GetTabId()
{
	return FName(TEXT("GSTNCombatValidation"));
}

void GSTNValidationTab::Register()
{
	ValidateAllCommand = IConsoleManager::Get().RegisterConsoleCommand(TEXT("gstn.Combat.ValidateAll"),
		TEXT("Validates every combat asset of the project (timelines, ability Blueprints, hurtbox sets, kits, weapons, loadouts) and writes the findings to the Message Log. Optional argument: a package path prefix such as /Game/Combat."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&GSTNValidationTab::RunValidateAllCommand), ECVF_Default);

	if (!FSlateApplication::IsInitialized() || bTabRegistered)
	{
		return;
	}
	bTabRegistered = true;

	TabGroup = WorkspaceMenu::GetMenuStructure().GetStructureRoot()->AddGroup(LOCTEXT("GroupName", "GSTN Combat"), LOCTEXT("GroupTip", "Tools of GSTN Combat"));
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(GetTabId(), FOnSpawnTab::CreateStatic(&SpawnValidationTab))
		.SetDisplayName(LOCTEXT("ValidationTab", "Validation"))
		.SetTooltipText(LOCTEXT("ValidationTabTip", "The findings of the combat validators for every timeline, ability, hurtbox set, kit, weapon and loadout of the project."))
		.SetGroup(TabGroup.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Validate"));
}

void GSTNValidationTab::Unregister()
{
	if (ValidateAllCommand != nullptr)
	{
		IConsoleManager::Get().UnregisterConsoleObject(ValidateAllCommand);
		ValidateAllCommand = nullptr;
	}
	if (!bTabRegistered)
	{
		return;
	}
	bTabRegistered = false;
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GetTabId());
		if (TabGroup.IsValid() && FModuleManager::Get().IsModuleLoaded("WorkspaceMenuStructure"))
		{
			WorkspaceMenu::GetMenuStructure().GetStructureRoot()->RemoveItem(TabGroup.ToSharedRef());
		}
	}
	TabGroup.Reset();
}

void GSTNValidationTab::RunValidateAllCommand(const TArray<FString>& Args)
{
	GSTNValidateAll::FOptions Options;
	if (Args.Num() > 0)
	{
		Options.PathPrefix = Args[0];
	}
	// A progress dialog needs a window; a commandlet runs without one
	Options.bShowProgress = FSlateApplication::IsInitialized();

	const GSTNValidateAll::FResult Result = GSTNValidateAll::Run(Options);
	GSTNValidationLog::WriteReport(Result.Flatten(), LOCTEXT("ValidateAllPage", "Validate all"), false, false);
	UE_LOG(LogGSTNCombatEditor, Display, TEXT("Validate all: %d assets checked, %d errors, %d warnings, %d notes."), Result.GetCheckedCount(),
		Result.CountSeverity(EGSTN_Severity::Error), Result.CountSeverity(EGSTN_Severity::Warning), Result.CountSeverity(EGSTN_Severity::Info));
}

#undef LOCTEXT_NAMESPACE
