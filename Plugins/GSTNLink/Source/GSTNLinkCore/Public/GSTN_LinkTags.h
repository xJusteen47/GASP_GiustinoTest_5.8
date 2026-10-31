// Links, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags the link plugin owns (m6 spec 1c, modularity-and-tags 4.2).
 *
 * Every link tag is declared here, in the Core module, so the GAS and NP modules define no tag of their own (accepted layout, plan
 * F5). The "owner" column of the spec names the module that WRITES a tag at runtime, not the declaring module. Combat.Link.Linked,
 * .Pending and the Combat.Link.Role.* tags are projected by the link tick extension from the link state's Role and Phase, never
 * granted by an ability or an effect, so they stay deterministic across a rollback. Combat.Mover.Linked is carried by the linked
 * modifier. Events are published by the watchdog and the pipeline on the authority. No link tag is ever stored in a sync state or a
 * snapshot: those carry the link type index, the role index and the break reason index.
 *
 * Combat.HitType.Grab is NOT declared here: GSTNCombatCore already declares it (GSTNCombatHitTags::HitType_Grab), so this plugin
 * reads it. Throw and Pummel are declared here because no other plugin does. The tags that other plugins add to the link subtree
 * (M7 Type.Mount/Ride and Role.Carrier/Rider, M10 Type.Held and Role.Holder/HeldObject, M8 Type.Minion, the drill's Type.Drill and
 * Role.Driller/Drilled) are reserved: they are declared by their own plugins, never here.
 *
 * The plugin declares no Combat.State.*, Combat.Part.* or Defence tag, and reads those by name through
 * FGameplayTag::RequestGameplayTag(..., false), never by an extern, so it builds and runs alone.
 */
namespace GSTNLinkTags
{
	// ---- State, projected ----

	/** The entity is in a Body-channel link. Projected from the body link state's Role, never granted */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Linked);

	/** A predicted link is awaiting its response. Projected from the link state's Phase, never granted */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pending);

	// ---- Link types ----

	/** Link type identity of a grab: on its definition asset, on grab hits and on events */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Type_Grab);

	/** Link type identity of a latch onto a large enemy: on its definition asset, on latch hits and on events */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Type_Latch);

	// ---- Roles ----

	/** The grabbing role. Projected from the link state's Role; bindings and ability blocks key on it */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Grabber);

	/** The grabbed role. Projected from the link state's Role; the base abilities are blocked by it */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Grabbed);

	/** The latching role (the entity that is carried on a partner's socket). Projected from the link state's Role */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Latcher);

	/** The latched role (the large enemy being ridden, which keeps acting). Projected from the link state's Role */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Role_Latched);

	// ---- Immunity and lock ----

	/** The entity cannot be grabbed (bosses, mid-link). Granted by kits and effects as an ordinary count; granting it mid-link breaks the link */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_Grab);

	/** The entity cannot be latched onto (bosses that must not be ridden). Granted by kits and effects as an ordinary count */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_Latch);

	/** Prevents the entity from accepting a link request. Read, never projected; an ordinary count granted by abilities and effects */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Lock_Link);

	// ---- Break reasons (generic; the wire form is the registry's reason index) ----

	/** The latched entity shook its latcher off with its own ability. Carried by LinkBroken */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_ShakeOff);

	/** The link lasted its definition's MaxFrames. Carried by LinkBroken */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_Timeout);

	/** The partner drifted farther from the expected socket than the break rules allow. Carried by LinkBroken */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_Distance);

	/** The entity that reports the break died. Carried by LinkBroken */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_OwnDeath);

	/** The partner died. Carried by LinkBroken */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_PartnerDeath);

	/** A hit that the role's break rules count broke the link. Carried by LinkBroken */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_Hit);

	/** The partner entered a state the break rules forbid, such as hitstun. Carried by LinkBroken */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_PartnerState);

	/** The grabbed entity mashed free before the end frame. Carried by LinkBroken and by GrabEscaped */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_Escape);

	/** The linking role released the partner on purpose. Carried by LinkBroken */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_Release);

	/** A throw ended the link. Carried by LinkBroken */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_Throw);

	/** Game code, a cutscene or a granted immunity forced the break. Carried by LinkBroken */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(BreakReason_Forced);

	// ---- Hit types (Combat.HitType.Grab is declared by GSTNCombatCore) ----

	/** A throw hit: delivered straight to the partner's inbox, skips parry and shield. On FGSTN_HitData */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitType_Throw);

	/** A pummel hit: lands only on the linked partner, no knockback, never breaks the link. On FGSTN_HitData */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitType_Pummel);

	// ---- Mover, interact ----

	/** Mover tag carried by the linked modifier while a parked role is linked. Added and removed with the modifier */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mover_Linked);

	/** A grabbable entity is the nearest interactable in reach. Projected by the link tick extension; the grab binding requires it */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Target_Grab);

	// ---- Commands (standard set) ----

	/** Grab command, a tap of the interact button. Requires Interact.Target.Grab. Part of the standard command set */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Link_Grab);

	/** Pummel command of the grabber role. Part of the standard command set */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Link_Pummel);

	/** Release command of the grabber role, a tap of the interact button. Part of the standard command set */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Link_Release);

	/** Forward throw command of the grabber role. Part of the standard command set */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Link_Throw_Forward);

	/** Back throw command of the grabber role. Part of the standard command set */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Link_Throw_Back);

	/** Up throw command of the grabber role. Part of the standard command set */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Link_Throw_Up);

	/** Down throw command of the grabber role. Part of the standard command set */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Link_Throw_Down);

	/** Attack command of the latcher role (a move whose hitbox lands only on the partner it rides). Part of the standard command set (F12b) */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Link_Latch_Attack);

	/** Locks the link command family. An ordinary count granted by abilities and effects */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Lock_Command_Link);

	// ---- Events ----

	/** A link formed. Authoritative event published once per key by the watchdog or the pipeline when the response is sent */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_LinkFormed);

	/** A link broke; the payload carries the reason. Authoritative event published once per key by the watchdog */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_LinkBroken);

	/** A throw released the partner. Authoritative event published once per key by the combat outbox */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Throw);

	/** The grabbed entity mashed free. Authoritative event published once per key by the watchdog */
	GSTNLINKCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_GrabEscaped);

	/** Appends every tag this module declares, so tests and validators cannot miss one */
	GSTNLINKCORE_API void GetDeclaredTags(TArray<FGameplayTag>& OutTags);
}
