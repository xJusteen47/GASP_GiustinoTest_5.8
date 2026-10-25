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

	/**
	 * SetByCaller keys of the knockback and push back effects (UGSTN_KnockbackMoveCreator, UGSTN_PushBackMoveCreator): the velocity in world
	 * space (cm/s), the decay per frame (a fraction of the horizontal speed), the gravity scale, and the lifetime in seconds
	 */
	GSTNCOMBATNP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_MoveVelocityX);
	GSTNCOMBATNP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_MoveVelocityY);
	GSTNCOMBATNP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_MoveVelocityZ);
	GSTNCOMBATNP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_MoveDecay);
	GSTNCOMBATNP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_MoveGravityScale);
	GSTNCOMBATNP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_MoveSeconds);
	GSTNCOMBATNP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_LaunchResetAirJumps);
	GSTNCOMBATNP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_LaunchResetCoyote);

	/**
	 * Tags of the HitLanded and HitReceived outbox events beyond the hit type and the relationship (which reuse Combat.HitType.* and
	 * Combat.Relationship.*): whether the hit staggered, whether it made the victim tumble, and the reaction it chose
	 */
	GSTNCOMBATNP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(EventHit_Staggered);
	GSTNCOMBATNP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(EventHit_Tumble);
	GSTNCOMBATNP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(EventHit_Reaction_StaggerInPlace);
	GSTNCOMBATNP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(EventHit_Reaction_PushBack);
	GSTNCOMBATNP_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(EventHit_Reaction_Launch);
}
