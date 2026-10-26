// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

/**
 * Registers the plugin's Config/Tags folder with the tag manager: the engine reads Config/Tags only for the project and for Game
 * Feature plugins, so the example Anim.Trait.* leaves would otherwise be missing
 */
class FGSTNAnimationCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GSTNAnimation")))
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

IMPLEMENT_MODULE(FGSTNAnimationCoreModule, GSTNAnimationCore)
