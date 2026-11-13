// Links, Network Prediction half.

#include "Engine/World.h"
#include "GSTN_InteractSubsystem.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Hits/GSTN_HitExtensions.h"
#include "Modules/ModuleManager.h"
#include "Pipeline/GSTN_LinkHitOutcome.h"
#include "Sim/GSTN_LinkAttach.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkInboxKinds.h"
#include "Sim/GSTN_LinkSnapshotContributor.h"
#include "Sim/GSTN_LinkThrow.h"
#include "Sim/GSTN_LinkTickExtension.h"
#include "Simulation/GSTN_CombatTickExtension.h"
#include "World/GSTN_StepSnapshot.h"
#include "Data/GSTN_LinkKitExtension.h"
#include "GSTN_LinkRequirementLibrary.h"
#include "GSTN_LinkSettings.h"
#include "Input/GSTN_LinkSlots.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutAssembly.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNLinkKitThrows, Log, All);

namespace
{
	const FName KitThrowsExtensionId(TEXT("GSTNLinkThrows"));
}

/**
 * Registers the link fragment and the hooks that give it meaning before the first simulation tick: the snapshot contributor (link summary,
 * request tags, the parked anchor), the tick extension (timeouts, predicted grabs, veto, modifier edges, tags), the outcome appliers of the
 * LinkRequest (formation, throw) and LinkBreakOnHit steps, the LinkResponse / LinkCommand inbox kinds, the loadout extension that
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
		FGSTN_OutcomeApplierRegistry::Get().Register(&Applier);
		FGSTN_OutcomeApplierRegistry::Get().Register(&BreakApplier);
		FGSTN_OutcomeApplierRegistry::Get().Register(&ThrowApplier);
		FGSTN_LinkInboxHandlers::Register();

		FGSTN_LoadoutExtension Loadout;
		Loadout.ContributeCompileInput = &FGSTN_LinkAttach::AttachFromLoadout;
		FGSTN_LoadoutExtensionRegistry::Register(FGSTN_LinkAttach::GetLoadoutExtensionId(), MoveTemp(Loadout));

		// M11 K2: the kit throws of every grabbing kit asset (the mode's slots, the kit's set, the common set, the special grab). Reads only the
		// resolved kit, the project settings and the blockers registered at startup, the same on every machine, so every machine compiles the
		// same table. A kit built in memory by code (tests that count their rows) keeps its authored table, as the kit swap binding does
		FGSTN_LoadoutExtension Throws;
		Throws.ContributeCompileInput = [](const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input)
		{
			const UGSTN_LinkSettings& Settings = UGSTN_LinkSettings::Get();
			const UGSTN_PrimaryKitDefinition* Kit = Context.Resolved ? Context.Resolved->Kit : nullptr;
			if (!Settings.bApplyKitThrows || !Kit || !Kit->IsAsset())
			{
				return;
			}
			const FGSTN_LinkKitExtension* KitExtension = UGSTN_LinkRequirementLibrary::FindKitExtension(Kit);
			FGSTN_KitThrowFill Fill(Settings.ThrowInputMode);
			Fill.bSpecialCrouchGrab = Settings.bGrabWithSpecialCrouch;
			Fill.KitThrows = KitExtension ? &KitExtension->Throws : nullptr;
			Fill.Fallback = &Settings.FallbackThrows;
			Fill.SlotOverrides = &Settings.ThrowSlotOverrides;
			UGSTN_LinkSlots::GetThrowBlockers(Fill.BlockAll, Fill.BlockStickOnly);
			const int32 Bound = UGSTN_LinkSlots::ApplyKitThrows(Fill, Input.Kit.Bindings);
			if (Bound > 0)
			{
				UE_LOG(LogGSTNLinkKitThrows, Verbose, TEXT("KitThrows kit=%s mode=%d bound=%d"), *Kit->GetName(), static_cast<int32>(Settings.ThrowInputMode), Bound);
			}
		};
		FGSTN_LoadoutExtensionRegistry::Register(KitThrowsExtensionId, MoveTemp(Throws));

		WorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddRaw(this, &FGSTNLinkNPModule::OnWorldInitialized);
	}

	virtual void ShutdownModule() override
	{
		FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitHandle);
		FGSTN_LoadoutExtensionRegistry::Unregister(KitThrowsExtensionId);
		FGSTN_LoadoutExtensionRegistry::Unregister(FGSTN_LinkAttach::GetLoadoutExtensionId());
		FGSTN_LinkInboxHandlers::Unregister();
		FGSTN_OutcomeApplierRegistry::Get().Unregister(&ThrowApplier);
		FGSTN_OutcomeApplierRegistry::Get().Unregister(&BreakApplier);
		FGSTN_OutcomeApplierRegistry::Get().Unregister(&Applier);
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
	FGSTN_LinkOutcomeApplier Applier;
	FGSTN_LinkBreakOutcomeApplier BreakApplier;
	FGSTN_LinkThrowOutcomeApplier ThrowApplier;
	FDelegateHandle WorldInitHandle;
};

IMPLEMENT_MODULE(FGSTNLinkNPModule, GSTNLinkNP)
