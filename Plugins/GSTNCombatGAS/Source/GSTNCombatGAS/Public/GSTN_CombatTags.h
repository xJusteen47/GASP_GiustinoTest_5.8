// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags of the combat ability layer. The input vocabulary (Input.Button/Context/Lock.Command/BufferEvent.*, the
 * Command.* families) belongs to the input plugin and the combat core, and Combat.Mover.* to the NP module that sees
 * Mover: none of those are declared here.
 *
 * Combat.State.* are projected by the combat tick from the simulation, never granted by an ability or an effect, so they
 * stay deterministic across a rollback. Designer tags (weapon, kit, super, loadout and skin identity, stat and meter
 * names) are ini content: this module declares only their roots so tag pickers can filter on them, and code never names
 * a leaf.
 */
namespace GSTNCombatTags
{
	// States, projected by the combat tick

	/** The combatant is committed to an action and cannot start a new one freely. Projected, never granted. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Busy);

	/** A combat timeline is playing. Projected, never granted. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_InTimeline);

	/** The super input sequence is being entered. Projected, never granted. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_SuperInput);

	/** The active timeline is inside a cancel window. Projected, never granted. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_CancelWindow);

	/** The active timeline is inside its startup override window (a new command may replace it). Projected, never granted. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_OverrideWindow);

	/** The combatant is falling relative to its local gravity. Projected, never granted. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Falling);

	// Ability identity

	/** Identifies an ability that plays a combat timeline. Set in the ability's AbilityTags. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Timeline);

	/** Marks a triggered ability that waits to be adopted by the timeline that spawned it. Owned by that ability while it waits. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_AwaitingAdoption);

	// Locks

	/** Locks the combatant's movement (read by the stats movement modifier). Granted by abilities and effects that root the combatant. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Lock_Movement);

	// Weapon roles

	/** Role of the weapon the combatant uses by default. Granted by the stats effect of the active weapon. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Role_Primary);

	/** Role of the weapon the combatant can switch to. Granted by the stats effect of the active weapon. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Role_Support);

	// Events (authoritative outbox, published once per occurrence)

	/** A combat timeline started. Published by the combat tick on the authority. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_TimelineStarted);

	/** A combat timeline finished or ended. Published by the combat tick on the authority. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_TimelineFinished);

	/** The active weapon changed. Published by the combat tick on the authority. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_WeaponSwitched);

	/** A super was used. Published by the combat tick on the authority. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_SuperUsed);

	// Set-by-caller keys (magnitudes travel in effects, not in tags)

	/** Set-by-caller key: the meter cost an effect spends. Set by the activating ability. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_MeterCost);

	// Roots of designer tags: leaves are ini content, code never names one

	/** Root of weapon identity tags. Leaves are designer content. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Weapon);

	/** Root of kit identity tags. Leaves are designer content. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Kit);

	/** Root of super identity tags. Leaves are designer content. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Super);

	/** Root of loadout tags. Leaves are designer content. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Loadout);

	/** Root of skin tags. Leaves are designer content. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Skin);

	/** Root of stat names. Leaves are designer content. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Stat);

	/** Root of meter names. Leaves are designer content. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Meter);

	/** Root of per-stat set-by-caller keys. Leaves are designer content. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_SetByCallerStat);

	/** Appends every tag this module declares, so tests and validators cannot miss one. */
	GSTNCOMBATGAS_API void GetDeclaredTags(TArray<FGameplayTag>& OutTags);
}
