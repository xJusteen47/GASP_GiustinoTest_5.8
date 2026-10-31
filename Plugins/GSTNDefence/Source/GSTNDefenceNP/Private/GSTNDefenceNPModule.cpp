// Defence, Network Prediction half.

#include "GSTN_DefenceFragmentRegistration.h"

#include "Data/GSTN_DefenceTracks.h"
#include "GSTN_DefenceOutcomeApplier.h"
#include "Sim/GSTN_DefenceInboxKinds.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Modules/ModuleManager.h"
#include "Sim/GSTN_DefenceAttach.h"
#include "Sim/GSTN_DefenceFragment.h"
#include "Sim/GSTN_DefenceSnapshotContributor.h"
#include "Sim/GSTN_DefenceTickExtension.h"
#include "Sim/GSTN_DefenceTrackHooks.h"
#include "Simulation/GSTN_TimelineTrackHook.h"
#include "Sync/GSTN_SyncFragment.h"
#include "World/GSTN_StepSnapshot.h"

bool FGSTN_DefenceFragmentRegistration::Register()
{
	return FGSTN_SyncFragmentRegistry::Register<FGSTN_DefenceFragment>(GSTNFeatureIds::Defence);
}

void FGSTN_DefenceFragmentRegistration::Unregister()
{
	FGSTN_SyncFragmentRegistry::Unregister(GSTNFeatureIds::Defence);
}

/**
 * Registers the defence fragment and the hooks that give it meaning before the first simulation tick: the snapshot contributor (the
 * extension and the bubble), the tick extension (clocks, stamps, tags, veto), the two track hooks (hold window, parry window), the outcome
 * applier of the Parry and Shield steps, the Punish inbox kind and the loadout extension that attaches a kit's defence to its pawn on every machine. A machine that has the plugin reads and writes the
 * fragment and a machine without it skips the bytes. An entity gets any of it only when its kit carries a defence extension, so a project
 * that enables the plugin and gives no kit one pays a map lookup per entity and tick and nothing on the wire.
 */
class FGSTNDefenceNPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_DefenceFragmentRegistration::Register();
		FGSTN_SnapshotContributorRegistry::Get().Register(&Contributor);
		FGSTN_CombatTickExtensionRegistry::Get().Register(&TickExtension);
		FGSTN_TimelineTrackHookRegistry::Get().Register(FGSTN_HoldWindowTrack::StaticStruct(), &HoldHook);
		FGSTN_TimelineTrackHookRegistry::Get().Register(FGSTN_ParryWindowTrack::StaticStruct(), &ParryHook);
		FGSTN_OutcomeApplierRegistry::Get().Register(&Applier);
		FGSTN_DefencePunishHandler::Register();

		FGSTN_LoadoutExtension Loadout;
		Loadout.ContributeCompileInput = &FGSTN_DefenceAttach::AttachFromLoadout;
		FGSTN_LoadoutExtensionRegistry::Register(FGSTN_DefenceAttach::GetLoadoutExtensionId(), MoveTemp(Loadout));
	}

	virtual void ShutdownModule() override
	{
		FGSTN_LoadoutExtensionRegistry::Unregister(FGSTN_DefenceAttach::GetLoadoutExtensionId());
		FGSTN_DefencePunishHandler::Unregister();
		FGSTN_OutcomeApplierRegistry::Get().Unregister(&Applier);
		FGSTN_TimelineTrackHookRegistry::Get().Unregister(&ParryHook);
		FGSTN_TimelineTrackHookRegistry::Get().Unregister(&HoldHook);
		FGSTN_CombatTickExtensionRegistry::Get().Unregister(&TickExtension);
		FGSTN_SnapshotContributorRegistry::Get().Unregister(&Contributor);
		FGSTN_DefenceFragmentRegistration::Unregister();
	}

private:
	FGSTN_DefenceSnapshotContributor Contributor;
	FGSTN_DefenceTickExtension TickExtension;
	FGSTN_HoldWindowHook HoldHook;
	FGSTN_ParryWindowHook ParryHook;
	FGSTN_DefenceOutcomeApplier Applier;
};

IMPLEMENT_MODULE(FGSTNDefenceNPModule, GSTNDefenceNP)
