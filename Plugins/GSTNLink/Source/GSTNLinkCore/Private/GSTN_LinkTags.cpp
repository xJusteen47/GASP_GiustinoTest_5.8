// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "GSTN_LinkTags.h"

namespace GSTNLinkTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Linked, "Combat.Link.Linked", "The entity is in a Body-channel link. Projected every frame by the link tick extension from the body link state's Role, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Pending, "Combat.Link.Pending", "A predicted link is awaiting its response. Projected by the link tick extension from the link state's Phase, never granted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Type_Grab, "Combat.Link.Type.Grab", "Link type identity of a grab, on its definition asset, on grab hits and on events. Declared by the link plugin");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Type_Latch, "Combat.Link.Type.Latch", "Link type identity of a latch onto a large enemy, on its definition asset, on latch hits and on events. Declared by the link plugin");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Role_Grabber, "Combat.Link.Role.Grabber", "The grabbing role of a grab link. Projected from the link state's Role by the link tick extension; bindings and ability blocks key on it");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Role_Grabbed, "Combat.Link.Role.Grabbed", "The grabbed role of a grab link. Projected from the link state's Role by the link tick extension; the base abilities are blocked by it");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Role_Latcher, "Combat.Link.Role.Latcher", "The latching role of a latch link, the entity carried on a partner's socket. Projected from the link state's Role by the link tick extension");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Role_Latched, "Combat.Link.Role.Latched", "The latched role of a latch link, the large enemy being ridden, which keeps acting. Projected from the link state's Role by the link tick extension");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Immunity_Grab, "Combat.Immunity.Grab", "The entity cannot be grabbed, such as a boss. Granted by kits and gameplay effects as an ordinary count; granting it mid-link breaks the link");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Immunity_Latch, "Combat.Immunity.Latch", "The entity cannot be latched onto, such as a boss that must not be ridden. Granted by kits and gameplay effects as an ordinary count");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Lock_Link, "Combat.Lock.Link", "Prevents the entity from accepting a link request. Read by the request rules, never projected; an ordinary count granted by abilities and effects");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_ShakeOff, "Combat.Link.BreakReason.ShakeOff", "The latched entity shook its latcher off with its own ability. Carried by the LinkBroken event; the wire form is the reason index");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_Timeout, "Combat.Link.BreakReason.Timeout", "The link lasted its definition's MaxFrames. Carried by the LinkBroken event; the wire form is the reason index");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_Distance, "Combat.Link.BreakReason.Distance", "The partner drifted farther from the expected socket than the break rules allow. Carried by the LinkBroken event");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_OwnDeath, "Combat.Link.BreakReason.OwnDeath", "The entity that reports the break died. Carried by the LinkBroken event; the wire form is the reason index");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_PartnerDeath, "Combat.Link.BreakReason.PartnerDeath", "The partner died. Carried by the LinkBroken event; the wire form is the reason index");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_Hit, "Combat.Link.BreakReason.Hit", "A hit that the role's break rules count broke the link. Carried by the LinkBroken event; hits from the partner never do unless allowed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_PartnerState, "Combat.Link.BreakReason.PartnerState", "The partner entered a state the break rules forbid, such as hitstun or tumble. Carried by the LinkBroken event");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_Escape, "Combat.Link.BreakReason.Escape", "The grabbed entity mashed free before the end frame. Carried by the LinkBroken event and by GrabEscaped");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_Release, "Combat.Link.BreakReason.Release", "The linking role released the partner on purpose with the release command. Carried by the LinkBroken event");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_Throw, "Combat.Link.BreakReason.Throw", "A throw ended the link. Carried by the LinkBroken event; the throw itself is a hit on the partner");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(BreakReason_Forced, "Combat.Link.BreakReason.Forced", "Game code, a cutscene or a granted immunity forced the break. Carried by the LinkBroken event");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HitType_Throw, "Combat.HitType.Throw", "A throw hit, delivered straight to the partner's inbox without a hitbox overlap; skips parry and shield. Set on the hit data");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(HitType_Pummel, "Combat.HitType.Pummel", "A pummel hit that lands only on the linked partner, with no knockback, and never breaks the link. Set on the hit data");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mover_Linked, "Combat.Mover.Linked", "Mover tag carried by the linked modifier while a parked role is linked; triggers the linked mode transition. Added and removed with the modifier");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Interact_Target_Grab, "Interact.Target.Grab", "A grabbable entity is the nearest interactable in reach. Projected by the link tick extension; the grab binding requires it");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Link_Grab, "Command.Link.Grab", "Grab command, a tap of the interact button, requiring Interact.Target.Grab. Part of the standard command set");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Link_Pummel, "Command.Link.Pummel", "Pummel command of the grabber role, an attack press with a neutral stick. Part of the standard command set");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Link_Release, "Command.Link.Release", "Release command of the grabber role, a tap of the interact button. Part of the standard command set");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Link_Throw_Forward, "Command.Link.Throw.Forward", "Forward throw command of the grabber role. Part of the standard command set");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Link_Throw_Back, "Command.Link.Throw.Back", "Back throw command of the grabber role. Part of the standard command set");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Link_Throw_Up, "Command.Link.Throw.Up", "Up throw command of the grabber role. Part of the standard command set");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Command_Link_Throw_Down, "Command.Link.Throw.Down", "Down throw command of the grabber role. Part of the standard command set");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Input_Lock_Command_Link, "Input.Lock.Command.Link", "Locks the link command family. An ordinary count granted by abilities and gameplay effects");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_LinkFormed, "Combat.Event.LinkFormed", "A link formed. Authoritative event published once per key by the watchdog or the pipeline when the response is sent");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_LinkBroken, "Combat.Event.LinkBroken", "A link broke; the payload carries the reason. Authoritative event published once per key by the watchdog");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Throw, "Combat.Event.Throw", "A throw released the partner. Authoritative event published once per key by the combat outbox on the authority");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_GrabEscaped, "Combat.Event.GrabEscaped", "The grabbed entity mashed free. Authoritative event published once per key by the watchdog on the authority");

	void GetDeclaredTags(TArray<FGameplayTag>& OutTags)
	{
		OutTags.Append({
			Linked, Pending,
			Type_Grab, Type_Latch,
			Role_Grabber, Role_Grabbed, Role_Latcher, Role_Latched,
			Immunity_Grab, Immunity_Latch, Lock_Link,
			BreakReason_ShakeOff, BreakReason_Timeout, BreakReason_Distance, BreakReason_OwnDeath, BreakReason_PartnerDeath,
			BreakReason_Hit, BreakReason_PartnerState, BreakReason_Escape, BreakReason_Release, BreakReason_Throw, BreakReason_Forced,
			HitType_Throw, HitType_Pummel,
			Mover_Linked, Interact_Target_Grab,
			Command_Link_Grab, Command_Link_Pummel, Command_Link_Release,
			Command_Link_Throw_Forward, Command_Link_Throw_Back, Command_Link_Throw_Up, Command_Link_Throw_Down,
			Input_Lock_Command_Link,
			Event_LinkFormed, Event_LinkBroken, Event_Throw, Event_GrabEscaped });
	}
}
