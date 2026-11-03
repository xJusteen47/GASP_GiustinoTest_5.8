// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/StrongObjectPtr.h"

class AActor;
class UGSTN_CombatRules;
class UGSTN_DefenceRules;
struct FGSTN_LoadoutExtensionContext;
struct FGSTN_CompileInput;

/** The defence content an entity plays with (FGSTN_DefenceAttach): never state, every machine resolves it from the same assets */
struct GSTNDEFENCENP_API FGSTN_DefenceEntityData
{
	/** The rules asset, kept alive by the attach table */
	const UGSTN_DefenceRules* Rules = nullptr;

	/** The rules' index in this machine's append-only table (what the snapshot carries, never on the wire) */
	uint8 RulesIndex = 0;

	/** The kit names a shield ability: the second condition of UGSTN_DefenceMath::IsShieldAvailable */
	bool bKitHasShield = false;

	/** The hit types the bubble lets through: the mode's non-blockable list plus the grab-like types (m6 spec 5.3 "Contact") */
	FGameplayTagContainer BubbleIgnoreHitTypes;

	// ---- F8: the kit's landing and tech abilities (FGSTN_DefenceKitExtension::GetupSet, Tech), loaded when attached. Started Forced by the tick
	// extension through the model (they are kit triggered abilities, so they are in the timeline registry). Null = the kit has none

	TWeakObjectPtr<UClass> GroundBounce;
	TWeakObjectPtr<UClass> GetupRoll;

	/** By EGSTN_TechType (index 0, None, unused). All null = the kit has no tech: no lockout, no window, the button does nothing in tumble */
	TWeakObjectPtr<UClass> TechAbilities[6];
	bool bHasTechSet = false;

	/** The tech ability of a kind, null when the kit has none */
	UClass* GetTechAbility(uint8 Type) const { return Type < UE_ARRAY_COUNT(TechAbilities) ? TechAbilities[Type].Get() : nullptr; }
};

/**
 * Which defence rules an entity plays with and whether its kit has a shield (m6 spec 1d): the content side of the defence, kept apart
 * from the simulation state. It is data every machine resolves the same way (the kit's FGSTN_DefenceKitExtension, the match's rules
 * extension, the plugin settings), never sync state: the fragment carries the numbers, this registry the meaning (the rules the shield
 * value is drained with, the bubble size, the parry profiles). The simulation reads it as it reads any asset (the snapshot contributor,
 * the tick extension, the steps through the snapshot's rules index), so an actor without an entry is an entity without defence and nothing
 * is paid for it.
 *
 * Apply is one function for every machine: the loadout extension registered by this module calls it from the compile input hook, which
 * runs wherever the loadout is compiled (host and clients alike), so there is no OnRep and no IsServer branch. The harness calls the same
 * function by hand (UGSTN_DefenceContentLibrary::AttachDefenceToActor) on every world.
 *
 * The rules table is append-only for the life of the process: the snapshot and the lag compensation history carry a rules INDEX (a byte,
 * the spec's RulesIndex), and an index must never change meaning while a record that holds it can still be rewound. Indices are local to
 * the machine (they never travel), so the order machines attach in does not matter. Game thread only.
 */
class GSTNDEFENCENP_API FGSTN_DefenceAttach
{
public:
	/** The data attached to Owner, or null (no defence) */
	static const FGSTN_DefenceEntityData* Find(const AActor* Owner);

	/**
	 * Attaches the rules to Owner on this machine (a null Rules detaches). IgnoreHitTypes is what the bubble lets through
	 * (MakeBubbleIgnoreHitTypes). Returns whether data is attached after the call
	 */
	static bool Apply(AActor* Owner, const UGSTN_DefenceRules* Rules, bool bKitHasShield, const FGameplayTagContainer& IgnoreHitTypes);

	/**
	 * F8: records the kit's landing and tech abilities on Owner's entry (loaded here, outside any tick: they are the classes the loadout grants).
	 * Call after Apply; nothing happens for an owner without an entry. A null extension clears them
	 */
	static void SetKitMoves(const AActor* Owner, const struct FGSTN_DefenceKitExtension* Extension);

	static void Clear(AActor* Owner);

	/** The rules at an index of the table, null for an index nothing was registered under */
	static const UGSTN_DefenceRules* RulesAt(uint8 Index);

	/** The index of a rules asset in the table, adding it when new (255 for a null asset or a full table: no shield for it) */
	static uint8 IndexOf(const UGSTN_DefenceRules* Rules);

	/**
	 * The hit types the bubble ignores: the combat rules' NonBlockableHitTypes (Unblockable by default) and the grab-like types, read by
	 * name (Combat.HitType.Grab is the core's; Throw and Pummel are the link plugin's and are skipped where nobody declares them)
	 */
	static FGameplayTagContainer MakeBubbleIgnoreHitTypes(const UGSTN_CombatRules* CombatRules);

	/**
	 * The loadout extension this module registers (id GSTNDefenceNP, after the GAS module's GSTNDefence): attaches the kit's defence to the
	 * loadout's owner wherever the loadout is compiled; a kit without the extension detaches. Nothing is added to the compile input
	 */
	static void AttachFromLoadout(const FGSTN_LoadoutExtensionContext& Context, FGSTN_CompileInput& Input);

	static FName GetLoadoutExtensionId();

	/** Number of attached entities (tests) */
	static int32 Num();

	/** Drops every attachment and the rules table (tests only: a running simulation holds indices into the table) */
	static void Reset();
};
