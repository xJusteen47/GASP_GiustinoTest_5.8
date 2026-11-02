// Spin and drill, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags the spin and drill plugin owns (spin-drill.md 10, modularity-and-tags 4.2).
 *
 * Every tag of the feature is declared here, in the Core module, so the NP, GAS and presentation modules define no tag of their own.
 * The families are not ours: Command.Movement is the combat plugin's (its Jump, FastFall and Dash leaves live in GSTNCombatCore),
 * Combat.State, Combat.HitType and Combat.Event are shared roots, and Input.Button is open to any plugin on GSTNInputCore. This
 * plugin only adds leaves under them.
 *
 * Combat.State.* leaves are projected by the drill tick extension from the drill fragment's phase, never granted by an ability or
 * an effect, so they stay deterministic across a rollback. No tag is stored in a sync state or a snapshot: those carry indices.
 *
 * Not declared here: the Anim.Set.* leaves (they follow GSTNAnimation's rules and come with the presentation checkpoint), the link
 * type Combat.Link.Type.Drill and its roles (they come with the latch checkpoint, as a GSTNLink type) and the GameplayCue.SpinDrill.*
 * leaves (ini only, Config/Tags/GSTNSpinDrill.ini).
 */
namespace GSTNSpinDrillTags
{
	// ---- Commands (leaves of the combat plugin's Command.Movement family) ----

	/** Spin command, a clockwise stick rotation in Stick space */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Movement_Spin_Clockwise);

	/** Spin command, a counter-clockwise stick rotation in Stick space */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Movement_Spin_CounterClockwise);

	/** Spin command from the optional spin button; the sense is the last stick rotation's, clockwise by default */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Movement_Spin_Button);

	/** Drill command, Hold(Crouch) while a spin is active */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Command_Movement_Drill);

	// ---- Input ----

	/** Optional spin button, for players who prefer a button to a stick rotation */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_Button_Spin);

	// ---- State, projected, never granted ----

	/** A spin is active, either sense */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Spinning);

	/** A clockwise spin is active */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Spinning_Clockwise);

	/** A counter-clockwise spin is active */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Spinning_CounterClockwise);

	/** The air spin was used during the current airtime; cleared on landing */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_AirSpinSpent);

	/** The drill is diving or boring */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Drilling);

	/** The drill is latched onto a target and boring */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_DrillLatched);

	/**
	 * The low gravity of the air spin is on. The one exception to "projected, never granted": the low gravity effect grants it, and the
	 * tick extension compares it with the fragment every frame to apply or remove the effect (a cheap tag test instead of an effect query)
	 */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_SpinLowGravity);

	/** The drill's dive gravity is on (zero gravity while plunging). Like SpinLowGravity: granted only by the dive gravity effect, synced from the fragment */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_DiveGravity);

	// ---- SetByCaller magnitudes of the low gravity effect ----

	/** Multiplier of the jump and fall gravity scales while the air spin's low gravity is on */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_SpinDrill_GravityScale);

	/** Multiplier of the maximum fall speed while the air spin's low gravity is on */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_SpinDrill_FallSpeedScale);

	// ---- Hit type ----

	/** A hit from the drill tip */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitType_Drill);

	// ---- Events ----

	/** The drill latched onto a target */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Drill_Latched);

	/** The drill detached from its target (release, maximum time, stagger, target gone) */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Drill_Detached);

	/** The drill ended in the high jump out of the target */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Drill_Launched);

	/** The dive reached the ground and the ground lag started */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Drill_GroundLag);

	// ---- Ability identity ----

	/** The spin ability, ground and air */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Movement_Spin);

	/** The opposite-spin cancel ability */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Movement_SpinCancel);

	/** The drill dive ability */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_Drill_Dive);

	/** The drill bore ability, while latched */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_Drill_Bore);

	/** The drill launch ability, the high jump out */
	GSTNSPINDRILLCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Combat_Drill_Launch);

	/** Every tag declared above, in declaration order; the tag tests read it */
	GSTNSPINDRILLCORE_API void GetDeclaredTags(TArray<FGameplayTag>& OutTags);
}
