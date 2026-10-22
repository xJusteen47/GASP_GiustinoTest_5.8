#include "Input/GSTN_CombatSettings.h"
#include "Modules/ModuleManager.h"

/** Contributes the combat priority settings to the input plugin while the module is loaded. */
class FGSTNCombatCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		// The settings object exists and has read its config by now: its first access happens here
		UGSTN_CombatSettings::Get().PushToInputSettings();
	}

	virtual void ShutdownModule() override
	{
		if (UObjectInitialized())
		{
			UGSTN_CombatSettings::RemoveFromInputSettings();
		}
	}
};

IMPLEMENT_MODULE(FGSTNCombatCoreModule, GSTNCombatCore)
