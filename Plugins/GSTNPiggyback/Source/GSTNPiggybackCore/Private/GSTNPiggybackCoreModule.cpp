// Piggyback, pure part (no GAS, no Network Prediction, no Mover).

#include "GSTNPiggybackCoreModule.h"

#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGSTNPiggyback);

/**
 * Registers the plugin's Config/Tags folder with the tag manager. The engine reads Config/Tags only for the project and for Game Feature
 * plugins, not for ordinary plugins, so the Command.Link.Mount/Dismount/Drop slots (designer tags, ini only) would otherwise be missing.
 * Registering it here, and not in a project ini, keeps the plugin usable alone: disabled, none of its tags exists.
 */
class FGSTNPiggybackCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GSTNPiggyback")))
		{
			TagIniPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config"), TEXT("Tags"));
			UGameplayTagsManager::Get().AddTagIniSearchPath(TagIniPath);
		}
	}

	virtual void ShutdownModule() override
	{
		if (!TagIniPath.IsEmpty() && UObjectInitialized())
		{
			UGameplayTagsManager::Get().RemoveTagIniSearchPath(TagIniPath);
		}
	}

private:
	FString TagIniPath;
};

IMPLEMENT_MODULE(FGSTNPiggybackCoreModule, GSTNPiggybackCore)
