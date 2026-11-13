// Minion runtime (authority batch, M8 G3).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Store/GSTN_MinionStore.h"
#include "GSTN_MinionSpawner.generated.h"

class UGSTN_MinionDefinition;

/**
 * Options of one spawn request (combat-m8-minions.md 10). The initial AI target of the spec joins with the brain (G8): until then a
 * spawner that wants a target writes an intent after the spawn.
 */
USTRUCT(BlueprintType)
struct GSTNMINIONRUNTIME_API FGSTN_MinionSpawnParams
{
	GENERATED_BODY()

	/** Free id a wave or a script groups its minions by; the batch never reads it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Spawn")
	int32 SpawnGroup = 0;

	/** Play the type's Spawn reaction timeline (the minion is in Spawning until it ends). Ignored when the type has none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Spawn")
	bool bPlaySpawnTimeline = true;
};

/** Options of UGSTN_MinionSpawner::Promote (combat-m8-minions.md 10) */
USTRUCT(BlueprintType)
struct GSTNMINIONRUNTIME_API FGSTN_MinionPromotionParams
{
	GENERATED_BODY()

	/** Fighter class to spawn instead of the definition's PromotionFighterClass. None = use the definition's */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Promotion")
	TSubclassOf<AActor> FighterClassOverride;

	/** The fighter's UGSTN_MinionPromotedComponent carries Health / MaxHealth of the minion (true) or 1 (false). The kit applies it (G9b) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Promotion")
	bool bKeepHealthFraction = true;
};

/**
 * Blueprint entry to spawn and despawn minions on the authority. A spawn claims the store slot at once (so the caller gets a stable
 * handle to keep) and is activated by the next batch's spawn queue drain, at most gstn.Minion.SpawnsPerFrame per frame in request
 * order: forty spawns of a wave land over ten frames instead of one hitch. On a client or with the plugin inactive every call is
 * a no-op that returns the null handle, so a level Blueprint needs no authority branch of its own.
 */
UCLASS()
class GSTNMINIONRUNTIME_API UGSTN_MinionSpawner : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Queues a spawn. Returns the claimed handle, or the null handle when the store is full, the definition is null or this is not the authority */
	UFUNCTION(BlueprintCallable, Category = "Minion|Spawn", meta = (WorldContext = "WorldContextObject"))
	static FGSTN_MinionHandle Spawn(const UObject* WorldContextObject, UGSTN_MinionDefinition* Definition, const FTransform& Transform, const FGSTN_MinionSpawnParams& Params);

	/** Frees a minion now (a queued spawn is cancelled). False for a stale or null handle */
	UFUNCTION(BlueprintCallable, Category = "Minion|Spawn", meta = (WorldContext = "WorldContextObject"))
	static bool Despawn(const UObject* WorldContextObject, FGSTN_MinionHandle Handle);

	/**
	 * Turns a live minion into a fighter (server core of the promotion; the client cross-fade and the wire flag are G9b). Spawns the fighter
	 * class (override, else the definition's PromotionFighterClass) at the minion's position and yaw, gives it a replicated
	 * UGSTN_MinionPromotedComponent, sets its team when it is a generic team agent, then frees the minion (the handle is invalid after).
	 * Returns the fighter, or null when refused: not the authority, the handle is stale or not alive (Spawning, Dying and Dead refuse), no
	 * class, or gstn.Minion.MaxFightersFromMinions promoted fighters are still alive. Not to be called from inside a batch stage
	 */
	UFUNCTION(BlueprintCallable, Category = "Minion|Spawn", meta = (WorldContext = "WorldContextObject"))
	static AActor* Promote(const UObject* WorldContextObject, FGSTN_MinionHandle Handle, const FGSTN_MinionPromotionParams& Params);

	/** True while the minion is valid (or queued) and is not Dying or Dead: a waiting spawn and a Spawning minion count as alive */
	UFUNCTION(BlueprintPure, Category = "Minion|Spawn", meta = (WorldContext = "WorldContextObject"))
	static bool IsMinionAlive(const UObject* WorldContextObject, FGSTN_MinionHandle Handle);

	/** True while the handle names a live (or queued) minion of this world */
	UFUNCTION(BlueprintPure, Category = "Minion|Spawn", meta = (WorldContext = "WorldContextObject"))
	static bool IsMinionValid(const UObject* WorldContextObject, FGSTN_MinionHandle Handle);
};
