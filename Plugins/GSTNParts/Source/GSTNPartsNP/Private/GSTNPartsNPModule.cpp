// Destructible parts, Network Prediction half.

#include "GSTN_PartsFragmentRegistration.h"

#include "Hits/GSTN_HitExtensions.h"
#include "Modules/ModuleManager.h"
#include "Sim/GSTN_PartsAttackerOutcome.h"
#include "Sim/GSTN_PartsOutcomeApplier.h"
#include "Sim/GSTN_PartsSnapshotContributor.h"
#include "Sim/GSTN_PartsTickExtension.h"
#include "State/GSTN_PartsFragment.h"
#include "Sync/GSTN_SyncFragment.h"
#include "World/GSTN_StepSnapshot.h"

bool FGSTN_PartsFragmentRegistration::Register()
{
	return FGSTN_SyncFragmentRegistry::Register<FGSTN_PartsFragment>(GSTNFeatureIds::Parts);
}

void FGSTN_PartsFragmentRegistration::Unregister()
{
	FGSTN_SyncFragmentRegistry::Unregister(GSTNFeatureIds::Parts);
}

/**
 * Registers the parts fragment and the four hooks that give it meaning before the first simulation tick: the snapshot contributor, the
 * tick extension, the outcome applier and the attacker outcome (the ricochet prediction). A machine that has the plugin reads and writes the fragment and a machine without it skips
 * the bytes. An entity gets any of it only when it is attached a part set, so a project that enables the plugin and attaches no set pays
 * a map lookup per entity and tick and nothing on the wire.
 */
class FGSTNPartsNPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_PartsFragmentRegistration::Register();
		FGSTN_SnapshotContributorRegistry::Get().Register(&Contributor);
		FGSTN_CombatTickExtensionRegistry::Get().Register(&TickExtension);
		FGSTN_OutcomeApplierRegistry::Get().Register(&Applier);
		FGSTN_AttackerOutcomeRegistry::Get().Register(&AttackerOutcome);
	}

	virtual void ShutdownModule() override
	{
		FGSTN_AttackerOutcomeRegistry::Get().Unregister(&AttackerOutcome);
		FGSTN_OutcomeApplierRegistry::Get().Unregister(&Applier);
		FGSTN_CombatTickExtensionRegistry::Get().Unregister(&TickExtension);
		FGSTN_SnapshotContributorRegistry::Get().Unregister(&Contributor);
		FGSTN_PartsFragmentRegistration::Unregister();
	}

private:
	FGSTN_PartsSnapshotContributor Contributor;
	FGSTN_PartsTickExtension TickExtension;
	FGSTN_PartsOutcomeApplier Applier;
	FGSTN_PartsAttackerOutcome AttackerOutcome;
};

IMPLEMENT_MODULE(FGSTNPartsNPModule, GSTNPartsNP)
