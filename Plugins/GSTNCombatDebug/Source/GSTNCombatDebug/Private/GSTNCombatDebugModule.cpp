// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#include "GSTNCombatDebugModule.h"

#include "GSTNInputDebugModule.h"
#include "HAL/IConsoleManager.h"
#include "Input/GSTN_StandardSlots.h"
#include "Modules/ModuleManager.h"
#include "Sources/GSTN_CombatInputSource.h"
#include "Sources/GSTN_InputSourceRegistry.h"

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

	FGSTN_InputDisplayProfile MakeCombatProfile()
	{
		FGSTN_InputDisplayProfile Profile;
		Profile.ButtonTags = FGSTN_CombatInputSource::GetCombatButtonTags();
		Profile.Slots = UGSTN_StandardSlots::Build();
		Profile.Label = LOCTEXT("CombatProfile", "Combat: the six standard buttons and the standard command set");
		return Profile;
	}
}

/**
 * The debug module of the combat plugin. What the input display shows of a combat character is this module's only input job now: the
 * display itself lives in GSTNInputDebug, and this module teaches it to read the combat component (a finder in its source registry),
 * gives it the combat buttons and the standard command set as its default profile, and keeps the old console variable name alive. The
 * Gameplay Debugger category, the debug draw variables and the trace sink arrive with E15 to E17, each registered here and removed in
 * ShutdownModule.
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
		UE_LOG(LogGSTNCombatDebug, Verbose, TEXT("GSTN Combat Debug module started"));
	}

	virtual void ShutdownModule() override
	{
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
