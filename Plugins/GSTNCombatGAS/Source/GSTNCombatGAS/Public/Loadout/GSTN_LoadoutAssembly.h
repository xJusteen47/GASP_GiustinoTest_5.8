// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "Loadout/GSTN_LoadoutRules.h"
#include "UObject/SoftObjectPath.h"

class UGSTN_InputSettings;
class UGSTN_PrimaryKitDefinition;
class UGSTN_SkinDefinition;
class UGSTN_SuperDefinition;
class UGSTN_SuperInputScheme;
class UGSTN_WeaponDefinition;
struct FGSTN_AbilityBinding;

/**
 * The assets a loadout names, already resolved. Looking ids up (asset manager, loading) is the loadout component's job and
 * happens outside the simulation; this struct is what it hands to the assembly. A null pointer is an id that did not resolve.
 */
struct GSTNCOMBATGAS_API FGSTN_ResolvedLoadout
{
	FGSTN_LoadoutData Loadout;

	const UGSTN_PrimaryKitDefinition* Kit = nullptr;

	/** In loadout order: [0] is the kit's primary weapon, then one per entry of Loadout.AdditionalWeapons */
	TArray<const UGSTN_WeaponDefinition*> Weapons;

	/** One per entry of Loadout.Supers, in the same order; null for an empty or unresolved slot */
	TArray<const UGSTN_SuperDefinition*> Supers;

	/** The kit's super input scheme */
	const UGSTN_SuperInputScheme* SuperScheme = nullptr;

	const UGSTN_SkinDefinition* Skin = nullptr;
};

/**
 * Turns kit, weapon and super assets into the plain inputs of the pure compiler and loadout rules in GSTNCombatCore. It is the
 * only place that knows both the asset types (ability classes) and the compiler, so the compiler needs no ability-system type
 * and a test can feed it values without assets. Stateless; it reads assets, so it runs outside the simulation.
 */
struct GSTNCOMBATGAS_API FGSTN_LoadoutAssembly
{
	/** The binding as the compiler reads it: the soft ability and timeline become opaque paths, the slot override a plain spec */
	static FGSTN_BindingInput MakeBindingInput(const FGSTN_AbilityBinding& Binding);

	/**
	 * Whether the ability class at Path is a timeline ability: its default object carries the timeline ability tag. Loads the
	 * class, so it is for loadout application and validation, never for the simulation.
	 */
	static bool IsTimelineAbilityClass(const FSoftObjectPath& Path);

	/**
	 * Fills the compiler input: the kit's command set (its base set is resolved here, which loads it), the kit's bindings, each
	 * weapon's bindings after inheritance along its parent chain, the filled super slots and the customisations.
	 * The timeline predicate is IsTimelineAbilityClass.
	 */
	static void BuildCompileInput(const FGSTN_ResolvedLoadout& Resolved, int32 Revision, FGSTN_CompileInput& OutInput);

	/**
	 * Fills the view the loadout rules read. Table and Report are the results of the compile and of the conflict check; their
	 * problems become the table findings, and the kit's timeline abilities are listed against the table's registry.
	 */
	static void BuildRulesView(const FGSTN_ResolvedLoadout& Resolved, const FGSTN_CompiledBindingTable& Table, const FGSTN_CompileReport& Report, FGSTN_LoadoutRulesView& OutView);

	/**
	 * Compile, check conflicts and run the rules in one go: the whole validation of a loadout. The table is returned even when
	 * the rules reject the loadout, so a UI can show what went wrong. True when the rules found no error.
	 */
	static bool CompileAndValidate(const FGSTN_ResolvedLoadout& Resolved, int32 Revision, const UGSTN_InputSettings& Settings, const UGSTN_LoadoutRules& Rules,
		FGSTN_CompiledBindingTable& OutTable, FGSTN_LoadoutRuleResult& OutResult);
};
