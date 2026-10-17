// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_CombatInputTags.h"

namespace GSTNInputTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Button_Attack, "Input.Combat.Button.Attack", "Normal attack");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Button_Strong, "Input.Combat.Button.Strong", "Strong attack");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Button_Special, "Input.Combat.Button.Special", "Special attack");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Button_Crouch, "Input.Combat.Button.Crouch", "Crouch: movement stance and the down modifier of the standard commands");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Button_Shield, "Input.Combat.Button.Shield", "Shield / guard");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Button_Jump, "Input.Combat.Button.Jump", "Jump");
}
