// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags of the standard commands (combat.md section 9.1). A command is an input combination identified by a tag
 * under "Command."; bindings, cancel windows, AI requests and events refer to the tag, never to the input, so changing
 * how a command is entered touches only its slot. Roots carry the priority tier (UGSTN_CombatInputSettings::CommandTiers).
 * Game-specific commands made by designers live in Config/Tags/GSTNCombatCommands.ini instead.
 */
namespace GSTNCommandTags
{
	// Tier roots (grounded). Airborne commands have their own roots so they can be configured and queried separately.

	/** Root of the grounded attack commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack);

	/** Root of the grounded strong commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Strong);

	/** Root of the grounded special commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Special);

	/** Root of the chord commands (several attack buttons at once) */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo);

	/** Root of the super commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Super);

	/** Root of the airborne attack commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Air_Attack);

	/** Root of the airborne strong commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Air_Strong);

	/** Root of the airborne special commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Air_Special);

	// Grounded commands, one per family and shape. See combat.md section 9.1 for the patterns.

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

	// Airborne commands (context: Combat.Context.Airborne).

	/** Airborne attack, stick neutral or lightly tilted (aims it) */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Air_Attack_Neutral);

	/** Airborne attack, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Air_Attack_Directional);

	/** Airborne attack, crouch held, stick neutral or lightly tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Air_Attack_CrouchNeutral);

	/** Airborne attack, crouch held, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Air_Attack_CrouchDirectional);

	/** Airborne strong, stick neutral or lightly tilted (aims it) */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Air_Strong_Neutral);

	/** Airborne strong, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Air_Strong_Directional);

	/** Airborne strong, crouch held, stick neutral or lightly tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Air_Strong_CrouchNeutral);

	/** Airborne strong, crouch held, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Air_Strong_CrouchDirectional);

	/** Airborne special, stick neutral or lightly tilted (aims it) */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Air_Special_Neutral);

	/** Airborne special, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Air_Special_Directional);

	/** Airborne special, crouch held, stick neutral or lightly tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Air_Special_CrouchNeutral);

	/** Airborne special, crouch held, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Air_Special_CrouchDirectional);

	// Attack + special pressed together: higher tier and specificity than either single button.

	/** Attack + special chord, stick neutral or lightly tilted (aims it) */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_AttackSpecial_Neutral);

	/** Attack + special chord, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_AttackSpecial_Directional);

	/** Attack + special chord, crouch held, stick neutral or lightly tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_AttackSpecial_CrouchNeutral);

	/** Attack + special chord, crouch held, stick tilted */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_AttackSpecial_CrouchDirectional);

	// Context

	/** Context tag the standard airborne commands require; granted by the game while the character is in the air */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Context_Airborne);
}
