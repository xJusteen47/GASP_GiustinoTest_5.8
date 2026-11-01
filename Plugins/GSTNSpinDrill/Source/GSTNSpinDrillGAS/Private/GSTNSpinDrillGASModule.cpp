// Spin and drill plugin, module GSTNSpinDrillGAS.

#include "GSTN_SpinDrillKitExtension.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Modules/ModuleManager.h"

/**
 * Registers the plugin's loadout extension with the combat GAS layer: a kit's FGSTN_SpinDrillKitExtension becomes the pawn's attached tuning
 * wherever the loadout is compiled. A build with the plugin disabled never registers it, so the loadout is exactly the combat plugin's.
 */
class FGSTNSpinDrillGASModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_LoadoutExtension Extension;
		Extension.ContributeCompileInput = &FGSTN_SpinDrillLoadout::AttachFromLoadout;
		FGSTN_LoadoutExtensionRegistry::Register(FGSTN_SpinDrillLoadout::GetLoadoutExtensionId(), MoveTemp(Extension));
	}

	virtual void ShutdownModule() override
	{
		FGSTN_LoadoutExtensionRegistry::Unregister(FGSTN_SpinDrillLoadout::GetLoadoutExtensionId());
	}
};

IMPLEMENT_MODULE(FGSTNSpinDrillGASModule, GSTNSpinDrillGAS)
