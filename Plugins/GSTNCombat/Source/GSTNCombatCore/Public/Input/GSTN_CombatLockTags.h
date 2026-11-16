// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native lock tags of the combat command roots (M2 section 6b). `Input.Lock.Command.<root>` locks `Command.<root>.*`; the
 * input plugin owns the global `Input.Lock.Command` and the lock mechanism, and each root's lock tag is declared with the
 * root. A stance, a hitstun or a cutscene grants the ones it needs through its ability's ActivationOwnedTags or a gameplay
 * effect. Roots that do not exist natively yet (Movement, Weapon, Link) declare theirs when they are added.
 */
namespace GSTNCombatLockTags
{
	/** Locks the attack commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack);

	/** Locks the strong commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Strong);

	/** Locks the special commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Special);

	/** Locks the chord commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Combo);

	/** Locks the super commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Super);
}
