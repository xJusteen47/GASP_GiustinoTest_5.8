#include "GameplayTagsManager.h"
#include "Input/GSTN_CombatSettings.h"
#include "Loadout/GSTN_LoadoutInputFeel.h"
#include "Loadout/GSTN_LoadoutInputProfile.h"
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

		// The player's input feel (stick zones, flick window) travels in the loadout under a stable id
		FGSTN_LoadoutExtensionTypes::Register(FGSTN_LoadoutInputFeelExt::WireId, FGSTN_LoadoutInputFeelExt::StaticStruct());
		// The player's input profile (device, remapped functions, picked commands): the F1 handshake
		FGSTN_LoadoutExtensionTypes::Register(FGSTN_LoadoutInputProfileExt::WireId, FGSTN_LoadoutInputProfileExt::StaticStruct());
	}

	virtual void ShutdownModule() override
	{
		FGSTN_LoadoutExtensionTypes::Unregister(FGSTN_LoadoutInputFeelExt::WireId);
		FGSTN_LoadoutExtensionTypes::Unregister(FGSTN_LoadoutInputProfileExt::WireId);
		if (UObjectInitialized())
		{
			UGSTN_CombatSettings::RemoveFromInputSettings();
		}
	}
};

IMPLEMENT_MODULE(FGSTNCombatCoreModule, GSTNCombatCore)
