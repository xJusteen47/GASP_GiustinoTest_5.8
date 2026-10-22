// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_CommandBindings.h"
#include "Input/GSTN_InputPattern.h"
#include "Input/GSTN_InputSlot.h"
#include "Kits/GSTN_CommandSet.h"
#include "Loadout/GSTN_LoadoutData.h"
#include "UObject/SoftObjectPath.h"

class UGSTN_InputSettings;
class UGSTN_SuperInputScheme;

/** Where a compiled entry comes from. The resolver filters entries by source each frame: the kit, the active weapon, a super slot */
enum class EGSTN_EntrySource : uint8
{
	Kit,
	Weapon,
	Super
};

/**
 * A binding as the compiler reads it: plain values with the ability as an opaque soft path. The compiler never loads or
 * inspects an ability class, so this module needs no ability-system type; the layer that owns the kit and weapon assets turns
 * its binding type into this one (GSTNCombatGAS, FGSTN_LoadoutAssembly).
 */
struct GSTNCOMBATCORE_API FGSTN_BindingInput
{
	FGameplayTag Command;
	FGSTN_TagRequirements Requirements;

	/** SlotOverride is the slot of a command the kit's command set does not have */
	bool bHasSlotOverride = false;
	FGSTN_InputSlotSpec SlotOverride;

	/** Opaque key of the ability class. Empty = binds nothing (the binding only hides a parent's), so it yields no entry */
	FSoftObjectPath Ability;

	/** Opaque key of the shared timeline the binding replaces the ability's own with (timeline abilities only) */
	FSoftObjectPath TimelineOverride;

	/** Added to the slot's priority offset for this binding only */
	int32 PriorityOffset = 0;

	/** -1 = the slot's buffer */
	int32 BufferFramesOverride = -1;
};

/** What the kit contributes to a compile */
struct GSTNCOMBATCORE_API FGSTN_KitCompileInput
{
	FGSTN_CommandSet CommandSet;

	/** The resolved base set of CommandSet (null: the set is only what the kit adds). Resolving loads, so the caller does it */
	const UGSTN_InputSlotSet* BaseSet = nullptr;

	TArray<FGSTN_BindingInput> Bindings;
	TArray<FSoftObjectPath> TriggeredAbilities;
	TArray<FSoftObjectPath> Passives;
	int32 SuperSlotCount = 0;
};

/** What one weapon contributes: its bindings after inheritance along the parent chain, and the passives it grants while active */
struct GSTNCOMBATCORE_API FGSTN_WeaponCompileInput
{
	TArray<FGSTN_BindingInput> Bindings;
	TArray<FSoftObjectPath> PassivesWhileActive;
};

/** What one filled super slot contributes. An empty slot has bPresent false */
struct GSTNCOMBATCORE_API FGSTN_SuperCompileInput
{
	bool bPresent = false;
	FSoftObjectPath Ability;
	FSoftObjectPath TimelineOverride;

	/** The code the player enters when the scheme is PerSuperCode */
	FGSTN_InputPattern InputCode;

	/** Context the super needs. State tags of any root */
	FGSTN_TagRequirements Requirements;
};

/** Everything Compile reads. Plain data, no world and no ability system */
struct GSTNCOMBATCORE_API FGSTN_CompileInput
{
	FGSTN_KitCompileInput Kit;

	/** In loadout order: index 0 is the kit's primary weapon, 1.. the additional weapons */
	TArray<FGSTN_WeaponCompileInput> Weapons;

	/** One per super slot of the kit, in slot order; fewer is allowed (the rest are empty) */
	TArray<FGSTN_SuperCompileInput> Supers;

	/** How supers are entered. Null with filled super slots is reported and the supers get no entry */
	const UGSTN_SuperInputScheme* SuperScheme = nullptr;

	/** The player's ability swaps, applied to the weapon entries */
	TArray<FGSTN_SlotCustomisation> Customisations;

	/** Stamped on the table; the loadout component increments it per applied loadout */
	int32 Revision = 0;

	/**
	 * Whether the ability class is a timeline ability (it then enters the timeline registry). The answer needs the class, which
	 * this module cannot inspect, so the layer that can supplies it. Unset: an entry is a timeline entry when it names a
	 * timeline override, and the kit's triggered abilities and passives are not (they carry no override to tell them apart).
	 * Must be a pure function of the path: it decides the registry, which every machine rebuilds identically.
	 */
	TFunction<bool(const FSoftObjectPath&)> IsTimelineAbility;
};

/** One row of the compiled table: a command bound to an ability in a context. The combat sync state carries the index of a row, never a tag */
struct GSTNCOMBATCORE_API FGSTN_CompiledEntry
{
	EGSTN_EntrySource Source = EGSTN_EntrySource::Kit;

	/** Kit: 0. Weapon: the weapon's loadout index. Super: the super slot */
	uint8 SourceIndex = 0;

	FGameplayTag Command;
	FGSTN_TagRequirements Requirements;

	/** Index in FGSTN_CompiledBindingTable::Slots. The slot is the command's input, shared by every entry of the command */
	int32 SlotIndex = INDEX_NONE;

	FSoftObjectPath Ability;

	/** Index in the timeline registry, INDEX_NONE for a plain ability */
	int16 TimelineIndex = INDEX_NONE;

	/** The binding's own offset and buffer, on top of the slot's (MakeEffectiveSlot applies them) */
	int32 PriorityOffset = 0;
	int32 BufferFramesOverride = -1;

	/** Rank of the effective slot with the settings the table was compiled with */
	FGSTN_CommandPriority Priority;
};

/** One distinct (ability, timeline override) pair a timeline ability can be started with */
struct GSTNCOMBATCORE_API FGSTN_TimelineRegistryEntry
{
	FSoftObjectPath Ability;
	FSoftObjectPath TimelineOverride;
};

/**
 * The compiled command table of one loadout (combat-m3 9): what every command starts in every context, plus the registry of
 * timeline abilities. It is rebuilt on every machine from the replicated loadout, so everything in it is a function of the
 * inputs and nothing else; the sync state refers to rows and registry slots by index.
 *
 * Order: kit rows, then weapon 0..N, then super slots; inside each source by command tag name (rows of one command keep the
 * order the data lists them in). The registry is sorted by (ability path, override path) with duplicates removed.
 */
struct GSTNCOMBATCORE_API FGSTN_CompiledBindingTable
{
	/** Registry indices are int16 in the sync state */
	static constexpr int32 MaxRegistryEntries = 32767;

	int32 Revision = 0;

	/** Weapons the table was compiled for (the number of weapon contexts the validator checks) */
	int32 WeaponCount = 0;

	/** Distinct slots in first-use order. Rows refer to them by index */
	TArray<FGSTN_InputSlotSpec> Slots;

	TArray<FGSTN_CompiledEntry> Entries;
	TArray<FGSTN_TimelineRegistryEntry> Registry;

	/** Ability classes to grant: those of the rows, the kit's triggered abilities and passives, sorted and unique. A weapon's own passives are granted when it becomes active */
	TArray<FSoftObjectPath> GrantedAbilities;

	/** The slot of the row with the binding's priority offset and buffer applied */
	FGSTN_InputSlotSpec MakeEffectiveSlot(const FGSTN_CompiledEntry& Entry) const;

	/** Registry index of the pair, INDEX_NONE if absent */
	int32 FindRegistryIndex(const FSoftObjectPath& Ability, const FSoftObjectPath& TimelineOverride) const;

	/**
	 * The table as bytes, for comparing two compiles and for a content signature. Tags are written as names in name order,
	 * so two tables compare equal exactly when they hold the same data.
	 */
	void WriteBytes(TArray<uint8>& OutBytes) const;
};

/** What a compile or a conflict check found, for the loadout rules and the UI */
struct GSTNCOMBATCORE_API FGSTN_CompileReport
{
	TArray<FText> Errors;
	TArray<FText> Warnings;

	void AddError(const FString& Text) { Errors.Add(FText::FromString(Text)); }
	void AddWarning(const FString& Text) { Warnings.Add(FText::FromString(Text)); }
};

/** The compiler. Stateless and pure: a function of the input and the settings, with no world, actor or asset load */
struct GSTNCOMBATCORE_API FGSTN_BindingTableCompiler
{
	/** Most super slots, as the number of Command.Super.Slot<N> commands this module declares */
	static constexpr int32 MaxSuperSlots = 8;

	/**
	 * Command of super slot Index (0-based): Command.Super.Slot<Index+1>. The default for PerSuperCode and for a FixedSlots
	 * entry that names no command. Invalid when Index is out of range.
	 */
	static FGameplayTag GetSuperSlotCommand(int32 Index);

	/**
	 * Builds the table. Rows come from the kit's bindings, each weapon's bindings and the filled super slots; the command set is
	 * the base minus the kit's removed commands plus its added slots, and a binding needs a slot in it (or its own override).
	 * Customisations then replace the ability of the weapon rows of a command. Problems that make a binding unusable (a
	 * removed command, no slot, a customisation of nothing) are reported and the binding is skipped; the compile never fails.
	 * @param Settings Ranks the slots (the priority criteria are data)
	 * @param OutReport Optional; receives the problems
	 */
	static FGSTN_CompiledBindingTable Compile(const FGSTN_CompileInput& Input, const UGSTN_InputSettings& Settings, FGSTN_CompileReport* OutReport = nullptr);

	/**
	 * Checks the table with the input plugin's validators, once per context a player can be in (the kit, one weapon, the
	 * supers): the slots against each other (ambiguous = error, shadowed = warning, duplicate command = error) and the bindings
	 * of each command against each other, using the settings' exclusive context groups (two bindings that require
	 * Grounded and Airborne never apply together). Repeated findings are reported once.
	 */
	static void FindConflicts(const FGSTN_CompiledBindingTable& Table, const UGSTN_InputSettings& Settings, FGSTN_CompileReport& OutReport);
};
