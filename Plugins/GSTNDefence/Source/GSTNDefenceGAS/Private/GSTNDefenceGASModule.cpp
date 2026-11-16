// Defence, ability-system half (no Network Prediction, no Mover).

#include "GSTN_DefenceLibrary.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Modules/ModuleManager.h"

/**
 * Registers the plugin's loadout extension with the combat GAS layer: what a kit's defence extension adds to a loadout (the shield
 * binding where the shield is available, the get-up binding, the Forced-started abilities) and the attribute set granted with it.
 * A build with the plugin disabled never registers it, so the loadout is exactly M3's.
 */
class FGSTNDefenceGASModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_LoadoutExtension Extension;
		Extension.ContributeCompileInput = &UGSTN_DefenceLibrary::ContributeToLoadout;
		Extension.OnGranted = &UGSTN_DefenceLibrary::GrantDefence;
		Extension.OnCleared = &UGSTN_DefenceLibrary::RevokeDefence;
		FGSTN_LoadoutExtensionRegistry::Register(UGSTN_DefenceLibrary::GetLoadoutExtensionId(), MoveTemp(Extension));
	}

	virtual void ShutdownModule() override
	{
		FGSTN_LoadoutExtensionRegistry::Unregister(UGSTN_DefenceLibrary::GetLoadoutExtensionId());
	}
};

IMPLEMENT_MODULE(FGSTNDefenceGASModule, GSTNDefenceGAS)
