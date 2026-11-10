// Held objects, world side (needs the GAS-NPP fork).

#include "GSTN_HeldAbilities.h"
#include "GSTN_HeldSettings.h"
#include "GSTN_HeldSim.h"
#include "GSTN_HeldSlots.h"
#include "GSTN_HeldTags.h"
#include "GSTN_InteractSettings.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Misc/CoreDelegates.h"
#include "Engine/Engine.h"
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
 * Registers the tag projection before the first simulation tick, the Held link type once the engine exists (the link registry is an engine
 * subsystem; M10 G1: the hold is a GSTNLink Hand link), the held command tier, and the
 * loadout extension that binds pickup, drop and throw in every compiled kit (the owner's default buttons, no kit Blueprint edited by hand);
 * removes them at shutdown
 */
class FGSTNHeldRuntimeModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FGSTN_CombatTickExtensionRegistry::Get().Register(&TickExtension);
		if (GEngine)
		{
			FGSTN_HeldLinkType::Register();
		}
		else
		{
			PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddStatic(&FGSTN_HeldLinkType::Register);
		}
		UGSTN_HeldSettings::Get().PushToInputSettings();

		// The pickup meaning the objects' GSTNInteract records carry (M10 G2), unless the project's interact settings list it already. GSTNLink's
		// probe reads these settings when it compiles its meaning table, so the meaning is there before any registry or world exists
		UGSTN_InteractSettings* Interact = GetMutableDefault<UGSTN_InteractSettings>();
		const FGameplayTag Pickup = GSTNHeldTags::Interact_Target_Pickup;
		if (!Interact->Meanings.ContainsByPredicate([&Pickup](const FGSTN_InteractMeaning& Meaning) { return Meaning.Tag == Pickup; }))
		{
			FGSTN_InteractMeaning& Meaning = Interact->Meanings.AddDefaulted_GetRef();
			Meaning.Tag = Pickup;
			Meaning.DefaultPriority = FMath::Clamp(UGSTN_HeldSettings::Get().PickupMeaningPriority, 0, 255);
			Meaning.HoldVerb = NSLOCTEXT("GSTNHeld", "PickupVerb", "Pick up");
			bAddedPickupMeaning = true;
		}

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
				// M10 G6: what each size class takes away while carrying (after the held bindings: they are never banned)
				UGSTN_HeldSlots::ApplySizeBans(Settings.SizeClasses, Input.Kit.Bindings);
				for (FGSTN_WeaponCompileInput& Weapon : Input.Weapons)
				{
					UGSTN_HeldSlots::ApplySizeBans(Settings.SizeClasses, Weapon.Bindings);
				}
			}
		};
		FGSTN_LoadoutExtensionRegistry::Register(LoadoutExtensionId, MoveTemp(Extension));
	}

	virtual void ShutdownModule() override
	{
		FGSTN_LoadoutExtensionRegistry::Unregister(LoadoutExtensionId);
		if (bAddedPickupMeaning && UObjectInitialized())
		{
			const FGameplayTag Pickup = GSTNHeldTags::Interact_Target_Pickup;
			GetMutableDefault<UGSTN_InteractSettings>()->Meanings.RemoveAll([&Pickup](const FGSTN_InteractMeaning& Meaning) { return Meaning.Tag == Pickup; });
		}
		UGSTN_HeldSettings::RemoveFromInputSettings();
		FGSTN_CombatTickExtensionRegistry::Get().Unregister(&TickExtension);
		FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
		FGSTN_HeldLinkType::Unregister();
	}

private:
	FGSTN_HeldTickExtension TickExtension;
	bool bAddedPickupMeaning = false;
	FDelegateHandle PostEngineInitHandle;
};

IMPLEMENT_MODULE(FGSTNHeldRuntimeModule, GSTNHeldRuntime);
