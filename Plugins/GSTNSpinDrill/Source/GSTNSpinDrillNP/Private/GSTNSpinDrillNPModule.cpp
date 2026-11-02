// Spin and drill, Network Prediction half.

#include "Modules/ModuleManager.h"
#include "Sim/GSTN_DrillFragment.h"
#include "Sim/GSTN_DrillTickExtension.h"

/**
 * Registers the drill fragment under GSTNFeatureIds::SpinDrill and the drill tick extension before the first simulation tick, and
 * unregisters both at shutdown. A machine that has the plugin reads and writes the fragment and a machine without it skips the bytes
 * (the fragment signature refuses a client whose plugin set differs). An entity gets a fragment only when something starts a spin,
 * so a project that enables the plugin and binds no spin pays one fragment lookup per entity and tick and nothing on the wire.
 */
class FGSTNSpinDrillNPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_DrillFragmentRegistration::Register();
		FGSTN_CombatTickExtensionRegistry::Get().Register(&TickExtension);
	}

	virtual void ShutdownModule() override
	{
		FGSTN_CombatTickExtensionRegistry::Get().Unregister(&TickExtension);
		FGSTN_DrillFragmentRegistration::Unregister();
	}

private:
	FGSTN_DrillTickExtension TickExtension;
};

IMPLEMENT_MODULE(FGSTNSpinDrillNPModule, GSTNSpinDrillNP)
