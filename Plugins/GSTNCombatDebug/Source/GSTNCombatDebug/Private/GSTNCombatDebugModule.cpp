// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#include "GSTNCombatDebugModule.h"

#include "Framework/Application/SlateApplication.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "Overlay/GSTN_InputDisplayOverlay.h"
#include "Widgets/GSTN_LocalInputSource.h"
#include "Widgets/SGSTN_InputDisplay.h"

#if WITH_EDITOR
#include "Framework/Docking/TabManager.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#endif

DEFINE_LOG_CATEGORY(LogGSTNCombatDebug);

#define LOCTEXT_NAMESPACE "GSTNCombatDebug"

namespace
{
	const FName ModuleName(TEXT("GSTNCombatDebug"));

#if WITH_EDITOR
	const FName InputDisplayTabId(TEXT("GSTNCombatDebug.InputDisplay"));
#endif

	/** The strip fed by the local player of whichever PIE or game world has one */
	TSharedRef<SGSTN_InputDisplay> MakeInputDisplay()
	{
		const TSharedRef<FGSTN_LocalInputSource> Source = MakeShared<FGSTN_LocalInputSource>();
		return SNew(SGSTN_InputDisplay)
			.Fetch(FGSTN_InputDisplayFetch::CreateLambda([Source](FGSTN_InputDisplayData& OutData) { return Source->Fetch(OutData); }))
			.bShowScriptBox(true);
	}
}

/**
 * The debug module. Startup registers the corner overlay of the input display (it only reads its console variable until that is
 * set) and, in an editor, the nomad tab Window > GSTN Combat > Input Display. The tab belongs to this module on purpose: the editor
 * plugin does not know the debug plugin exists, so with the debug plugin off the tab is simply not there and the editor builds and
 * runs the same. The Gameplay Debugger category, the debug draw variables and the trace sink arrive with E15 to E17, each registered
 * here and removed in ShutdownModule.
 */
class FGSTN_CombatDebugModule : public IGSTN_CombatDebugModule
{
public:
	virtual void StartupModule() override
	{
		Overlay.Start();

#if WITH_EDITOR
		// The tab manager exists once the engine is initialised; a module of the Default phase loads before that in some start-ups
		if (GEngine)
		{
			RegisterInputDisplayTab();
		}
		else
		{
			PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FGSTN_CombatDebugModule::RegisterInputDisplayTab);
		}
#endif
		UE_LOG(LogGSTNCombatDebug, Verbose, TEXT("GSTN Combat Debug module started"));
	}

	virtual void ShutdownModule() override
	{
		Overlay.Stop();

#if WITH_EDITOR
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
		UnregisterInputDisplayTab();
#endif
	}

	virtual TSharedPtr<SWidget> CreateInputDisplay() override
	{
		// Slate needs an application (not in a commandlet or on a dedicated server): asking there gets the "absent" answer
		if (!FSlateApplication::IsInitialized())
		{
			return nullptr;
		}
		return MakeInputDisplay();
	}

private:
#if WITH_EDITOR
	void RegisterInputDisplayTab()
	{
		if (!FSlateApplication::IsInitialized() || bTabRegistered)
		{
			return;
		}
		bTabRegistered = true;

		TabGroup = WorkspaceMenu::GetMenuStructure().GetStructureRoot()->AddGroup(LOCTEXT("GroupName", "GSTN Combat"), LOCTEXT("GroupTip", "Tools of GSTN Combat"));
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(InputDisplayTabId, FOnSpawnTab::CreateRaw(this, &FGSTN_CombatDebugModule::SpawnInputDisplayTab))
			.SetDisplayName(LOCTEXT("InputDisplayTab", "Input Display"))
			.SetTooltipText(LOCTEXT("InputDisplayTabTip", "The input history of the local player in a play session (buttons, stick, frame gaps, commands), and a box to try a scripted input."))
			.SetGroup(TabGroup.ToSharedRef())
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.UserDefinedStruct"));
	}

	void UnregisterInputDisplayTab()
	{
		if (!bTabRegistered)
		{
			return;
		}
		bTabRegistered = false;
		if (FSlateApplication::IsInitialized())
		{
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(InputDisplayTabId);
			if (TabGroup.IsValid() && FModuleManager::Get().IsModuleLoaded("WorkspaceMenuStructure"))
			{
				WorkspaceMenu::GetMenuStructure().GetStructureRoot()->RemoveItem(TabGroup.ToSharedRef());
			}
		}
		TabGroup.Reset();
	}

	TSharedRef<SDockTab> SpawnInputDisplayTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			.Label(LOCTEXT("InputDisplayTab", "Input Display"))
			[
				MakeInputDisplay()
			];
	}

	TSharedPtr<FWorkspaceItem> TabGroup;
	FDelegateHandle PostEngineInitHandle;
	bool bTabRegistered = false;
#endif

	FGSTN_InputDisplayOverlay Overlay;
};

IGSTN_CombatDebugModule* IGSTN_CombatDebugModule::Get()
{
	return FModuleManager::GetModulePtr<IGSTN_CombatDebugModule>(ModuleName);
}

bool IGSTN_CombatDebugModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(ModuleName);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGSTN_CombatDebugModule, GSTNCombatDebug)
