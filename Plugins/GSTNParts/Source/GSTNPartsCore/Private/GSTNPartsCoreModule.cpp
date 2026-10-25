#include "GSTNPartsCoreModule.h"

#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Modules/ModuleManager.h"

void FGSTNPartsCoreModule::StartupModule()
{
	if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GSTNParts")))
	{
		UGameplayTagsManager::Get().AddTagIniSearchPath(Plugin->GetBaseDir() / TEXT("Config") / TEXT("Tags"));
	}
}

IMPLEMENT_MODULE(FGSTNPartsCoreModule, GSTNPartsCore)
