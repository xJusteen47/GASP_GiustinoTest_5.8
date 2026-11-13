// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#include "GSTNPartsCoreModule.h"

#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGSTNParts);

/**
 * Registers the plugin's Config/Tags folder with the tag manager. The engine reads Config/Tags only for the project and for
 * Game Feature plugins, not for ordinary plugins, so the Part.Type.* and GameplayCue.Combat.Part.* leaves (designer
 * vocabulary, ini only) would otherwise be missing.
 */
class FGSTNPartsCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GSTNParts")))
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

IMPLEMENT_MODULE(FGSTNPartsCoreModule, GSTNPartsCore)
