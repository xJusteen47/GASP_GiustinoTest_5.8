// Kit mods, ability layer: registers the loadout extension (compile input, rules view, stat effects).

#include "GSTN_KitModChoices.h"
#include "GSTN_KitModLoadout.h"
#include "Loadout/GSTN_LoadoutChoices.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Modules/ModuleManager.h"

class FGSTNKitModsGASModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_LoadoutExtension Extension;
		Extension.Priority = FGSTN_KitModLoadout::ExtensionPriority;
		Extension.ContributeCompileInput = &FGSTN_KitModLoadout::ContributeCompileInput;
		Extension.ContributeRulesView = &FGSTN_KitModLoadout::ContributeRulesView;
		Extension.OnGranted = &FGSTN_KitModLoadout::OnGranted;
		Extension.OnCleared = &FGSTN_KitModLoadout::OnCleared;
		FGSTN_LoadoutExtensionRegistry::Register(FGSTN_KitModLoadout::ExtensionId, MoveTemp(Extension));
		// The loadout screen's mod rows (M11): mods are chosen before the match, per kit
		FGSTN_LoadoutChoiceRegistry::Register(FGSTN_KitModChoices::ProviderId, FGSTN_KitModChoices::ProviderPriority, &FGSTN_KitModChoices::Gather);
	}

	virtual void ShutdownModule() override
	{
		FGSTN_LoadoutExtensionRegistry::Unregister(FGSTN_KitModLoadout::ExtensionId);
		FGSTN_LoadoutChoiceRegistry::Unregister(FGSTN_KitModChoices::ProviderId);
		FGSTN_KitModLoadout::ResetDerivedCache();
	}
};

IMPLEMENT_MODULE(FGSTNKitModsGASModule, GSTNKitModsGAS);
