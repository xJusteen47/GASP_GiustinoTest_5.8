// Links, Network Prediction half.

#include "Engine/World.h"
#include "GSTN_InteractSubsystem.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Modules/ModuleManager.h"
#include "Sim/GSTN_LinkAttach.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkSnapshotContributor.h"
#include "Sim/GSTN_LinkTickExtension.h"
#include "Simulation/GSTN_CombatTickExtension.h"
#include "World/GSTN_StepSnapshot.h"

/**
 * Registers the link fragment and the hooks that give it meaning before the first simulation tick: the snapshot contributor (link summary,
 * request tags, the parked anchor), the tick extension (timeouts, predicted grabs, veto, modifier edges, tags), the loadout extension that
 * attaches every compiled pawn's link content on every machine, and the link candidate provider with the interact subsystem of every world
 * (the probe of the tick extension calls it directly; the registration lets the interact runtime list it). A machine that has the plugin
 * reads and writes the fragment; a machine without it skips the bytes (the length-prefixed fragment list). An entity gets a fragment only
 * once it links, so enabling the plugin costs an idle pawn the attach entry, a blob in the snapshot and the tag projection, nothing on the wire.
 */
class FGSTNLinkNPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_LinkFragmentRegistration::Register();
		FGSTN_SnapshotContributorRegistry::Get().Register(&Contributor);
		FGSTN_CombatTickExtensionRegistry::Get().Register(&TickExtension);

		FGSTN_LoadoutExtension Loadout;
		Loadout.ContributeCompileInput = &FGSTN_LinkAttach::AttachFromLoadout;
		FGSTN_LoadoutExtensionRegistry::Register(FGSTN_LinkAttach::GetLoadoutExtensionId(), MoveTemp(Loadout));

		WorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FGSTNLinkNPModule::OnWorldInitialized);
	}

	virtual void ShutdownModule() override
	{
		FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitHandle);
		FGSTN_LoadoutExtensionRegistry::Unregister(FGSTN_LinkAttach::GetLoadoutExtensionId());
		FGSTN_CombatTickExtensionRegistry::Get().Unregister(&TickExtension);
		FGSTN_SnapshotContributorRegistry::Get().Unregister(&Contributor);
		FGSTN_LinkFragmentRegistration::Unregister();
	}

private:
	void OnWorldInitialized(UWorld* World, const UWorld::InitializationValues)
	{
		if (UGSTN_InteractSubsystem* Interact = UGSTN_InteractSubsystem::Get(World))
		{
			Interact->RegisterProvider(&TickExtension.GetProvider());
		}
	}

	FGSTN_LinkSnapshotContributor Contributor;
	FGSTN_LinkTickExtension TickExtension;
	FDelegateHandle WorldInitHandle;
};

IMPLEMENT_MODULE(FGSTNLinkNPModule, GSTNLinkNP)
