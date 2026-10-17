// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "NativeGameplayTags.h"

/**
 * Native tags of the standard combat buttons. A button is identified by its tag everywhere in data (conditions, input
 * config); its bit index exists only inside the simulation and on the wire (UGSTN_CombatInputConfig maps one to the
 * other). Projects add their own buttons with their own tags.
 */
namespace GSTNInputTags
{
	/** Normal attack */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Button_Attack);

	/** Strong attack */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Button_Strong);

	/** Special attack */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Button_Special);

	/** Crouch: movement stance and the down modifier of the standard commands */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Button_Crouch);

	/** Shield / guard */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Button_Shield);

	/** Jump */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Button_Jump);
}
