#include "GameplayTagsManager.h"
#include "Input/GSTN_CombatSettings.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"

/**
 * Contributes the combat priority settings to the input plugin while the module is loaded, and registers the plugin's own tag
 * ini folder (Config/Tags): the engine does not scan the Config/Tags of a plugin by itself.
 */
class FGSTNCombatCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GSTNCombat")))
		{
			UGameplayTagsManager::Get().AddTagIniSearchPath(Plugin->GetBaseDir() / TEXT("Config") / TEXT("Tags"));
		}

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
