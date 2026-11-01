// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "GSTNLinkCoreModule.h"

#include "Data/GSTN_LinkTypeDefinition.h"
#include "Engine/AssetManager.h"
#include "GSTN_LinkSettings.h"
#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGSTNLink);

namespace GSTNLinkCoreModule
{
	/**
	 * Registers the GSTN_LinkType primary asset type over the plugin's content and the settings' LinkTypeDirectories, always cooked.
	 * Done here rather than in the project's DefaultGame.ini because an ini entry names this plugin's class and the engine ensures on
	 * an unloadable base class whenever the plugin is disabled (it is off by default). A project that still lists the type in its ini
	 * must use the same base class and flags (no Blueprint classes, not editor only), or the asset manager reports a mismatch.
	 */
	void RegisterLinkTypeAssets()
	{
		UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
		if (AssetManager == nullptr)
		{
			return;
		}

		TArray<FString> Paths;
		// The plugin's mount exists only once the plugin ships a Content folder; scanning an unmounted root would only log noise
		if (FPackageName::MountPointExists(FStringView(TEXT("/GSTNLink/"))))
		{
			Paths.Add(TEXT("/GSTNLink"));
		}
		for (const FDirectoryPath& Directory : UGSTN_LinkSettings::Get().LinkTypeDirectories)
		{
			if (!Directory.Path.IsEmpty())
			{
				Paths.AddUnique(Directory.Path);
			}
		}

		const FPrimaryAssetType Type = GSTNLinkAssetTypes::LinkType();
		AssetManager->ScanPathsForPrimaryAssets(Type, Paths, UGSTN_LinkTypeDefinition::StaticClass(), false, false, true);

		FPrimaryAssetRules Rules;
		Rules.CookRule = EPrimaryAssetCookRule::AlwaysCook;
		AssetManager->SetPrimaryAssetTypeRules(Type, Rules);
	}
}

/**
 * Registers the plugin's Config/Tags folder with the tag manager. The engine reads Config/Tags only for the project and for
 * Game Feature plugins, not for ordinary plugins, so the GameplayCue.Combat.* leaves (designer vocabulary, ini only) would
 * otherwise be missing. Also registers the link type primary asset type once the asset manager exists (it is created after the
 * modules of the Default phase start), so the registry of the GAS module finds every definition.
 */
class FGSTNLinkCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GSTNLink")))
		{
			TagIniPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config"), TEXT("Tags"));
			UGameplayTagsManager::Get().AddTagIniSearchPath(TagIniPath);
		}

		UAssetManager::CallOrRegister_OnAssetManagerCreated(FSimpleMulticastDelegate::FDelegate::CreateStatic(&GSTNLinkCoreModule::RegisterLinkTypeAssets));

		// The link command tier, beside the combat plugin's tiers (the settings object has read its config by now: its first access is here)
		UGSTN_LinkSettings::Get().PushToInputSettings();
	}

	virtual void ShutdownModule() override
	{
		if (UObjectInitialized())
		{
			UGSTN_LinkSettings::RemoveFromInputSettings();
		}
		if (!TagIniPath.IsEmpty() && UObjectInitialized())
		{
			UGameplayTagsManager::Get().RemoveTagIniSearchPath(TagIniPath);
		}
	}

private:
	FString TagIniPath;
};

IMPLEMENT_MODULE(FGSTNLinkCoreModule, GSTNLinkCore)
