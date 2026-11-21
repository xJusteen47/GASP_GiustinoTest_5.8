// Respawn, pure core (no GAS, no Network Prediction, no world).

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Events the respawn framework publishes on the authoritative event stream (UGSTN_SimEventSubsystem, server only), for UI, audio, missions and
 * scoring. game-modes 8.2: root Message.Respawn.* owned by GSTNRespawn. Never granted on a pawn.
 */
namespace GSTNRespawnTags
{
	/** A player was knocked out and spent lives (Target = the pawn, Magnitude = lives left) */
	GSTNRESPAWNCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Respawn_Down);
	/** A player came back (Target = the new pawn) */
	GSTNRESPAWNCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Respawn_Respawned);
	/** The pool changed (Magnitude = lives now) */
	GSTNRESPAWNCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Respawn_PoolChanged);
	/** A checkpoint became the active one (Instigator = the player who touched it) */
	GSTNRESPAWNCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Respawn_CheckpointActivated);
	/** The pool ran out and the team restarts from the last checkpoint */
	GSTNRESPAWNCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Respawn_RestartFromCheckpoint);
	/** The pool ran out and the team restarts the level (missions listen to this to reset) */
	GSTNRESPAWNCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Respawn_RestartLevel);
}
