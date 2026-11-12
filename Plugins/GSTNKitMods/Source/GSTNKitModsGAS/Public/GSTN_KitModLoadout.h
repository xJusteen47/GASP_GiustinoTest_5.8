// Kit mods, ability layer: what the mods do to a loadout (combat-loadout-mods.md §8, §11.2).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_KitModTypes.h"

class UAbilitySystemComponent;
class UGSTN_CombatTimeline;
class UGSTN_KitModDefinition;
struct FGSTN_CompileInput;
struct FGSTN_CombatTimelineData;
struct FGSTN_LoadoutExtensionContext;
struct FGSTN_LoadoutRulesView;
struct FGSTN_ResolvedLoadout;

/** The mods of a resolved loadout that actually apply, in kit slot order, and why the others do not */
struct GSTNKITMODSGAS_API FGSTN_ResolvedMods
{
	/** The kit's mod extension, null if the kit takes no mod */
	const FGSTN_KitModSlots* KitSlots = nullptr;

	TArray<const UGSTN_KitModDefinition*> Mods;
	TArray<FSoftObjectPath> ModIds;

	/** A mod that does not apply is skipped with a warning, never an error: a kit swap must not be refused because a mod does not fit */
	TArray<FString> Warnings;
};

/**
 * The loadout extension of the mods plugin (registered as "GSTN.KitMods", priority 100: after extensions that add bindings, so their
 * moves get modified too). Game thread, outside the simulation; loads the mod assets and base timelines it reads.
 */
struct GSTNKITMODSGAS_API FGSTN_KitModLoadout
{
	static const FName ExtensionId;
	static constexpr int32 ExtensionPriority = 100;

	/** Picks the configuration for the kit, loads the mods, checks slot and compatibility; the same answer on every machine */
	static void ResolveMods(const FGSTN_ResolvedLoadout& Resolved, FGSTN_ResolvedMods& Out);

	/**
	 * Removes the bindings of RemovedCommands, adds AddedBindings and Passives to the kit, then replaces the timeline of every binding a
	 * move modifier selects with a derived timeline (FindOrBakeDerived). Moves past the kit's MaxDerivedTimelines keep their base
	 */
	static void ContributeCompileInput(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input);

	/** The mods' warnings, and their unlocked alternatives added to the primary weapon's allowed list */
	static void ContributeRulesView(const FGSTN_LoadoutExtensionContext& Context, FGSTN_LoadoutRulesView& View);

	/** Applies each mod's StatsEffect on the authority */
	static void OnGranted(const FGSTN_LoadoutExtensionContext& Context, UAbilitySystemComponent& AbilitySystem);

	/** Removes what OnGranted applied */
	static void OnCleared(UAbilitySystemComponent& AbilitySystem);

	/**
	 * The frame data a binding starts by default: its timeline override when set, else the ability class's own (the TimelineAsset or
	 * inline Timeline property of a combat timeline ability, read by reflection: this plugin does not depend on the NP layer). False for
	 * a plain ability. Loads
	 */
	static bool FindBaseTimeline(const FSoftObjectPath& Ability, const FSoftObjectPath& TimelineOverride, FGSTN_CombatTimelineData& OutData);

	/**
	 * The derived timeline of (base path, mods): a transient UGSTN_CombatTimeline named FGSTN_KitModBaking::MakeDerivedName, so its path
	 * is the same on every machine and the compiled registry sorts it the same everywhere. Rebaked on every call (a mod edited in the
	 * editor shows at the next compile) and kept alive by this module until shutdown
	 */
	static UGSTN_CombatTimeline* FindOrBakeDerived(const FSoftObjectPath& BasePath, const FGSTN_CombatTimelineData& Base, TConstArrayView<FSoftObjectPath> ModIds,
		const FGSTN_CombinedMoveModifier& Modifier);

	/** Drops the kept derived timelines (module shutdown, tests) */
	static void ResetDerivedCache();
};
