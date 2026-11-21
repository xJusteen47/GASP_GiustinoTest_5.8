// Respawn, pure core (no GAS, no Network Prediction, no world).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_RespawnRules.h"
#include "GSTN_RespawnLogic.generated.h"

class UGSTN_RespawnRules;

/** What a KO did to the pool */
UENUM(BlueprintType)
enum class EGSTN_LifeSpendResult : uint8
{
	/** The player comes back */
	Respawn,
	/** The pool ran out: the team restarts (the fallen player waits for it) */
	TeamRestart
};

/**
 * The team's shared lives (owner GM.9: shared reinforcements, no revive). Plain arithmetic so the respawn subsystem, its replicated mirror and
 * the tests use one rule. Server only; time is the caller's (seconds of game time).
 */
USTRUCT(BlueprintType)
struct GSTNRESPAWNCORE_API FGSTN_LivesPool
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Lives")
	int32 Lives = 0;

	/** The refill target (LivesPoolSize) */
	UPROPERTY(BlueprintReadOnly, Category = "Lives")
	int32 PoolSize = 0;

	/** Upper bound with extra lives */
	UPROPERTY(BlueprintReadOnly, Category = "Lives")
	int32 MaxLives = 0;

	/** Seconds per refilled life, 0 = none */
	UPROPERTY(BlueprintReadOnly, Category = "Lives")
	float RegenSeconds = 0.f;

	/** Seconds accumulated toward the next life (only while below PoolSize) */
	UPROPERTY(BlueprintReadOnly, Category = "Lives")
	float RegenProgress = 0.f;

	bool bRestartAtZero = true;

	/** Full pool from the rules (start and every team restart) */
	void Reset(const UGSTN_RespawnRules& Rules);

	/**
	 * A KO costing Cost lives. TeamRestart when the pool runs out per the rules' bRestartWhenPoolReachesZero: at zero after this KO, or at a KO
	 * with the pool already empty. A free KO (Cost 0) always respawns
	 */
	EGSTN_LifeSpendResult Spend(int32 Cost);

	/** Advances the refill; returns the lives gained. No refill at or above PoolSize; the progress restarts from 0 when the pool is full */
	int32 Advance(float DeltaSeconds);

	/** Extra lives (collectibles, debug), capped at MaxLives. Returns the lives actually added */
	int32 Add(int32 Count);

	/** Seconds until the next refilled life, -1 when none is coming */
	float SecondsToNextLife() const;
};

/** The team's checkpoint progress: the active checkpoint is the one players come back to (game-modes 5: shared, first touch activates) */
USTRUCT(BlueprintType)
struct GSTNRESPAWNCORE_API FGSTN_CheckpointProgress
{
	GENERATED_BODY()

	/** Order of the active checkpoint, INDEX_NONE = none (level start) */
	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	int32 ActiveOrder = INDEX_NONE;

	/** Identity of the active checkpoint (its actor name on the server; any stable id) */
	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	FName ActiveId;

	bool HasActive() const { return ActiveOrder != INDEX_NONE; }

	/** A player touched checkpoint Id of Order. Returns whether it became the active one (rules: bUseCheckpoints, bCheckpointsOnlyForward) */
	bool TryActivate(int32 Order, FName Id, const UGSTN_RespawnRules& Rules);

	void Clear() { ActiveOrder = INDEX_NONE; ActiveId = NAME_None; }
};

/** Where a player comes back, chosen from what the world offers */
UENUM(BlueprintType)
enum class EGSTN_RespawnPlacement : uint8
{
	Checkpoint,
	NearTeammate,
	LevelStart
};

namespace GSTNRespawnLogic
{
	/**
	 * The placement rule (game-modes 5): the active checkpoint when the rules use checkpoints and one is active; else near a living teammate on safe
	 * ground when the rules allow it and one exists; else the level start. bTeamRestart (the pool ran out) never uses a teammate: everyone is coming back
	 */
	GSTNRESPAWNCORE_API EGSTN_RespawnPlacement ChoosePlacement(const UGSTN_RespawnRules& Rules, bool bHasActiveCheckpoint, bool bHasSafeTeammate, bool bTeamRestart);

	/** Slot of the Index-th player among a checkpoint's NumPoints spawn points; beyond them the points repeat with an offset ring (OutRing) */
	GSTNRESPAWNCORE_API int32 SpawnPointFor(int32 Index, int32 NumPoints, int32& OutRing);
}
