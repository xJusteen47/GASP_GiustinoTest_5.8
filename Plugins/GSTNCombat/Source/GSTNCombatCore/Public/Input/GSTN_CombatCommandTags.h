// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags of the standard commands (combat.md section 9.1). A command is an input combination identified by a tag
 * under "Command."; bindings, cancel windows, AI requests and events refer to the tag, never to the input, so changing
 * how a command is entered touches only its slot. A command is the same on the ground, in the air or on a wall: which
 * move it starts depends on the Input.Context.* tags (GSTNInputTags), checked by the bindings (section 9.2). Roots carry the priority tier
 * (UGSTN_InputSettings::CommandTiers). Game-specific commands made by designers live in
 * Config/Tags/GSTNCommands.ini instead.
 */
namespace GSTNCommandTags
{
	// Tier roots

	/** Root of the attack commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack);

	/** Root of the strong commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Strong);

	/** Root of the special commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Special);

	/** Root of the chord commands (several attack buttons at once) */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo);

	/** Root of the super commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Super);

	// Standard commands, one per family and shape. See combat.md section 9.1 for the patterns.

	/** Attack, stick neutral or lightly tilted (aims it) */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack_Neutral);

	/** Attack, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack_Directional);

	/** Attack, crouch held, stick neutral or lightly tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack_CrouchNeutral);

	/** Attack, crouch held, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack_CrouchDirectional);

	/** Strong, stick neutral or lightly tilted (aims it) */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Strong_Neutral);

	/** Strong, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Strong_Directional);

	/** Strong, crouch held, stick neutral or lightly tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Strong_CrouchNeutral);

	/** Strong, crouch held, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Strong_CrouchDirectional);

	/** Special, stick neutral or lightly tilted (aims it) */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Special_Neutral);

	/** Special, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Special_Directional);

	/** Special, crouch held, stick neutral or lightly tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Special_CrouchNeutral);

	/** Special, crouch held, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Special_CrouchDirectional);

	// Attack + special pressed together: higher tier and specificity than either single button.

	/** Attack + special chord, stick neutral or lightly tilted (aims it) */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_AttackSpecial_Neutral);

	/** Attack + special chord, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_AttackSpecial_Directional);

	/** Attack + special chord, crouch held, stick neutral or lightly tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_AttackSpecial_CrouchNeutral);

	/** Attack + special chord, crouch held, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_AttackSpecial_CrouchDirectional);

	// Movement commands: no tier root of their own (they lose to every attack command pressed with them). The default command set does not
	// fill a slot for them; a kit adds the slot (Press of the jump button, a down press for the fast fall) and binds the ability.

	/** Jump: ground jump, coyote jump or air jump, decided by the movement state (UGSTN_GA_PlatformJump) */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Jump);

	/** Fast fall while descending (UGSTN_GA_PlatformFastFall) */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_FastFall);

	// Defence, movement and interaction commands of the default movement set (UGSTN_MovementSlots). Like the jump commands they are
	// kit content: a kit removes them (CommandSet.RemovedCommands) or binds other abilities to them.

	/** Dash: a burst of speed along the facing, in the plane perpendicular to the local gravity (UGSTN_GA_Dash) */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Movement_Dash);

	/** Dodge: bound twice by context, a ground dodge (long jump arc) when grounded and an air dodge (dive roll) when airborne */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Defence_Dodge);

	/** Interact, short press: grab in the default movement set */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_Grab);

	/** Interact, long press: pick up in the default movement set */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interact_PickUp);
}
