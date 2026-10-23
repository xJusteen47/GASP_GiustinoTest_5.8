// Development tooling of GSTN Combat (PIE and development builds, never Shipping).

#include "GSTNCombatDebugModule.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGSTNCombatDebug);

namespace
{
	const FName ModuleName(TEXT("GSTNCombatDebug"));
}

/**
 * The debug module. Startup registers nothing yet: the Gameplay Debugger category, the debug draw console variables and the
 * trace sink arrive with E14 to E17, each registered here and removed in ShutdownModule.
 */
class FGSTN_CombatDebugModule : public IGSTN_CombatDebugModule
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogGSTNCombatDebug, Verbose, TEXT("GSTN Combat Debug module started"));
	}

	virtual void ShutdownModule() override
	{
	}

	virtual TSharedPtr<SWidget> CreateInputDisplay() override
	{
		// E14
		return nullptr;
	}
};

IGSTN_CombatDebugModule* IGSTN_CombatDebugModule::Get()
{
	return FModuleManager::GetModulePtr<IGSTN_CombatDebugModule>(ModuleName);
}

bool IGSTN_CombatDebugModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded(ModuleName);
}

IMPLEMENT_MODULE(FGSTN_CombatDebugModule, GSTNCombatDebug)
