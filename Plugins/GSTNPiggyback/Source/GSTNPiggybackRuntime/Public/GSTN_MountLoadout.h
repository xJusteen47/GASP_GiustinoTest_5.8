// Piggyback, runtime part (GAS and Network Prediction through GSTNLink and GSTNCombat).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

struct FGSTN_BindingInput;
struct FGSTN_CompileInput;
struct FGSTN_LoadoutExtensionContext;
struct FGSTN_MountKitExtension;

/**
 * The mount's part of a loadout compile (combat-m7-piggyback.md 7), registered with GSTNCombatGAS's loadout extension registry by the module, so
 * the kit asset never names piggyback. For a kit with FGSTN_MountKitExtension:
 *
 *   Command.Link.Mount     Press(Interact), offset -1           requires Interact.Target.Mount        when the kit can ride
 *   Command.Link.Dismount  Press(Interact), offset 101          requires Combat.Link.Role.Rider       when the kit can ride
 *   Command.Link.Drop      Press(Interact) + Hold(Crouch), 102  requires Combat.Link.Role.Carrier     when the kit can be mounted
 *
 * The offsets keep each slot off the priority of GSTNLink's slots on the same press (Grab 0, Release 100): the loadout validator refuses two slots
 * with one input at one priority whatever their requirements.
 *
 * Every slot falls through when unavailable (as GSTNLink's link slots), so outside its context the press stays free for the grab below it.
 * The command tags are the plugin's ini tags, requested by name at compile time (a disabled plugin has none, and nothing binds them).
 *
 * DisallowedCommands of the carrier profile: every kit and weapon binding of a listed command that does not already require
 * Combat.Link.Role.Carrier gets the role in its ignored tags, so while carrying the binding does not hold and the command is unbound.
 *
 * While riding, the linked movement mode exposes neither Grounded nor Airborne, so every ordinary binding of the rider stops holding by itself;
 * nothing is removed for the rider (spec 7).
 */
struct GSTNPIGGYBACKRUNTIME_API FGSTN_MountLoadout
{
	/** The registry id of the extension */
	static FName GetLoadoutExtensionId();

	/** ContributeCompileInput of the extension: the bindings and the disallowed commands of the resolved kit's mount data */
	static void Contribute(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input);

	/** The same on a kit's mount data directly (tests, tools) */
	static void ContributeFromKitData(const FGSTN_MountKitExtension& Mount, FGSTN_CompileInput& Input);

	/** The default bindings a kit with this mount data gets, in the order above (with their slot overrides) */
	static TArray<FGSTN_BindingInput> MakeDefaultBindings(const FGSTN_MountKitExtension& Mount);

	/** Adds Combat.Link.Role.Carrier to the ignored tags of every binding of Disallowed that does not require it. Returns how many changed */
	static int32 ApplyDisallowedCommands(const FGameplayTagContainer& Disallowed, FGSTN_CompileInput& Input);

	/** The command tags of the plugin's ini (invalid when the ini was not loaded) */
	static FGameplayTag GetMountCommand();
	static FGameplayTag GetDismountCommand();
	static FGameplayTag GetDropCommand();
};
