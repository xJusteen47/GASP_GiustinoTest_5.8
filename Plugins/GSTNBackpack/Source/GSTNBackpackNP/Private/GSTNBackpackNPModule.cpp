// Backpacks, Network Prediction half (M11 L7b, L7c).

#include "GSTN_BackpackDomeOutcome.h"
#include "GSTN_BackpackDomeSnapshot.h"
#include "GSTN_BackpackSim.h"
#include "GSTN_BackpackTickExtension.h"
#include "Hits/GSTN_HitExtensions.h"
#include "Modules/ModuleManager.h"

/**
 * Registers the backpack fragment under GSTNFeatureIds::Backpack and the backpack tick extension before the first simulation tick, and
 * unregisters both at shutdown. An entity gets a fragment only when a backpack move starts, so a match without backpack moves pays one
 * fragment lookup per entity and tick and nothing on the wire.
 *
 * M11 L7c, the dome blocks hits: the snapshot contributor (the dome, its shell and who stands in it), the attacker outcome Backpack.Dome
 * (a blocked melee hit recoils) and the event applier of the block. The BackpackDome step itself enters a rules asset through
 * UGSTN_BackpackContentLibrary::AddDomeStepToRules; without it the dome is only state.
 */
class FGSTNBackpackNPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_BackpackFragmentRegistration::Register();
		FGSTN_CombatTickExtensionRegistry::Get().Register(&TickExtension);
		FGSTN_SnapshotContributorRegistry::Get().Register(&Contributor);
		FGSTN_AttackerOutcomeRegistry::Get().Register(&AttackerOutcome);
		FGSTN_OutcomeApplierRegistry::Get().Register(&EventApplier);
	}

	virtual void ShutdownModule() override
	{
		FGSTN_OutcomeApplierRegistry::Get().Unregister(&EventApplier);
		FGSTN_AttackerOutcomeRegistry::Get().Unregister(&AttackerOutcome);
		FGSTN_SnapshotContributorRegistry::Get().Unregister(&Contributor);
		FGSTN_CombatTickExtensionRegistry::Get().Unregister(&TickExtension);
		FGSTN_BackpackFragmentRegistration::Unregister();
	}

private:
	FGSTN_BackpackTickExtension TickExtension;
	FGSTN_BackpackSnapshotContributor Contributor;
	FGSTN_BackpackDomeAttackerOutcome AttackerOutcome;
	FGSTN_BackpackDomeEventApplier EventApplier;
};

IMPLEMENT_MODULE(FGSTNBackpackNPModule, GSTNBackpackNP)
