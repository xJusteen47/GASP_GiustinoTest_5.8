#include "Kits/GSTN_LocomotionProfile.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Modules/ModuleManager.h"

/**
 * Registers the locomotion loadout extension: a kit's FGSTN_LocomotionKitExtension becomes the pawn's attached run tuning wherever
 * the loadout is compiled (Mario-style run, docs/notes/corsa-mario-piano.md). A kit without the entry moves as before.
 */
class FGSTNCombatGASModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_LoadoutExtension Extension;
		Extension.ContributeCompileInput = &FGSTN_LocomotionLoadout::AttachFromLoadout;
		FGSTN_LoadoutExtensionRegistry::Register(FGSTN_LocomotionLoadout::GetLoadoutExtensionId(), MoveTemp(Extension));
	}

	virtual void ShutdownModule() override
	{
		FGSTN_LoadoutExtensionRegistry::Unregister(FGSTN_LocomotionLoadout::GetLoadoutExtensionId());
	}
};

IMPLEMENT_MODULE(FGSTNCombatGASModule, GSTNCombatGAS)
