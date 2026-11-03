#include "Tests/GSTN_TestTags.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNTestTags
{
	UE_DEFINE_GAMEPLAY_TAG(Button_Attack, "Input.Button.Test.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Button_Strong, "Input.Button.Test.Strong");
	UE_DEFINE_GAMEPLAY_TAG(Button_Special, "Input.Button.Test.Special");
	UE_DEFINE_GAMEPLAY_TAG(Button_Crouch, "Input.Button.Test.Crouch");
	UE_DEFINE_GAMEPLAY_TAG(Button_Shield, "Input.Button.Test.Shield");
	UE_DEFINE_GAMEPLAY_TAG(Button_Jump, "Input.Button.Test.Jump");

	UE_DEFINE_GAMEPLAY_TAG(Attack, "Command.Test.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Strong, "Command.Test.Strong");
	UE_DEFINE_GAMEPLAY_TAG(Special, "Command.Test.Special");
	UE_DEFINE_GAMEPLAY_TAG(Combo, "Command.Test.Combo");
	UE_DEFINE_GAMEPLAY_TAG(Super, "Command.Test.Super");

	UE_DEFINE_GAMEPLAY_TAG(Attack_Neutral, "Command.Test.Attack.Neutral");
	UE_DEFINE_GAMEPLAY_TAG(Attack_Directional, "Command.Test.Attack.Directional");
	UE_DEFINE_GAMEPLAY_TAG(Attack_CrouchDirectional, "Command.Test.Attack.CrouchDirectional");
	UE_DEFINE_GAMEPLAY_TAG(Strong_Neutral, "Command.Test.Strong.Neutral");
	UE_DEFINE_GAMEPLAY_TAG(Strong_CrouchDirectional, "Command.Test.Strong.CrouchDirectional");
	UE_DEFINE_GAMEPLAY_TAG(Special_Neutral, "Command.Test.Special.Neutral");
	UE_DEFINE_GAMEPLAY_TAG(Special_Directional, "Command.Test.Special.Directional");
	UE_DEFINE_GAMEPLAY_TAG(Combo_AttackSpecial_Neutral, "Command.Test.Combo.AttackSpecial.Neutral");
	UE_DEFINE_GAMEPLAY_TAG(Combo_AttackSpecial_CrouchNeutral, "Command.Test.Combo.AttackSpecial.CrouchNeutral");

	UE_DEFINE_GAMEPLAY_TAG(Lock_Attack, "Input.Lock.Command.Test.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Lock_AttackNeutral, "Input.Lock.Command.Test.Attack.Neutral");
}

#endif // WITH_DEV_AUTOMATION_TESTS
