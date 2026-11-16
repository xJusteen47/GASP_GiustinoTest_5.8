// Native tags used only by the Core automation tests. The input plugin has no game vocabulary of its own, so the
// tests declare a small fighting-game-like set under a Test sub-root instead of borrowing the combat plugin's.

#pragma once

#include "NativeGameplayTags.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNTestTags
{
	// Buttons, in the bit order the tests' scripts use (A to F)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Button_Attack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Button_Strong);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Button_Special);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Button_Crouch);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Button_Shield);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Button_Jump);

	// Tier roots
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Strong);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Special);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Super);

	// Commands
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack_Neutral);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack_Directional);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack_CrouchDirectional);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Strong_Neutral);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Strong_CrouchDirectional);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Special_Neutral);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Special_Directional);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_AttackSpecial_Neutral);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo_AttackSpecial_CrouchNeutral);

	// Lock tags: one for a whole root, one for a single leaf. Special has none (a root without a lock tag)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Lock_Attack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Lock_AttackNeutral);
}

#endif // WITH_DEV_AUTOMATION_TESTS
