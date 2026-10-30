// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class UAbilitySystemComponent;
struct FGSTN_CompileInput;
struct FGSTN_ResolvedLoadout;

/**
 * What a loadout extension is handed: the resolved assets and, when the loadout is applied in a world, an object of that world
 * (the loadout component), so an extension can find the match's rules component. Null in tools and tests, which an extension
 * must accept (it then reads its own settings or the kit alone).
 */
struct GSTNCOMBATGAS_API FGSTN_LoadoutExtensionContext
{
	const FGSTN_ResolvedLoadout* Resolved = nullptr;
	const UObject* WorldContext = nullptr;
};

/**
 * What an optional plugin adds to a loadout (combat extension surface, modularity-and-tags 5b): bindings and abilities it reads
 * from its own kit extension, and attribute sets or effects it puts on the ability system when the loadout is granted. Every
 * callback is optional. They run outside the simulation, on the game thread, exactly where the loadout assembly and the
 * loadout component call them; nothing here is read by a simulation tick.
 */
struct GSTNCOMBATGAS_API FGSTN_LoadoutExtension
{
	/**
	 * Runs at the end of FGSTN_LoadoutAssembly::BuildCompileInput, on every machine that compiles the loadout: what it adds to the
	 * input (a binding, a triggered ability, a slot override) must depend on the resolved assets and on data every machine shares,
	 * because the compiled table is rebuilt identically everywhere and the combat sync state carries its indices.
	 */
	TFunction<void(const FGSTN_LoadoutExtensionContext&, FGSTN_CompileInput&)> ContributeCompileInput;

	/** Runs on the authority after the loadout component granted the table's abilities (an attribute set, a passive effect) */
	TFunction<void(const FGSTN_LoadoutExtensionContext&, UAbilitySystemComponent&)> OnGranted;

	/** Runs on the authority before the loadout component removes what it granted, to undo OnGranted */
	TFunction<void(UAbilitySystemComponent&)> OnCleared;
};

/**
 * The registered loadout extensions. An optional plugin registers one from its module startup under its own id and removes it at
 * shutdown; a disabled plugin is simply absent. Extensions run in id order (lexical), so two plugins contribute in the same
 * order on every machine. Game thread only.
 */
struct GSTNCOMBATGAS_API FGSTN_LoadoutExtensionRegistry
{
	/** Registers an extension; registering an id again replaces it */
	static void Register(FName Id, FGSTN_LoadoutExtension Extension);

	/** Removes an extension; returns whether it was registered */
	static bool Unregister(FName Id);

	static int32 GetCount();

	/** Whether an extension is registered under Id */
	static bool IsRegistered(FName Id);

	/** Runs every ContributeCompileInput in id order */
	static void RunContributeCompileInput(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input);

	/** Runs every OnGranted in id order */
	static void RunOnGranted(const FGSTN_LoadoutExtensionContext& Context, UAbilitySystemComponent& AbilitySystem);

	/** Runs every OnCleared in id order */
	static void RunOnCleared(UAbilitySystemComponent& AbilitySystem);
};
