// Minion runtime (authority batch, M8 G3).

#pragma once

#include "CoreMinimal.h"
#include "Containers/BitArray.h"
#include "Data/GSTN_MinionDefinition.h"
#include "LOD/GSTN_MinionLOD.h"
#include "Spawn/GSTN_MinionSpawner.h"
#include "Store/GSTN_MinionStore.h"
#include "Subsystems/WorldSubsystem.h"
#include "Sync/GSTN_InboxEntry.h"
#include "Timeline/GSTN_TimelinePlayer.h"
#include "World/GSTN_StepSnapshot.h"
#include "GSTN_MinionCombatSubsystem.generated.h"

class AGSTN_MinionActor;
class UGSTN_CombatTimeline;
class UGSTN_HurtboxSetDefinition;
struct FGSTN_TimelineTrack_Event;

namespace GSTNMinion
{
	/**
	 * MoveId values of the reaction timelines the batch plays without a request. Moves use 1..MaxMoves (index + 1); the reactions sit
	 * at the top of the 6-bit wire range so they never collide with a move. G3 uses Spawn and Death only; G5 adds the hit reactions
	 * and G6 freezes the table when the net item ships.
	 */
	namespace ReactionMoveIds
	{
		constexpr uint8 Spawn = 62;
		constexpr uint8 Death = 63;
	}

	/** Provider id of the minion subsystem in the combat world subsystem (fixed: providers append in id order on every machine) */
	constexpr int32 SnapshotProviderId = 8;
}

/**
 * A definition of the registry with its soft references resolved once, at registration: the batch reads raw pointers, never a soft
 * path, and the resolved objects are held here so the garbage collector keeps them as long as the type is registered (the whole
 * world, since the id table never shrinks).
 */
USTRUCT()
struct GSTNMINIONRUNTIME_API FGSTN_MinionResolvedType
{
	GENERATED_BODY()

	/** Same order as UGSTN_MinionDefinition::Moves; null where the move's timeline did not load */
	UPROPERTY()
	TArray<TObjectPtr<UGSTN_CombatTimeline>> MoveTimelines;

	UPROPERTY()
	TObjectPtr<UGSTN_CombatTimeline> SpawnTimeline;

	UPROPERTY()
	TObjectPtr<UGSTN_CombatTimeline> DeathTimeline;

	UPROPERTY()
	TObjectPtr<UGSTN_HurtboxSetDefinition> Hurtboxes;

	/** UGSTN_CombatWorldSubsystem::ComputeBoundRadius of Hurtboxes, once */
	float BoundRadius = 0.f;
};

/** Numbers of the last batch, for stats, the bench report and the tests */
struct FGSTN_MinionBatchStats
{
	int32 Frame = INDEX_NONE;
	int32 Alive = 0;
	int32 Scheduled = 0;
	int32 SpawnsActivated = 0;
	int32 HitsDrained = 0;
	int32 TimelineEvents = 0;
	int32 Despawned = 0;
};

/** Running totals for the bench report. Wall-clock measurement only: nothing in the simulation reads them */
struct FGSTN_MinionBenchTotals
{
	int32 Batches = 0;
	double TotalBatchSeconds = 0.0;
	double MaxBatchSeconds = 0.0;
};

/**
 * The authority side of the minions (combat-m8-minions.md 4, 5): owns the store, the definition registry, the actor pool, the spawn
 * queue and the inbox of hits, and runs the batch once per fixed simulation frame.
 *
 * Authority only. The subsystem exists in every game and PIE world (the net mode is unknown when subsystems are created) but
 * activates in OnWorldBeginPlay only when the world is not a client: a client binds nothing, registers no provider and refuses
 * spawns, so no stage ever needs an IsServer branch. The authority never re-simulates, so the batch is not rollback code; it is
 * still deterministic given the store, the intents and the inbox (no wall clock, no random, ascending slot order in every
 * stage), which is what the replay, the tests and the spec's "fighters see minions one frame older" rule rely on.
 *
 * Driving: UNetworkPredictionWorldManager::OnPostFixedTick (the G1 fork hook) calls TickBatch(ServerFrame) after every NP model of
 * the frame has ticked, so the snapshot S(F) the fighters hit was built from the store as written by batch F-1.
 *
 * Batch order (TickBatch, every stage a loop in ascending slot index over the slots the schedule runs this frame):
 *   0 spawn queue drain (FIFO, gstn.Minion.SpawnsPerFrame), 1 LOD update, 2 inbox drain, 3 timeline advance, 4 movement, 5 publish.
 * Events for Blueprint (hit, died, LOD changed) are queued by the stages and broadcast after stage 5, then dead minions are freed.
 *
 * Hit provider: the subsystem is the IGSTN_HittableProvider of GSTNEntityKinds::Minion. AppendSnapshotEntities reads the store,
 * which the batch of this frame has not touched yet (it runs after the models), so a fighter hits the end-of-F-1 minion.
 * PostToEntityInbox stamps a hit into PendingHits; the drain of the next batch applies it (the M4 rule: reaction at F+1).
 */
UCLASS()
class GSTNMINIONRUNTIME_API UGSTN_MinionCombatSubsystem : public UWorldSubsystem, public IGSTN_HittableProvider
{
	GENERATED_BODY()

public:
	/** The subsystem of World when it is active (authority, after begin play), null otherwise */
	static UGSTN_MinionCombatSubsystem* GetActive(const UWorld* World);

	// UWorldSubsystem
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// IGSTN_HittableProvider
	virtual int32 GetProviderId() const override { return GSTNMinion::SnapshotProviderId; }
	virtual void AppendSnapshotEntities(int32 Frame, TArray<FGSTN_SnapshotEntry>& OutEntries) override;
	virtual uint8 GetEntityKind() const override { return GSTNEntityKinds::Minion; }
	virtual bool PostToEntityInbox(int32 EntityId, uint16 Generation, const FGSTN_InboxEntry& Entry) override;

	bool IsActive() const { return bActive; }

	/**
	 * Claims a slot now and queues its activation (UGSTN_MinionSpawner::Spawn). The slot is alive in the store but pending: no stage
	 * and no snapshot sees it until the queue drain activates it. Null handle when full, inactive or the registry is full
	 */
	FGSTN_MinionHandle EnqueueSpawn(UGSTN_MinionDefinition* Definition, const FTransform& Transform, const FGSTN_MinionSpawnParams& Params);

	/** Frees a minion now (or cancels its queued spawn). Not to be called from inside a stage: the stages free through the despawn list */
	bool Despawn(const FGSTN_MinionHandle& Handle);

	/** True for a live or queued minion */
	bool IsValidHandle(const FGSTN_MinionHandle& Handle) const { return Store.IsValid(Handle); }

	/** Writes the intent of a live minion, stamped with the current server frame. False for a stale handle */
	bool SetIntent(const FGSTN_MinionHandle& Handle, const FGSTN_MinionIntent& Intent);

	/** Sets only the command of the intent (and refreshes its stamp) */
	bool RequestCommand(const FGSTN_MinionHandle& Handle, const FGameplayTag& Command);

	float GetHealth(const FGSTN_MinionHandle& Handle) const;

	/**
	 * Runs the batch of server frame Frame (see the class comment for the stage order). bResimulating is skipped: the authority
	 * never re-simulates, and the hook only reports it for the clients' managers; the guard keeps a misconfigured world harmless.
	 */
	void TickBatch(int32 Frame, bool bResimulating = false);

	/** Debug and test: every slot is put in this tier, no hysteresis (INDEX_NONE restores the distance rule) */
	void SetForcedLODTier(int32 Tier) { ForcedLODTier = Tier; }

	/** Fixed step in seconds the movement integrates with (read from the NP manager each frame when driven by it) */
	void SetFixedStepSeconds(float Seconds) { FixedStepSeconds = FMath::Max(Seconds, UE_KINDA_SMALL_NUMBER); }

	const FGSTN_MinionStore& GetStore() const { return Store; }
	const FGSTN_MinionBatchStats& GetLastBatchStats() const { return LastBatch; }
	const FGSTN_MinionBenchTotals& GetBenchTotals() const { return BenchTotals; }
	void ResetBenchTotals() { BenchTotals = FGSTN_MinionBenchTotals(); }
	int32 GetLastBatchFrame() const { return LastBatchFrame; }

	/** The actor of a live, activated slot; null otherwise */
	AGSTN_MinionActor* GetActorOfSlot(int32 SlotIndex) const;

	/** The registered definition of a store TypeIndex, null when out of range */
	const UGSTN_MinionDefinition* GetDefinition(int32 TypeIndex) const;

	/** Number of pooled (free) actors */
	int32 GetNumPooledActors() const { return FreeActors.Num(); }

	/**
	 * C++ hook: an event track of a minion's timeline was crossed this frame (fires once per crossing, whatever frames the LOD
	 * skipped). Called from inside stage 3: a listener must not spawn, despawn or write intents. G5 routes cues and outbox events here
	 */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnMinionTimelineEvent, const FGSTN_MinionHandle& /*Minion*/, const FGSTN_TimelineTrack_Event& /*Track*/, int32 /*TimelineFrame*/);
	FOnMinionTimelineEvent OnTimelineEvent;

	// Test hooks (GSTNMinion.Runtime.*). Kept outside an #if because UHT does not accept that macro in a reflected class; both are inert
	// unless a test calls them.

	/** Activates without begin play (a test world has none), as OnWorldBeginPlay would on an authority */
	void ActivateForTests() { Activate(); }

	/** When set, every stage appends (stage number, slot index) for each slot it processes */
	TArray<FIntPoint>* StageTraceForTests = nullptr;

	enum class EStage : uint8
	{
		SpawnDrain = 0,
		LODUpdate,
		InboxDrain,
		TimelineAdvance,
		Movement,
		Publish
	};

private:
	struct FQueuedSpawn
	{
		FGSTN_MinionHandle Handle;
		uint8 TypeIndex = 0;
		FTransform Transform;
		FGSTN_MinionSpawnParams Params;
	};

	struct FPendingHit
	{
		int32 Frame = INDEX_NONE;
		int32 EntityId = INDEX_NONE;
		uint16 Generation = 0;
		FGSTN_InboxEntry Entry;
	};

	/** A Blueprint event a stage queued; broadcast after stage 5 */
	struct FQueuedEvent
	{
		enum class EKind : uint8 { Hit, Died, LODChanged } Kind = EKind::Hit;
		FGSTN_MinionHandle Handle;
		float Damage = 0.f;
		int32 OtherId = INDEX_NONE;
		uint8 OldTier = 0;
		uint8 NewTier = 0;
	};

	void Activate();
	void HandlePostFixedTick(int32 ServerFrame, int32 EndTimeMS, bool bResimulating);

	/** Index of Definition in the id table, registering it (and resolving its references) on first use. INDEX_NONE when full */
	int32 RegisterDefinition(UGSTN_MinionDefinition* Definition);

	// Stages
	void StageSpawnDrain(int32 Frame);
	void BuildSchedule(int32 Frame);
	void StageLODUpdate(int32 Frame);
	void StageInboxDrain(int32 Frame);
	void StageTimelineAdvance(int32 Frame);
	void StageMovement(int32 Frame);
	void StagePublish(int32 Frame);
	void FlushEventsAndDespawns();

	/** Starts a timeline on a slot (a move, or a reaction id of GSTNMinion::ReactionMoveIds) at Frame */
	void StartTimeline(int32 Slot, uint8 MoveId, int32 Frame);
	const UGSTN_CombatTimeline* TimelineOfMoveId(int32 Slot, uint8 MoveId) const;
	/** Ends whatever timeline the slot plays: cooldown stamp for a move, Idle after Spawning, Dead after the death timeline */
	void EndTimeline(int32 Slot, int32 Frame);

	void AcquireActor(int32 Slot, const FTransform& Transform);
	void ReleaseActor(int32 Slot);
	void FreeSlot(int32 Slot);

	const FGSTN_MinionLODParams& LODParamsOf(int32 Slot) const;
	int32 IntervalOf(int32 Slot) const;
	void QueueLODChange(int32 Slot, uint8 OldTier, uint8 NewTier);
	void TraceStage(EStage Stage, int32 Slot);

	bool bActive = false;
	FDelegateHandle PostFixedTickHandle;
	bool bProviderRegistered = false;

	FGSTN_MinionStore Store;

	/** The stable id table: index = store TypeIndex. Registered on first spawn, never reordered or shrunk */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UGSTN_MinionDefinition>> Definitions;

	/** Parallel to Definitions */
	UPROPERTY(Transient)
	TArray<FGSTN_MinionResolvedType> ResolvedTypes;

	/** Actor of each slot (null for a free or pending slot) */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AGSTN_MinionActor>> SlotActors;

	/** Released actors, reused before any new spawn; last released is reused first */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AGSTN_MinionActor>> FreeActors;

	// Runtime columns the Core store does not carry (Capacity entries each; reset when a slot is activated)

	/** Claimed by EnqueueSpawn, not yet activated by the drain */
	TBitArray<> PendingActivation;
	/** Last frame the slot ran a batch (the movement dt and the LOD dwell count from it) */
	TArray<int32> LastRunFrame;
	/** Timeline frame of the previous evaluation, INDEX_NONE before the first: events fire in (previous, current] exactly once */
	TArray<int32> TimelinePrevFrame;
	/** Cooldown stamps: frame from which move m of slot s may start again, at [s * GSTNMinion::MaxMoves + m] */
	TArray<int32> NextMoveFrame;
	/** Pipeline id of the last attacker (KO credit; the 4-entry ring of the spec comes with G5) */
	TArray<int32> LastAttackerId;

	TArray<FQueuedSpawn> SpawnQueue;
	TArray<FPendingHit> PendingHits;
	TArray<FQueuedEvent> QueuedEvents;
	/** Slots whose death ended this frame, freed after the events are broadcast */
	TArray<int32> DespawnList;

	// Per-batch scratch (kept to avoid allocation)
	TArray<int32> Scheduled;
	TArray<FVector2D> PlayerPositions;
	TArray<FPendingHit> DrainScratch;
	FGSTN_TimelineOutput TimelineOutput;

	FGSTN_MinionLODParams DefaultLODParams;
	int32 ForcedLODTier = INDEX_NONE;
	float FixedStepSeconds = 1.f / 62.5f;
	int32 LastBatchFrame = INDEX_NONE;

	FGSTN_MinionBatchStats LastBatch;
	FGSTN_MinionBenchTotals BenchTotals;
};
