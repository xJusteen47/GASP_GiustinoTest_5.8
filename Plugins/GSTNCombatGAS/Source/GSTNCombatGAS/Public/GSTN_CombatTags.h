// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "GSTN_CombatCoreTags.h"

/**
 * Native tags of the combat ability layer. The input vocabulary (Input.Button/Context/Lock.Command/BufferEvent.*, the
 * Command.* families) belongs to the input plugin and the combat core, and Combat.Mover.* to the NP module that sees
 * Mover: none of those are declared here.
 *
 * The Combat.State.* leaves and the weapon, kit, super, loadout and skin identity roots belong to the combat core and are
 * declared in GSTN_CombatCoreTags.h (same namespace, included here). Designer tags (stat and meter names) are ini content:
 * this module declares only their roots so tag pickers can filter on them, and code never names a leaf.
 */
namespace GSTNCombatTags
{
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

	// Hurtbox zones (classification of a hurtbox, read by hit resolution and the editor tooling; never granted to an actor)

	/** Zone of a hurtbox on the head. Classifies a hurtbox in FGSTN_Hurtbox::Tags and in the editor's Physics Asset mapping; never granted to an actor. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hurtbox_Head);

	/** Zone of a hurtbox on the torso. Classifies a hurtbox; never granted to an actor. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hurtbox_Torso);

	/** Zone of a hurtbox on an arm (the side is the bone name, not a tag). Classifies a hurtbox; never granted to an actor. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hurtbox_Arm);

	/** Zone of a hurtbox on a leg (the side is the bone name, not a tag). Classifies a hurtbox; never granted to an actor. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hurtbox_Leg);

	/** Zone of a hurtbox on a hand. Classifies a hurtbox; never granted to an actor. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hurtbox_Hand);

	/** Zone of a hurtbox on a foot. Classifies a hurtbox; never granted to an actor. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hurtbox_Foot);

	// Set-by-caller keys (magnitudes travel in effects, not in tags)

	/** Set-by-caller key: the meter cost an effect spends. Set by the activating ability. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_MeterCost);

	// Roots of designer tags: leaves are ini content, code never names one

	/** Root of stat names. Leaves are designer content. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Stat);

	/** Root of meter names. Leaves are designer content. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_Meter);

	/** Root of per-stat set-by-caller keys. Leaves are designer content. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Root_SetByCallerStat);

	/** Appends every tag this module declares, so tests and validators cannot miss one. */
	GSTNCOMBATGAS_API void GetDeclaredTags(TArray<FGameplayTag>& OutTags);
}
