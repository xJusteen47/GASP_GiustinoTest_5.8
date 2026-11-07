// Minion runtime (authority batch, M8 G9a).

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "Store/GSTN_MinionStore.h"
#include "GSTN_MinionSpawnerActor.generated.h"

class UBoxComponent;
class UGSTN_MinionCombatSubsystem;
class UGSTN_MinionWaveDefinition;
class UPrimitiveComponent;


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSTN_OnWaveClearedSignature);

/**
 * Plays a UGSTN_MinionWaveDefinition (combat-m8-minions.md 10). A designer places it in the level, assigns a wave and either ticks
 * bStartOnBeginPlay, leaves the trigger box on (a pawn entering starts the wave once), or calls StartWave from a Blueprint.
 *
 * Not replicated: it exists in the level on every machine so a client can preload the wave's puppets, but only the authority spawns. The
 * minions themselves replicate through the packs like any other.
 *
 * Preload: BeginPlay, on every net mode (bPreloadOnClients), loads the wave's assets asynchronously through the streamable manager, in two
 * steps because a definition has to be in memory before its soft references can be listed. Without it the first puppet of a type loads
 * synchronously on a client (perf-debt G6-G8) and the first spawn of a type on the server loads its timelines synchronously. StartWave
 * waits for the preload on the authority; an asset that is already in memory is not requested again (the subsystem and the pooled actors
 * keep what the minions use alive).
 *
 * Spawning: from the world tick, while a wave runs only (tick is enabled by StartWave and disabled when the wave is cleared). Each entry
 * spawns all its minions on the first update where the minion subsystem's last batch frame has reached the start frame plus the entry's
 * DelayFrames; the subsystem's spawn queue (gstn.Minion.SpawnsPerFrame) spreads them over frames. Minions are dealt round robin over the
 * actors carrying the entry's tag (looked up once at the start) and spread on a ring of RingRadius where several share a point.
 *
 * Alive count: the spawner keeps its own handles and drops a handle the moment its minion is Dying, Dead, freed or promoted, so counting
 * is a walk of at most the wave size and never of the whole store. OnWaveCleared fires once, when every entry has spawned and no minion
 * is left alive: a minion that is merely dying already counts as gone, so the signal does not wait for the corpses.
 */
UCLASS(Blueprintable, BlueprintType)
class GSTNMINIONRUNTIME_API AGSTN_MinionSpawnerActor : public AActor
{
	GENERATED_BODY()

public:
	AGSTN_MinionSpawnerActor();

	/** The wave to play */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Wave")
	TObjectPtr<UGSTN_MinionWaveDefinition> Wave;

	/** Start the wave when the level begins play (authority) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Wave")
	bool bStartOnBeginPlay = false;

	/** A pawn entering the trigger box starts the wave, once (authority). Off = the box does nothing and is not even registered for overlaps */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Wave")
	bool bUseTrigger = true;

	/** Preload the wave's assets asynchronously on a client too, so the puppets do not load synchronously when the first item arrives */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Wave")
	bool bPreloadOnClients = true;

	/** The trigger volume: scale it in the level. Overlaps pawns only */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Minion|Wave")
	TObjectPtr<UBoxComponent> TriggerBox;

	/**
	 * Starts the wave: waits for the preload, then spawns on the schedule of the entries. Authority only; ignored on a client, with no
	 * wave, while a wave is running and when the minion subsystem is inactive. A cleared wave may be started again. True when it started
	 */
	UFUNCTION(BlueprintCallable, Category = "Minion|Wave")
	bool StartWave();

	/** Minions of this wave that are valid and not Dying or Dead (queued and Spawning count). Entries not spawned yet are not counted */
	UFUNCTION(BlueprintPure, Category = "Minion|Wave")
	int32 GetAliveCount() const;

	/** True from StartWave until OnWaveCleared */
	UFUNCTION(BlueprintPure, Category = "Minion|Wave")
	bool IsWaveRunning() const { return bRunning; }

	/** True once the assets the wave needs are in memory (immediately for a wave with nothing to load) */
	UFUNCTION(BlueprintPure, Category = "Minion|Wave")
	bool IsPreloaded() const { return bPreloaded; }

	/** Fires once when every entry has spawned and no minion of the wave is alive (death, despawn and promotion all count) */
	UPROPERTY(BlueprintAssignable, Category = "Minion|Wave")
	FGSTN_OnWaveClearedSignature OnWaveCleared;

	/** C++ twin of OnWaveCleared with the batch frame it fired on (the bench and the tests listen to it) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnWaveClearedNative, int32 /*BatchFrame*/);
	FOnWaveClearedNative OnWaveClearedNative;

	/** Appends the handles of this wave's minions that are alive now (spawned and neither dying, dead nor gone). For the bench and scripts */
	void GetAliveHandles(TArray<FGSTN_MinionHandle>& Out) const;

	/** Appends every handle this wave spawned that still names a minion, corpses and dying ones included (the bench frees them at once) */
	void GetSpawnedHandles(TArray<FGSTN_MinionHandle>& Out) const;

	/** Requests the asynchronous preload (BeginPlay does; StartWave does when it has not happened). Idempotent */
	void BeginPreload();

	/** One step of the wave: spawns the entries that are due and checks for the clear. Tick calls it; tests drive it directly */
	void UpdateWave();

	/** The id every minion of this spawner carries as SpawnGroup (decides the pack) */
	int32 GetSpawnGroup() const { return SpawnGroup; }

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void OnDefinitionsLoaded();
	void OnDependenciesLoaded();
	/** Second preload step: the definitions are in memory, list and request what they reference */
	void RequestDependencies();
	void GatherSpawnPoints();
	void SpawnEntry(int32 EntryIndex, UGSTN_MinionCombatSubsystem& Minions);
	void FinishWave(int32 Frame);
	bool IsAuthorityWorld() const;

	bool bPreloadStarted = false;
	bool bPreloaded = false;
	bool bRunning = false;
	bool bSpawnPointsGathered = false;
	bool bTriggered = false;
	/** Set by StartWave when the preload was not done yet: the wave begins (start frame stamped) on the first update after it is */
	bool bAwaitingStart = false;

	int32 SpawnGroup = 0;
	int32 StartFrame = 0;
	int32 TotalToSpawn = 0;
	int32 TotalSpawned = 0;

	/** Minions spawned so far per entry (a full store stops an entry; it resumes on a later update) */
	TArray<int32> EntrySpawned;
	/** Handles of the spawned minions that were still alive at the last check (own list: no walk of the store) */
	TArray<FGSTN_MinionHandle> Tracked;
	/** Every handle spawned since the start, never pruned while the wave runs (GetSpawnedHandles) */
	TArray<FGSTN_MinionHandle> Spawned;
	/** Transforms of the spawn point actors, by tag, gathered once at the start */
	TMap<FName, TArray<FTransform>> SpawnPoints;

	TSharedPtr<FStreamableHandle> DefinitionsHandle;
	TSharedPtr<FStreamableHandle> DependenciesHandle;
};
