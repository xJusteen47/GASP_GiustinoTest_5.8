// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_CombatCommandTags.h"

namespace GSTNCommandTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Attack, "Command.Attack", "Root of the attack commands");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Strong, "Command.Strong", "Root of the strong commands");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Special, "Command.Special", "Root of the special commands");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combo, "Command.Combo", "Root of the chord commands (several attack buttons at once)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Super, "Command.Super", "Root of the super commands");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Attack_Neutral, "Command.Attack.Neutral", "Attack, stick neutral or lightly tilted (aims it)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Attack_Directional, "Command.Attack.Directional", "Attack, stick tilted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Attack_CrouchNeutral, "Command.Attack.CrouchNeutral", "Attack, crouch held, stick neutral or lightly tilted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Attack_CrouchDirectional, "Command.Attack.CrouchDirectional", "Attack, crouch held, stick tilted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Strong_Neutral, "Command.Strong.Neutral", "Strong, stick neutral or lightly tilted (aims it)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Strong_Directional, "Command.Strong.Directional", "Strong, stick tilted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Strong_CrouchNeutral, "Command.Strong.CrouchNeutral", "Strong, crouch held, stick neutral or lightly tilted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Strong_CrouchDirectional, "Command.Strong.CrouchDirectional", "Strong, crouch held, stick tilted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Special_Neutral, "Command.Special.Neutral", "Special, stick neutral or lightly tilted (aims it)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Special_Directional, "Command.Special.Directional", "Special, stick tilted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Special_CrouchNeutral, "Command.Special.CrouchNeutral", "Special, crouch held, stick neutral or lightly tilted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Special_CrouchDirectional, "Command.Special.CrouchDirectional", "Special, crouch held, stick tilted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combo_AttackSpecial_Neutral, "Command.Combo.AttackSpecial.Neutral", "Attack + special chord, stick neutral or lightly tilted (aims it)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combo_AttackSpecial_Directional, "Command.Combo.AttackSpecial.Directional", "Attack + special chord, stick tilted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combo_AttackSpecial_CrouchNeutral, "Command.Combo.AttackSpecial.CrouchNeutral", "Attack + special chord, crouch held, stick neutral or lightly tilted");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combo_AttackSpecial_CrouchDirectional, "Command.Combo.AttackSpecial.CrouchDirectional", "Attack + special chord, crouch held, stick tilted");
}
