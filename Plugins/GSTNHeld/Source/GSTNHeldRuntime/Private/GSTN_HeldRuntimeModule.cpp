// Held objects, world side (needs the GAS-NPP fork).

#include "GSTN_HeldAbilities.h"
#include "GSTN_HeldSettings.h"
#include "GSTN_HeldSim.h"
#include "GSTN_HeldSlots.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Modules/ModuleManager.h"
#include "Simulation/GSTN_CombatTickExtension.h"

namespace
{
	const FName LoadoutExtensionId(TEXT("GSTNHeld"));

	FSoftObjectPath AbilityOrDefault(const FSoftClassPath& Override, const UClass* Default)
	{
		return Override.IsValid() ? FSoftObjectPath(Override.ToString()) : FSoftObjectPath(Default);
	}
}

/**
 * Registers the held fragment, its inbox kind and the tag projection before the first simulation tick, the held command tier, and the
 * loadout extension that binds pickup, drop and throw in every compiled kit (the owner's default buttons, no kit Blueprint edited by hand);
 * removes them at shutdown
 */
class FGSTNHeldRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_HeldInbox::Register();
		FGSTN_CombatTickExtensionRegistry::Get().Register(&TickExtension);
		UGSTN_HeldSettings::Get().PushToInputSettings();

		FGSTN_LoadoutExtension Extension;
		// Reads only the resolved kit and the project settings, which every machine of a match shares, so every machine compiles the same table
		Extension.ContributeCompileInput = [](const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input)
		{
			const UGSTN_HeldSettings& Settings = UGSTN_HeldSettings::Get();
			const UGSTN_PrimaryKitDefinition* Kit = Context.Resolved ? Context.Resolved->Kit : nullptr;
			if (Settings.WantsDefaultBindings(Kit ? FSoftObjectPath(Kit) : FSoftObjectPath()))
			{
				UGSTN_HeldSlots::AppendKitBindings(AbilityOrDefault(Settings.PickupAbility, UGSTN_GA_HeldPickup::StaticClass()),
					AbilityOrDefault(Settings.DropAbility, UGSTN_GA_HeldDrop::StaticClass()),
					AbilityOrDefault(Settings.ThrowAbility, UGSTN_GA_HeldThrow::StaticClass()), Input.Kit.Bindings);
			}
		};
		FGSTN_LoadoutExtensionRegistry::Register(LoadoutExtensionId, MoveTemp(Extension));
	}

	virtual void ShutdownModule() override
	{
		FGSTN_LoadoutExtensionRegistry::Unregister(LoadoutExtensionId);
		UGSTN_HeldSettings::RemoveFromInputSettings();
		FGSTN_CombatTickExtensionRegistry::Get().Unregister(&TickExtension);
		FGSTN_HeldInbox::Unregister();
	}

private:
	FGSTN_HeldTickExtension TickExtension;
};

IMPLEMENT_MODULE(FGSTNHeldRuntimeModule, GSTNHeldRuntime);
