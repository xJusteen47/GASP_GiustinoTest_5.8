// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_CombatLockTags.h"

namespace GSTNCombatLockTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Attack, "Input.Lock.Command.Attack", "Locks the attack commands");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Strong, "Input.Lock.Command.Strong", "Locks the strong commands");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Special, "Input.Lock.Command.Special", "Locks the special commands");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Combo, "Input.Lock.Command.Combo", "Locks the chord commands");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Super, "Input.Lock.Command.Super", "Locks the super commands");
}
