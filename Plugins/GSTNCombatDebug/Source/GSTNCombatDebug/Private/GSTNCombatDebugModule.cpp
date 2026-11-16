// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#include "GSTNCombatDebugModule.h"

#include "GSTNInputDebugModule.h"
#include "HAL/IConsoleManager.h"
#include "Input/GSTN_StandardSlots.h"
#include "Modules/ModuleManager.h"
#include "Sources/GSTN_CombatInputSource.h"
#include "Sources/GSTN_InputSourceRegistry.h"
#include "Trace/GSTN_CombatTraceSink.h"

#if WITH_EDITOR
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Misc/CoreDelegates.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Window/SGSTN_DebugOptionsPanel.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#endif

#if WITH_GAMEPLAY_DEBUGGER_MENU
#include "GameplayDebugger.h"
#include "GameplayDebugger/GSTN_GameplayDebuggerCategory_Combat.h"
#endif

DEFINE_LOG_CATEGORY(LogGSTNCombatDebug);

#define LOCTEXT_NAMESPACE "GSTNCombatDebug"

namespace
{
	const FName ModuleName(TEXT("GSTNCombatDebug"));

	/** The key this module registers under in the input debug registry */
	const FName RegistryKey(TEXT("GSTNCombatDebug"));

	/** The console variable the input display had while it belonged to the combat plugin, and the one that replaced it */
	const TCHAR* const OldInputDisplayVariable = TEXT("gstn.Combat.Debug.InputDisplay");
	const TCHAR* const NewInputDisplayVariable = TEXT("gstn.Input.Debug.InputDisplay");

	/** Setting the old name sets the new one, so a habit, a config file or a script that still uses it keeps working */
	void ForwardInputDisplayVariable(IConsoleVariable* Old)
	{
		if (IConsoleVariable* Target = IConsoleManager::Get().FindConsoleVariable(NewInputDisplayVariable))
		{
			Target->Set(Old->GetInt(), ECVF_SetByConsole);
		}
	}

#if WITH_EDITOR
	/** The Debug Options editor tab (Tools > Debug > GSTN Debug Options) */
	const FName DebugOptionsTabId(TEXT("GSTNCombatDebug.DebugOptions"));
	const FVector2D DebugOptionsTabSize(420.0, 640.0);
#endif

	FGSTN_InputDisplayProfile MakeCombatProfile()
	{
		FGSTN_InputDisplayProfile Profile;
		Profile.ButtonTags = FGSTN_CombatInputSource::GetCombatButtonTags();
		Profile.Slots = UGSTN_StandardSlots::Build();
		for (FGSTN_InputSlotSpec& Slot : Profile.Slots)
		{
			Slot.ApplyActiveButtonLayout(); // IO-2c: the buttons of the active layout, as the kits compile them
		}
		Profile.Label = LOCTEXT("CombatProfile", "Combat: the six standard buttons and the standard command set");
		return Profile;
	}
}

/**
 * The debug module of the combat plugin. What the input display shows of a combat character is this module's only input job now: the
 * display itself lives in GSTNInputDebug, and this module teaches it to read the combat component (a finder in its source registry),
 * gives it the combat buttons and the standard command set as its default profile, and keeps the old console variable name alive. The
 * Gameplay Debugger category (E15) is registered here and removed in ShutdownModule; the trace sink (E17, gstn.Combat.Debug.Trace) is installed and removed the same way.
 */
class FGSTN_CombatDebugModule : public IGSTN_CombatDebugModule
{
public:
	virtual void StartupModule() override
	{
		FGSTN_InputSourceRegistry& Registry = FGSTN_InputSourceRegistry::Get();
		Registry.RegisterFinder(RegistryKey, [](APawn& Pawn) { return FGSTN_CombatInputSource::Find(Pawn); });
		Registry.RegisterDefaultProfile(RegistryKey, []() { return MakeCombatProfile(); });

		OldVariable = IConsoleManager::Get().RegisterConsoleVariable(OldInputDisplayVariable, 0,
			TEXT("The older name of gstn.Input.Debug.InputDisplay (the input display moved to the input plugin): setting this sets that. 0 = off, 1 = on."), ECVF_Default);
		if (OldVariable)
		{
			OldVariable->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&ForwardInputDisplayVariable));
		}
#if WITH_GAMEPLAY_DEBUGGER_MENU
		// E15: the combat category, enabled in game and simulate at slot 6; the debugger reads its slot and state from the ini when set there
		IGameplayDebugger& Debugger = IGameplayDebugger::Get();
		Debugger.RegisterCategory(FGSTN_GameplayDebuggerCategory_Combat::CategoryName,
			IGameplayDebugger::FOnGetCategory::CreateStatic(&FGSTN_GameplayDebuggerCategory_Combat::MakeInstance),
			EGameplayDebuggerCategoryState::EnabledInGameAndSimulate, FGSTN_GameplayDebuggerCategory_Combat::DefaultSlot);
		Debugger.NotifyCategoriesChanged();
#endif
		// E17: gstn.Combat.Debug.Trace installs the trace sink while it is 1
		FGSTN_CombatTraceController::Startup();
#if WITH_EDITOR
		// The Debug Options window: an editor tab, like the options window of the Game Animation Sample, instead of anything drawn over the game.
		// The tab manager exists once the engine is initialised; a module of the Default phase loads before that in some start-ups
		if (GEngine)
		{
			RegisterDebugOptionsTab();
		}
		else
		{
			PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FGSTN_CombatDebugModule::RegisterDebugOptionsTab);
		}
#endif
		UE_LOG(LogGSTNCombatDebug, Verbose, TEXT("GSTN Combat Debug module started"));
	}

	virtual void ShutdownModule() override
	{
#if WITH_EDITOR
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
		PostEngineInitHandle.Reset();
		UnregisterDebugOptionsTab();
#endif
		FGSTN_CombatTraceController::Shutdown();
#if WITH_GAMEPLAY_DEBUGGER_MENU
		if (IGameplayDebugger::IsAvailable())
		{
			IGameplayDebugger& Debugger = IGameplayDebugger::Get();
			Debugger.UnregisterCategory(FGSTN_GameplayDebuggerCategory_Combat::CategoryName);
			Debugger.NotifyCategoriesChanged();
		}
#endif
		if (OldVariable)
		{
			IConsoleManager::Get().UnregisterConsoleObject(OldVariable, false);
			OldVariable = nullptr;
		}
		FGSTN_InputSourceRegistry& Registry = FGSTN_InputSourceRegistry::Get();
		Registry.UnregisterFinder(RegistryKey);
		Registry.UnregisterDefaultProfile(RegistryKey);
	}

	virtual TSharedPtr<SWidget> CreateInputDisplay() override
	{
		// The display is the input plugin's; the combat finder and profile are already in its registry
		IGSTN_InputDebugModule* Input = IGSTN_InputDebugModule::Get();
		return Input ? Input->CreateInputDisplay() : nullptr;
	}

private:
#if WITH_EDITOR
	void RegisterDebugOptionsTab()
	{
		if (OpenCommand == nullptr)
		{
			// The command works in any editor start-up, with or without Slate, so a script or the editor MCP can open the window too
			OpenCommand = IConsoleManager::Get().RegisterConsoleCommand(TEXT("gstn.Debug.OpenWindow"),
				TEXT("Opens the GSTN Debug Options editor window (Tools > Debug > GSTN Debug Options): every gstn.*Debug* console variable as a switch."),
				FConsoleCommandDelegate::CreateLambda([]()
				{
					if (FSlateApplication::IsInitialized())
					{
						FGlobalTabmanager::Get()->TryInvokeTab(FTabId(DebugOptionsTabId));
					}
				}), ECVF_Default);
		}
		if (!FSlateApplication::IsInitialized() || bTabRegistered)
		{
			return;
		}
		bTabRegistered = true;

		// Tools > Debug, next to the engine's own debug windows: where an Unreal user looks for a debug panel
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(DebugOptionsTabId, FOnSpawnTab::CreateRaw(this, &FGSTN_CombatDebugModule::SpawnDebugOptionsTab))
			.SetDisplayName(LOCTEXT("DebugOptionsTab", "GSTN Debug Options"))
			.SetTooltipText(LOCTEXT("DebugOptionsTabTip", "Every debug switch of the GSTN plugins (hitboxes, hurtboxes, input display, minions, trace...) in one window, next to the viewport instead of over the game."))
			.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsDebugCategory())
			.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings"));
		FTabManager::RegisterDefaultTabWindowSize(FTabId(DebugOptionsTabId), DebugOptionsTabSize);
	}

	void UnregisterDebugOptionsTab()
	{
		if (OpenCommand != nullptr)
		{
			IConsoleManager::Get().UnregisterConsoleObject(OpenCommand);
			OpenCommand = nullptr;
		}
		if (!bTabRegistered)
		{
			return;
		}
		bTabRegistered = false;
		if (FSlateApplication::IsInitialized())
		{
			FTabManager::UnregisterDefaultTabWindowSize(FTabId(DebugOptionsTabId));
			FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DebugOptionsTabId);
		}
	}

	TSharedRef<SDockTab> SpawnDebugOptionsTab(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			.Label(LOCTEXT("DebugOptionsTab", "GSTN Debug Options"))
			[
				SNew(SGSTN_DebugOptionsPanel)
			];
	}

	FDelegateHandle PostEngineInitHandle;
	IConsoleCommand* OpenCommand = nullptr;
	bool bTabRegistered = false;
#endif

	IConsoleVariable* OldVariable = nullptr;
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
