// Spin and drill, Network Prediction half.

#include "Modules/ModuleManager.h"
#include "Sim/GSTN_DrillFragment.h"
#include "Data/GSTN_SpinDrillTracks.h"
#include "Sim/GSTN_DrillTickExtension.h"
#include "Sim/GSTN_DrillTrackHook.h"

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
		FGSTN_TimelineTrackHookRegistry::Get().Register(FGSTN_BoreTrack::StaticStruct(), &BoreHook);
	}

	virtual void ShutdownModule() override
	{
		FGSTN_TimelineTrackHookRegistry::Get().Unregister(&BoreHook);
		FGSTN_CombatTickExtensionRegistry::Get().Unregister(&TickExtension);
		FGSTN_DrillFragmentRegistration::Unregister();
	}

private:
	FGSTN_DrillTickExtension TickExtension;
	FGSTN_BoreTrackHook BoreHook;
};

IMPLEMENT_MODULE(FGSTNSpinDrillNPModule, GSTNSpinDrillNP)
