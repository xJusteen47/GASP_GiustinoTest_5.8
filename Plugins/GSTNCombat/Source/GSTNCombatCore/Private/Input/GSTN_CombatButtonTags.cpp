// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_CombatButtonTags.h"

namespace GSTNCombatButtonTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Button_Attack, "Input.Button.Attack", "Normal attack");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Button_Strong, "Input.Button.Strong", "Strong attack");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Button_Special, "Input.Button.Special", "Special attack");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Button_Crouch, "Input.Button.Crouch", "Crouch: movement stance and the down modifier of the standard commands");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Button_Shield, "Input.Button.Shield", "Shield / guard");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Button_Jump, "Input.Button.Jump", "Jump");
}
