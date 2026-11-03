// Traversal surface data contract (no GAS, no Network Prediction, no Mover).

#include "GSTNSurfaceCoreModule.h"

#include "Engine/Engine.h"
#include "GameplayTagsManager.h"
#include "GameFramework/GameModeBase.h"
#include "GameplayTagsModule.h"
#include "GSTN_SurfaceJoinCheck.h"
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
 * they are frozen by that first bake (unless it found no types: an empty table is never frozen), and a later tree change that would
 * move them is an error in the log, not a silent shift. It also checks, on the server, the type table checksum a joining client announces.
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

		// Server side of the connect-time type table check (GSTN_SurfaceJoinCheck.h): the event is static and global, so the
		// plugin needs no game mode of its own
		PostLoginHandle = FGameModeEvents::OnGameModePostLoginEvent().AddStatic(&FGSTN_SurfaceJoinCheck::CheckLogin);

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
		FGameModeEvents::OnGameModePostLoginEvent().Remove(PostLoginHandle);
		if (!TagIniPath.IsEmpty() && UObjectInitialized())
		{
			UGameplayTagsManager::Get().RemoveTagIniSearchPath(TagIniPath);
		}
	}

private:
	FDelegateHandle PostEngineInitHandle;
	FDelegateHandle TagTreeChangedHandle;
	FDelegateHandle PostLoginHandle;
	FString TagIniPath;
};

IMPLEMENT_MODULE(FGSTNSurfaceCoreModule, GSTNSurfaceCore)
