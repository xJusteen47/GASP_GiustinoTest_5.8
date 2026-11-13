// Defence, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags the defence plugin owns (m6 spec 1c, modularity-and-tags 4.2).
 *
 * Every defence tag is declared here, in the Core module, so the GAS and NP modules define no tag of their own and stay free of
 * tag definitions (accepted layout, plan F3). The "owner" column of the spec names the module that WRITES a tag at runtime, not
 * the declaring module. State tags (Combat.State.*) are absolute counts projected by the combat tick from the defence fragment,
 * never granted by an ability or an effect, so they stay deterministic across a rollback. Combat.Mover.Tumble is carried by the
 * tumble contact modifier. Events are published by the combat outbox on the authority. No defence tag is ever stored in a sync
 * state or a snapshot: those carry indices and flags only.
 *
 * The plugin declares no Combat.Link.*, Combat.Part.* or Interact.* tag. It reads the other plugins' tags by name through
 * FGameplayTag::RequestGameplayTag(..., false), never by an extern, so it builds and runs alone. There is no ShieldPoked event
 * and no poke tag: the shield is a fixed-size bubble (owner decision 2026-10-26).
 */
namespace GSTNDefenceTags
{
	/** Shield bubble raised (after startup). Projected, never granted; never projected where the mode disables the shield. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Shielding);

	/** Stalled by a blocked hit: the bubble stays up, no out-of-shield action. Projected until ShieldStunEndFrame. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ShieldStun);

	/** Long stun after a shield break (launched upward, no bubble). Projected until BreakEndFrame. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ShieldBroken);

	/** A parry window is open: the shield's perfect-shield window or a move's parry window track. Projected for the window. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ParryWindow);

	/** Defender's short recovery after a shield-source parry (a move keeps playing). Projected. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_ParryRecovery);

	/** Attacker punished by a parry. Projected until PunishEndFrame. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Parried);

	/** Tech animation running. Projected. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Teching);

	/** Downed after landing (the identifiers keep the name Knockdown). Projected until KnockdownEndFrame. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Knockdown);

	/** A tech press is ignored (anti-mash), for UI and AI. Projected while the frame is before TechLockoutEndFrame. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Tech_Lockout);

	/** Mover tag carried by the tumble contact modifier while the entity tumbles. Added and removed with the modifier. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mover_Tumble);

	/** The attack cannot be parried (victim- or ability-side, such as a boss signature attack). Granted by kits, abilities and effects. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_Parry);

	/** The attack cannot be blocked by the shield (victim- or ability-side). Granted by kits, abilities and effects. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Immunity_Shield);

	/** Shield command, Hold(Shield). Built only where the mode enables the shield. Part of the standard command set. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Defence_Shield);

	/** Locks the defence commands. An ordinary count granted by abilities and effects. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Lock_Command_Defence);

	/** A hit was blocked by the shield bubble. Authoritative event published by the combat outbox. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Blocked);

	/** A hit was parried. Authoritative event; the payload carries the source (shield or move). */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Parried);

	/** The shield broke from damage. Authoritative event published by the combat outbox. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_ShieldBroken);

	/** A tech succeeded. Authoritative event published by the combat outbox. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Teched);

	/** The entity was knocked down (landed and is downed). Authoritative event published by the combat outbox. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_KnockedDown);

	/** Gameplay effect magnitude key for the shield damage a blocked hit deals. Set by the damage code, read by the shield mirror effect. */
	GSTNDEFENCECORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_ShieldDamage);

	/** Appends every tag this module declares, so tests and validators cannot miss one. */
	GSTNDEFENCECORE_API void GetDeclaredTags(TArray<FGameplayTag>& OutTags);
}
