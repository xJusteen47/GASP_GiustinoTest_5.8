// Kit mods, ability layer: what the mods do to a loadout (combat-loadout-mods.md §8, §11.2).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_KitModTypes.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Timeline/GSTN_TimelineTypes.h"

class UAbilitySystemComponent;
class UGSTN_CombatTimeline;
class UGSTN_KitModDefinition;
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

/** One move the mods change, as the compile would bake it (editor preview and validators, M11 L8) */
struct GSTNKITMODSGAS_API FGSTN_ModMovePreview
{
	FGameplayTag Command;

	/** INDEX_NONE for a kit binding, else the weapon's index in the loadout */
	int32 WeaponIndex = INDEX_NONE;

	/** The ability the row really binds (a slot customisation's alternative included) */
	FSoftObjectPath Ability;

	/** What the derived timeline is named from: the binding's timeline override, else the ability */
	FSoftObjectPath BasePath;

	/** The combined change, after the kit's limits */
	FGSTN_CombinedMoveModifier Modifier;

	/** True when the kit's limits cut the mods' product back */
	bool bClamped = false;

	FName DerivedName;
	FGSTN_CombatTimelineData Base;
	FGSTN_CombatTimelineData Derived;
};

/** What a set of mods does to a kit, without compiling or registering anything */
struct GSTNKITMODSGAS_API FGSTN_KitModPreview
{
	const FGSTN_KitModSlots* KitSlots = nullptr;
	TArray<FSoftObjectPath> ModIds;

	/** The compile input after the mods' verbs (removed and added bindings, passives) */
	FGSTN_CompileInput Input;

	/** Every modded move, cap ignored, in compile order (kit bindings, then each weapon's) */
	TArray<FGSTN_ModMovePreview> Moves;

	/** ResolveMods' warnings: the mods that do not apply and why */
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

	/**
	 * What the compile would do to BaseInput (a compile input built without mods, FGSTN_LoadoutAssembly::BuildCompileInput) with the mods
	 * of Resolved: the same resolution, the same verbs and the same move selection and baking as ContributeCompileInput, but every
	 * derived timeline is returned as data, nothing is registered and the cap is not applied. Editor preview and validators; loads
	 */
	static void BuildPreview(const FGSTN_ResolvedLoadout& Resolved, const FGSTN_CompileInput& BaseInput, FGSTN_KitModPreview& Out);

	/** Drops the kept derived timelines (module shutdown, tests) */
	static void ResetDerivedCache();
};
