// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags of the standard commands (combat.md section 9.1). A command is an input combination identified by a tag
 * under "Command."; bindings, cancel windows, AI requests and events refer to the tag, never to the input, so changing
 * how a command is entered touches only its slot. A command is the same on the ground, in the air or on a wall: which
 * move it starts depends on the context tags below, checked by the bindings (section 9.2). Roots carry the priority tier
 * (UGSTN_CombatInputSettings::CommandTiers). Game-specific commands made by designers live in
 * Config/Tags/GSTNCombatCommands.ini instead.
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

	// Context tags the combat model projects from the owner's and Mover's state each frame (M2), for bindings to require

	/** The character is on the ground */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Context_Grounded);

	/** The character is in the air (falling or rising, not on a wall) */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Context_Airborne);

	/** The character is in a wall movement mode; with Airborne it identifies wall bindings */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Context_OnWall);
}
