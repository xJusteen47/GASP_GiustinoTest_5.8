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

	/** Locks the movement of a combatant in a hit reaction or KO (read by the stats movement modifier). Projected from the combat state every frame, never granted */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Lock_Reaction);

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
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_LoadoutSwapped);

	/** A super was used. Published by the combat tick on the authority. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_SuperUsed);

	/** A hit landed. Outbox event on the attacker (CM 5.6), published once on the authority. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_HitLanded);

	/** A hit was received. Outbox event on the victim, published once on the authority. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_HitReceived);

	/** A combatant was knocked out; the KOReason tag says why. Published once by the combat tick on the authority. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_KO);

	/** Gameplay event sent to the victim's abilities when it is hit (payload: FGSTN_HitEventPayload in the event's context). Not an outbox event. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Received);

	/** Root of the surface impact events (splat, bounce, break). */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Impact);

	/** A launched victim was pinned to a surface (outbox event on the victim; the tags carry the surface type). */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Impact_Splat);

	/** A launched victim bounced off a surface (outbox event on the victim). */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Impact_Bounce);

	/** A launched victim broke the surface it hit and went through (outbox event on the victim). */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Impact_Break);

	// KO reasons (named on the KO event, never granted)

	/** The combatant was knocked out by its health or damage cap. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(KOReason_Health);

	/** The combatant was knocked out by entering a KO zone. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(KOReason_Zone);

	// States (Hitstun and Hitstop belong to the combat core)

	/** The combatant was launched hard enough to tumble. Projected by the combat tick once hits exist, never granted. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Tumble);

	// Cues

	/** Root of the hit cues. Leaf cues are designer content in a tag ini, code never names one. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cue_Hit);

	// Activation sources (EGSTN_ActivationSource as tags, on the events that name how a timeline started)

	/** A command started the timeline. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Source_Command);

	/** A forced command (test or scripted) started the timeline. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Source_Forced);

	/** Something other than a command (a trigger, an effect, another ability, AI) started the ability and the combat tick adopted it. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Source_Adopted);

	// Set-by-caller keys (magnitudes travel in effects, not in tags)

	/** Set-by-caller key: the meter cost an effect spends. Set by the activating ability. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_MeterCost);

	/** Set-by-caller key: the damage of a hit, in the unit of the damage mode. Read by the damage execution. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);

	/** Set-by-caller key: the meter a gain effect adds. Set by whoever applies the effect. */
	GSTNCOMBATGAS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Meter);

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
