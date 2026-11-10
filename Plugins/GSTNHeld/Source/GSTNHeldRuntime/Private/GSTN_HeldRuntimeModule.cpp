// Held objects, world side (needs the GAS-NPP fork).

#include "GSTN_HeldSim.h"
#include "Modules/ModuleManager.h"
#include "Simulation/GSTN_CombatTickExtension.h"

/** Registers the held fragment, its inbox kind and the tag projection before the first simulation tick, and removes them at shutdown */
class FGSTNHeldRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_HeldInbox::Register();
		FGSTN_CombatTickExtensionRegistry::Get().Register(&TickExtension);
	}

	virtual void ShutdownModule() override
	{
		FGSTN_CombatTickExtensionRegistry::Get().Unregister(&TickExtension);
		FGSTN_HeldInbox::Unregister();
	}

private:
	FGSTN_HeldTickExtension TickExtension;
};

IMPLEMENT_MODULE(FGSTNHeldRuntimeModule, GSTNHeldRuntime);
