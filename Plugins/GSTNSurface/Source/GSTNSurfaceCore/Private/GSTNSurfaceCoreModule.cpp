// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#include "GSTNSurfaceCoreModule.h"

#include "Engine/Engine.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsModule.h"
#include "GSTN_SurfaceSettings.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGSTNSurface);

/**
 * Registers the plugin's Config/Tags folder with the tag manager (the engine reads Config/Tags only for the project and for
 * Game Feature plugins, not for ordinary plugins, so the Surface.Type.* leaves would otherwise be missing). Then bakes the
 * surface table once the engine has started (content can be loaded, the tag tree is complete), so the first lookup inside the
 * simulation does not load assets. In the editor the type indices are rebuilt when the tag tree changes; in the running game
 * they are frozen by that first bake, and a later tree change that would move them is an error in the log, not a silent shift.
 */
class FGSTNSurfaceCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GSTNSurface")))
		{
			TagIniPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config"), TEXT("Tags"));
			UGameplayTagsManager::Get().AddTagIniSearchPath(TagIniPath);
		}

		if (GEngine && GEngine->IsInitialized())
		{
			UGSTN_SurfaceSettings::GetMutable().Rebake();
		}
		else
		{
			PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddLambda([]()
			{
				UGSTN_SurfaceSettings::GetMutable().Rebake();
			});
		}

		TagTreeChangedHandle = IGameplayTagsModule::OnGameplayTagTreeChanged.AddLambda([]()
		{
			// Types only: in the editor a tag added by a designer must get an index, and nothing here may load assets. In the
			// running game the table is frozen after engine init: a tree change that moves the indices is logged as an error
			if (UObjectInitialized())
			{
				UGSTN_SurfaceSettings::GetMutable().RebakeTypes();
			}
		});
	}

	virtual void ShutdownModule() override
	{
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
		IGameplayTagsModule::OnGameplayTagTreeChanged.Remove(TagTreeChangedHandle);
		if (!TagIniPath.IsEmpty() && UObjectInitialized())
		{
			UGameplayTagsManager::Get().RemoveTagIniSearchPath(TagIniPath);
		}
	}

private:
	FDelegateHandle PostEngineInitHandle;
	FDelegateHandle TagTreeChangedHandle;
	FString TagIniPath;
};

IMPLEMENT_MODULE(FGSTNSurfaceCoreModule, GSTNSurfaceCore)
