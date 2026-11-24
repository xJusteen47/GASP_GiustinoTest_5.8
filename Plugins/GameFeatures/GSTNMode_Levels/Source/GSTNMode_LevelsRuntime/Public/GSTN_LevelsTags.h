// Levels mode: tags.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

/**
 * Tags of GSTNMode_Levels (game-modes 8.2). Message.Levels.* are published on the authoritative event stream (server only) for UI, audio and
 * scoring, like Message.Mission.* and Message.Respawn.*; never granted on a pawn. Levels.Collectible is the default identity of AGSTN_LevelCollectible.
 */
namespace GSTNLevelsTags
{
	/** Identity added to the Collected event of an AGSTN_LevelCollectible (designers add their own leaves on the actor) */
	GSTNMODE_LEVELSRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Levels_Collectible);
	/** The level began (Magnitude = level index in the set, -1 = not in the set) */
	GSTNMODE_LEVELSRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Levels_LevelStarted);
	/** The first alive player entered a goal zone and its grace timer started (Instigator = the zone, Magnitude = grace seconds) */
	GSTNMODE_LEVELSRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Levels_GoalGraceStarted);
	/** A goal zone finished (Instigator = the zone): the level completes right after */
	GSTNMODE_LEVELSRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Levels_GoalReached);
	/** The level is complete and the results are ready (Magnitude = total score) */
	GSTNMODE_LEVELSRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Levels_LevelCompleted);
	/** The last level of the set was completed (no next level to travel to) */
	GSTNMODE_LEVELSRUNTIME_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Message_Levels_LevelSetCompleted);
}
