// Piggyback, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags the piggyback plugin owns (combat-m7-piggyback.md 11, plan G4).
 *
 * Every piggyback tag is declared here, in the Core module (a Runtime module), so the Runtime, Editor and Tests modules define none of their
 * own. The link type and role tags are the ones GSTNLink reserves for this plugin and never declares itself. The generic tags GSTNLink owns
 * (Combat.Link.Linked/Pending, Combat.Lock.Link, Combat.Mover.Linked, Combat.Event.LinkFormed/LinkBroken and the generic break reasons
 * Timeout, Distance, OwnDeath, PartnerDeath, Hit, Release, Throw, Forced...) are reused by name, never redeclared here. Designer-made
 * command slots (Command.Link.Mount/Dismount/Drop) are not native: they live in Config/Tags/GSTNPiggybackCommands.ini.
 *
 * No tag here is ever stored in a sync state or a snapshot: the link state carries the link type, role and break reason indices of
 * GSTNLink's registry. Projected tags (roles, Riding, MountPending, Interact.Target.Mount) are written by the link tick from the link state
 * and removed by the same projection, so a rollback or a break cannot leave one behind; Carrying is an ordinary tag of the carry load effect.
 */
namespace GSTNPiggybackTags
{
	// ---- Link type and roles (reserved for this plugin by GSTNLink) ----

	/** Link type identity of a mount: on its definition asset, on mount hits and on events */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Type_Mount);

	/** The carrying role of a mount (the link's target, acts while linked). Projected from the link state's Role */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Carrier);

	/** The riding role of a mount (the link's initiator, parked on the carrier, acts while linked). Projected from the link state's Role */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Rider);

	// ---- Hit type ----

	/** The interaction hit type of the mount ability's hitbox, which makes a mount request. On FGSTN_HitData, never on an actor */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitType_Interaction_Mount);

	// ---- State ----

	/** The rider's mount is Active. Projected from the link state, cleared by the same projection on any break */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Riding);

	/** The carrier carries a rider. An ordinary tag of the carry load effect, removed with it */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Carrying);

	/** The rider's pending phase before the mount is accepted (cosmetics, locks). Projected from the link state's Phase */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_MountPending);

	// ---- Pair moves ----

	/** Identity of the reference pair move where the carrier throws its rider. On the definition, carried by pair-move events */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PairMove_ThrowRider);

	/** Identity of the reference pair move where the rider jumps off the carrier with a boost. On the definition, carried by pair-move events */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(PairMove_BoostJump);

	// ---- Events ----

	/** A mount formed. Authoritative event, published once per key on the authority */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Link_Mounted);

	/** A mount request was refused; the payload carries the verdict. Authoritative event, published once per key on the authority */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Link_MountRefused);

	/** A pair move started on both sides. Authoritative event, published once per key on the authority */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_PairMove_Started);

	/** A pair move was aborted. Authoritative event, published once per key on the authority */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_PairMove_Aborted);

	// ---- Break reasons (this plugin's own; the wire form is GSTNLink's reason index from FirstPlugin on) ----

	/** The rider left on purpose with the dismount command. Carried by LinkBroken */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_Dismount);

	/** The carrier put its rider down with the drop command. Carried by LinkBroken */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_Drop);

	/** A pair move's outcome frame ended the mount. Carried by LinkBroken */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_Outcome);

	/** A hit on the carrier above the mount rules' carrier threshold ended the mount. Carried by LinkBroken */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_CarrierHit);

	/** A hit on the rider above the mount rules' rider threshold ended the mount. Carried by LinkBroken */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_RiderHit);

	// ---- Credit, set by caller, interact ----

	/** Event tag: the hit or KO event credits a partner of a pair move */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Link_Credit_Partner);

	/** Set-by-caller key of the carry load effect: the rider's quantised weight ratio as a unit value (0..1) */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_CarryLoad_Ratio);

	/** A mountable ally is the nearest interactable in reach. Projected from the interact probe's meaning; the mount binding requires it */
	GSTNPIGGYBACKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Target_Mount);

	/** Appends every tag this module declares, so tests and validators cannot miss one */
	GSTNPIGGYBACKCORE_API void GetDeclaredTags(TArray<FGameplayTag>& OutTags);

	/** Appends the command slots of the plugin's tag ini (requested by name, so an unregistered one comes back empty) */
	GSTNPIGGYBACKCORE_API void GetCommandSlotTags(TArray<FGameplayTag>& OutTags);
}
