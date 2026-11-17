// GSTN Motion Tools panel: registers the docked tab Window > GSTN Animation > Motion Tools.

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Modules/ModuleManager.h"
#include "SGSTN_MotionToolsPanel.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "GSTNMotionTools"

namespace
{
	const FName MotionToolsTabId(TEXT("GSTNMotionTools"));
}

class FGSTNMotionToolsEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (!FSlateApplication::IsInitialized())
		{
			return;   // commandlets, -nullrhi runs
		}
		TabGroup = WorkspaceMenu::GetMenuStructure().GetStructureRoot()->AddGroup(
			LOCTEXT("GroupName", "GSTN Animation"), LOCTEXT("GroupTip", "Animation authoring tools of the project"));
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(MotionToolsTabId, FOnSpawnTab::CreateStatic(&SpawnTab))
			.SetDisplayName(LOCTEXT("TabName", "Motion Tools"))
			.SetTooltipText(LOCTEXT("TabTip", "Overlap e easing non distruttivi sulla Level Sequence aperta (layer additivi)."))
			.SetGroup(TabGroup.ToSharedRef())
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Sequencer"));
		bRegistered = true;
	}

	virtual void ShutdownModule() override
	{
		if (!bRegistered)
		{
			return;
		}
		bRegistered = false;
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(MotionToolsTabId);
			if (TabGroup.IsValid() && FModuleManager::Get().IsModuleLoaded("WorkspaceMenuStructure"))
			{
				WorkspaceMenu::GetMenuStructure().GetStructureRoot()->RemoveItem(TabGroup.ToSharedRef());
			}
		}
		TabGroup.Reset();
	}

private:
	static TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			.Label(LOCTEXT("TabName", "Motion Tools"))
			[
				SNew(SGSTN_MotionToolsPanel)
			];
	}

	TSharedPtr<FWorkspaceItem> TabGroup;
	bool bRegistered = false;
};

IMPLEMENT_MODULE(FGSTNMotionToolsEditorModule, GSTNMotionToolsEditor)

#undef LOCTEXT_NAMESPACE
