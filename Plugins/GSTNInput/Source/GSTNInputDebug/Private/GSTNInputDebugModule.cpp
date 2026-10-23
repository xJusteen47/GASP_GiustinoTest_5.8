// Development tooling of GSTN Input (PIE and development builds, never Shipping).

#include "GSTNInputDebugModule.h"

#include "Framework/Application/SlateApplication.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "Overlay/GSTN_InputDisplayOverlay.h"
#include "Sources/GSTN_LocalInputSource.h"
#include "Widgets/SGSTN_InputDisplay.h"

#if WITH_EDITOR
#include "Framework/Docking/TabManager.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#endif

DEFINE_LOG_CATEGORY(LogGSTNInputDebug);

#define LOCTEXT_NAMESPACE "GSTNInputDebug"

namespace
{
	const FName ModuleName(TEXT("GSTNInputDebug"));

#if WITH_EDITOR
	const FName InputDisplayTabId(TEXT("GSTNInputDebug.InputDisplay"));

	/** The first size of the tab's window: wide enough for forty columns and the rows under them */
	const FVector2D InputDisplayTabSize(1100.0, 340.0);
#endif

	/** The strip fed by the local players of every PIE or game world, with the script box and the player picker */
	TSharedRef<SGSTN_InputDisplay> MakeInputDisplay()
	{
		return SNew(SGSTN_InputDisplay)
			.LocalSource(MakeShared<FGSTN_LocalInputSource>())
			.bShowScriptBox(true)
			.bShowPlayerPicker(true);
	}
}

/**
 * The input debug module. Startup registers the corner overlay of the input display (it only reads its console variable until that is set)
 * and, in an editor, the nomad tab Window > GSTN Input > Input Display. The tab belongs to this module on purpose: the editor plugins do not
 * know it exists, so with this plugin off the tab is simply not there and they build and run the same.
 */
class FGSTN_InputDebugModule : public IGSTN_InputDebugModule
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
			PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FGSTN_InputDebugModule::RegisterInputDisplayTab);
		}
#endif
		UE_LOG(LogGSTNInputDebug, Verbose, TEXT("GSTN Input Debug module started"));
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

		TabGroup = WorkspaceMenu::GetMenuStructure().GetStructureRoot()->AddGroup(LOCTEXT("GroupName", "GSTN Input"), LOCTEXT("GroupTip", "Tools of GSTN Input"));
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(InputDisplayTabId, FOnSpawnTab::CreateRaw(this, &FGSTN_InputDebugModule::SpawnInputDisplayTab))
			.SetDisplayName(LOCTEXT("InputDisplayTab", "Input Display"))
			.SetTooltipText(LOCTEXT("InputDisplayTabTip", "The input history of the local player in a play session (buttons, stick, frame gaps, commands), and a box to try a scripted input."))
			.SetGroup(TabGroup.ToSharedRef())
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.UserDefinedStruct"));
		FTabManager::RegisterDefaultTabWindowSize(FTabId(InputDisplayTabId), InputDisplayTabSize);
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
			FTabManager::UnregisterDefaultTabWindowSize(FTabId(InputDisplayTabId));
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
		// The strip takes the width the window gives it: more columns fit in a wider window, never narrower than 18 px each
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			.Label(LOCTEXT("InputDisplayTab", "Input Display"))
			[
				SNew(SBox)
				.Padding(8.f)
				[
					MakeInputDisplay()
				]
			];
	}

	TSharedPtr<FWorkspaceItem> TabGroup;
	FDelegateHandle PostEngineInitHandle;
	bool bTabRegistered = false;
#endif

	FGSTN_InputDisplayOverlay Overlay;
};

IGSTN_InputDebugModule* IGSTN_InputDebugModule::Get()
{
	return FModuleManager::GetModulePtr<IGSTN_InputDebugModule>(ModuleName);
}

bool IGSTN_InputDebugModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(ModuleName);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGSTN_InputDebugModule, GSTNInputDebug)
