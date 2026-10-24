// Native tags of the combat Network Prediction adapter.

#pragma once

#include "NativeGameplayTags.h"

/**
 * Tags the NP adapter owns. Mover tags are read by Mover itself (UMoverComponent::HasGameplayTagInState), so a mode can ask about them
 * from the start sync state without touching the ability system.
 */
namespace GSTNCombatNPTags
{
	/**
	 * Carried by FGSTN_LayeredMove_Freeze while it is active: the pawn is held still by hitstop. Movement modes that run timers or
	 * read input (the wall modes) lock on it. A Mover tag, never granted to the ability system.
	 */
	GSTNCOMBATNP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mover_Frozen);

	/** SetByCaller key of the freeze effect: the freeze duration in seconds (UGSTN_FreezeMoveCreator) */
	GSTNCOMBATNP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_FreezeSeconds);
}
