// Backpacks, Network Prediction half (M11 L7b).

#include "GSTN_BackpackSim.h"
#include "GSTN_BackpackTickExtension.h"
#include "Modules/ModuleManager.h"

/**
 * Registers the backpack fragment under GSTNFeatureIds::Backpack and the backpack tick extension before the first simulation tick, and
 * unregisters both at shutdown. An entity gets a fragment only when a backpack move starts, so a match without backpack moves pays one
 * fragment lookup per entity and tick and nothing on the wire.
 */
class FGSTNBackpackNPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_BackpackFragmentRegistration::Register();
		FGSTN_CombatTickExtensionRegistry::Get().Register(&TickExtension);
	}

	virtual void ShutdownModule() override
	{
		FGSTN_CombatTickExtensionRegistry::Get().Unregister(&TickExtension);
		FGSTN_BackpackFragmentRegistration::Unregister();
	}

private:
	FGSTN_BackpackTickExtension TickExtension;
};

IMPLEMENT_MODULE(FGSTNBackpackNPModule, GSTNBackpackNP)
