// Minion runtime (authority batch, M8 G3).

#pragma once

#include "CoreMinimal.h"
#include "Containers/BitArray.h"
#include "Data/GSTN_MinionDefinition.h"
#include "Hits/GSTN_HitResolution.h"
#include "LagComp/GSTN_MinionHistory.h"
#include "LOD/GSTN_MinionLOD.h"
#include "Movement/GSTN_MinionMovement_Lightweight.h"
#include "Movement/GSTN_MinionMovementBackend.h"
#include "Net/GSTN_MinionNetCodec.h"
#include "Net/GSTN_MinionNetRelevancy.h"
#include "Simulation/GSTN_HitInbox.h"
#include "Spawn/GSTN_MinionSpawner.h"
#include "Store/GSTN_MinionStore.h"
#include "Subsystems/WorldSubsystem.h"
#include "Sync/GSTN_InboxEntry.h"
#include "Timeline/GSTN_TimelinePlayer.h"
#include "Tokens/GSTN_MinionAttackTokens.h"
#include "World/GSTN_StepSnapshot.h"
#include "GSTN_MinionCombatSubsystem.generated.h"

class AGSTN_MinionActor;
class AGSTN_MinionNetManager;
class AGSTN_MinionPack;
struct FGSTN_MinionNetArray;
class UGSTN_CombatRules;
class UGSTN_CombatTimeline;
class UGSTN_CombatWorldSubsystem;
class UGSTN_HitResolutionStep;
class UGSTN_HurtboxSetDefinition;
struct FGSTN_CombatTimelineData;
struct FGSTN_TimelineTrack_Event;

namespace GSTNMinion
{
	/**
	 * MoveId values of the reaction timelines the batch plays without a request. Moves use 1..MaxMoves (index + 1); the reactions sit
	 * at the top of the 6-bit wire range so they never collide with a move. G3 uses Spawn and Death only; the hit reactions (G5) are
	 * action states without a timeline (the presentation of G7 picks the clip from the state), and G6 freezes the table when the net
	 * item ships.
	 */
	namespace ReactionMoveIds
	{
		constexpr uint8 Spawn = 62;
		constexpr uint8 Death = 63;
	}

	/** Provider id of the minion subsystem in the combat world subsystem (fixed: providers append in id order on every machine) */
	constexpr int32 SnapshotProviderId = 8;

	/** Entries of the last-attacker ring of a minion (KO credit, assists; spec 5 stage 2.2) */
	constexpr int32 LastAttackerRingSize = 4;

	/** Entries of the HitBy ring of a minion: the victim-side dedupe of re-delivered hits (spec 9.2 item 4) */
	constexpr int32 HitByRingSize = 8;

	/** Entries of the attacker-side target ring of a minion's running move: the HitTargets of a fighter, 16 since G1 */
	constexpr int32 AttackTargetRingSize = 16;
}

/** The reaction tier a staggered hit left waiting for the end of the victim's hitstop (a runtime column, never on the wire) */
enum class EGSTN_MinionPendingReaction : uint8
{
	None = 0,
	StaggerInPlace,
	PushBack,
	Launch
};

/** A minion as a rewound attack query sees it at the rewind frame (UGSTN_MinionCombatSubsystem::GatherRewoundCandidates) */
struct FGSTN_MinionRewoundPose
{
	int32 EntityId = INDEX_NONE;
	uint16 Generation = 0;
	int32 Frame = INDEX_NONE;
	/** Feet, yawed; the hurtboxes hang from it like from the snapshot entry's transform */
	FTransform Transform;
	uint8 ReactionFlags = 0;
};

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
	/** Drained hits the pipeline let through (the rest: refused by a step, a ricochet, another inbox kind or a HitBy duplicate) */
	int32 HitsApplied = 0;
	int32 HitsDeduped = 0;
	/** Hits whose outcome carried extra gameplay effects, which minions ignore in v1 (STAT_GSTN_MinionIgnoredEffects) */
	int32 IgnoredEffects = 0;
	/** Moves started this frame, and starts refused because the target's attack tokens were taken */
	int32 AttacksStarted = 0;
	int32 TokensDenied = 0;
	/** Hits minions delivered to fighters' inboxes this frame */
	int32 AttackHits = 0;
	int32 TimelineEvents = 0;
	int32 Despawned = 0;
};

/** Running totals for the bench report. Wall-clock measurement only: nothing in the simulation reads them */
struct FGSTN_MinionBenchTotals
{
	int32 Batches = 0;
	double TotalBatchSeconds = 0.0;
	double MaxBatchSeconds = 0.0;
	/** Stage 4 only, and the minion-frames it stepped: their ratio is the spike 3 cost per minion-frame (cB) */
	double MovementSeconds = 0.0;
	int64 MovementMinionFrames = 0;
	/** Minions freed for falling below the world's KillZ */
	int32 FellOut = 0;
	/** Spike 5: stage 2.2 time and the hits it drained (the 12-minion cleave cost is their ratio times 12) */
	double DrainSeconds = 0.0;
	int64 DrainedHits = 0;
	/** Spike 5: rewound candidate queries, their time, the candidates the hash gave and those the swept bound rejected */
	int64 LagCompQueries = 0;
	double LagCompSeconds = 0.0;
	int64 LagCompCandidates = 0;
	int64 LagCompRejected = 0;
	int64 IgnoredEffects = 0;
	/** G5 gate: attack starts, starts refused for a taken token and hits delivered to fighters (the harness scenarios read them) */
	int64 AttacksStarted = 0;
	int64 TokensDenied = 0;
	int64 AttackHits = 0;
};

/** What a MutatePack mutator changed: the array indices it wrote and whether it added or removed items */
struct FGSTN_MinionPackMutation
{
	TArray<int32, TInlineAllocator<16>> ChangedItems;
	bool bStructureChanged = false;
};

/** Replication counters of the authority (G6), for the bench, the harness dump and the tests. Nothing in the batch reads them */
struct FGSTN_MinionNetStats
{
	int64 ItemUpdates = 0;
	int64 ItemAdds = 0;
	int64 ItemRemoves = 0;
	/** Frames a pack had at least one item marked */
	int64 PackSends = 0;
	int64 Wakes = 0;
	int64 Flushes = 0;
	int64 Sleeps = 0;
	int64 RelevancyPasses = 0;
	/** Last pass, summed over the connections */
	int32 LastGranted = 0;
	int32 LastDeniedByBudget = 0;
	int32 LastOutOfRange = 0;
	int32 LastGrantedItems = 0;
	/** Item updates sent to a connection for a member beyond the relevant distance of it, over all sent (8.1, re-pack trigger above 30 %) */
	FGSTN_MinionOverInclusion OverInclusion;
	uint32 RegistryHash = 0;
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
 *
 * Hit flows (G5, spec 9):
 *  - player -> minion: stage 2 runs the rules' hit pipeline (MeterGain skipped: a minion has no meter) on a victim view built from the
 *    store, applies damage, the hitstop and the reaction tier (Hitstop, then Hitstun / PushedBack / Launched; below the stagger
 *    threshold only HitCounter moves), cancels the running move of a staggered minion, and dedupes re-deliveries with the HitBy ring;
 *  - minion -> player: stage 3 evaluates the open hitboxes of the running move against the FIGHTERS of S(F) only (end-of-F-1
 *    transforms on both sides), runs the pipeline as the attacker's prediction for the minion's own hitstop, and posts the hit to the
 *    fighter's inbox stamped F; the fighter drains it at F+1 like any hit. A minion attacker never rewinds;
 *  - attack tokens: a move against a fighter starts only with a token (at most MaxAttackersPerPlayer holders per fighter); the token
 *    goes back when the move ends, when a hit staggers the minion and when it dies;
 *  - lag compensation: the minions are in every snapshot through this provider, so the combat world subsystem's ring records them
 *    and the fighters' overlay (GSTNLagComp::BuildOverlay) rewinds them like any entity without an NP component, with no code outside
 *    this plugin. The minion history (FGSTN_MinionHistory, written in stage 5) adds the cheap candidate query of spec 9.2 for a pose
 *    source that wants it (GatherRewoundCandidates) and the spike 5 measurement.
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

	/**
	 * Gives a live minion a launch velocity, consumed by its next movement step (stage 4). bPushBack slides it on the ground
	 * (decaying, stops at walls and ledges, never airborne); otherwise it flies under gravity until it lands. Sets the action state
	 * to PushedBack or Launched (a dying minion keeps its state); the backend returns it to Idle when the knockback ends.
	 * The drain calls this for the reaction tiers once the victim hitstop is over (and cancels the move itself); a script launch leaves a
	 * running timeline alone. Scripts and the bench may call it outside a stage. False for a stale handle
	 */
	bool LaunchMinion(const FGSTN_MinionHandle& Handle, const FVector& Velocity, bool bPushBack);

	/** Gravity acceleration at a position for every minion (a gravity field). Unset (empty function) = the world's gravity Z */
	void SetGravitySource(TFunction<FVector(const FVector&)> InGravityAt) { GravitySource = MoveTemp(InGravityAt); }

	// Hits (G5)

	/**
	 * The rules minions are hit and hit with. Set before minions take hits (a game mode at begin play); unset, Activate takes the project's
	 * default rules (UGSTN_CombatSettings). The rules must have their hit pipeline resolved; the batch only runs the resolved list
	 */
	void SetCombatRules(UGSTN_CombatRules* Rules);
	const UGSTN_CombatRules* GetCombatRules() const;

	/**
	 * The optional pipeline step slot (bridge seam for parts on minions, M4b P16): a step class run for minion victims right before the
	 * rules' ComputeKnockback step (at the end when the rules have none). Null (the default) runs the rules' pipeline unchanged. Same
	 * contract as every hit step: pure, deterministic, class default object only
	 */
	void SetExtraVictimStep(TSubclassOf<UGSTN_HitResolutionStep> StepClass) { ExtraVictimStep = StepClass; }

	/**
	 * Store hook after a drained hit was written into the store (health, state, rings), before the next entry: the slot for a bridge that
	 * keeps per-minion data of its own (M4b P16: part damage from the outcome extensions). Called from inside stage 2: a listener reads
	 * and writes its own data only; it must not spawn, despawn, write intents or touch the store. Inert unless bound
	 */
	DECLARE_MULTICAST_DELEGATE_FourParams(FOnMinionOutcomeApplied, const FGSTN_MinionHandle& /*Minion*/, const FGSTN_HitInboxEntry& /*Hit*/, const FGSTN_HitContextView& /*Resolved*/, int32 /*Frame*/);
	FOnMinionOutcomeApplied OnOutcomeApplied;

	/** The victim view the pipeline reads for a live minion at Frame (the store as it is now): kind tag, health fraction, weight, hurt state */
	bool BuildVictimView(const FGSTN_MinionHandle& Handle, int32 Frame, FGSTN_HitVictimView& OutView) const;

	/** The attack tokens of the minions against fighters. MaxPerTarget is gstn.Minion.MaxAttackersPerPlayer at activation */
	const FGSTN_MinionAttackTokens& GetAttackTokens() const { return AttackTokens; }
	void SetMaxAttackersPerTarget(int32 MaxAttackers) { AttackTokens.SetMaxPerTarget(MaxAttackers); }

	// Per-slot hit state (authority columns, for tests, debug and the bridges)
	int32 GetHitstopEndFrame(int32 Slot) const { return HitstopEndFrame.IsValidIndex(Slot) ? HitstopEndFrame[Slot] : INDEX_NONE; }
	int32 GetHitstunEndFrame(int32 Slot) const { return HitstunEndFrame.IsValidIndex(Slot) ? HitstunEndFrame[Slot] : INDEX_NONE; }
	int32 GetAttackerHitstopEndFrame(int32 Slot) const { return AttackerHitstopEndFrame.IsValidIndex(Slot) ? AttackerHitstopEndFrame[Slot] : INDEX_NONE; }
	int32 GetPausedFrames(int32 Slot) const { return PausedFrames.IsValidIndex(Slot) ? PausedFrames[Slot] : 0; }
	float GetKnockbackPercent(int32 Slot) const { return KnockbackPercent.IsValidIndex(Slot) ? KnockbackPercent[Slot] : 0.f; }
	EGSTN_MinionPendingReaction GetPendingReaction(int32 Slot) const
	{
		return PendingReaction.IsValidIndex(Slot) ? static_cast<EGSTN_MinionPendingReaction>(PendingReaction[Slot]) : EGSTN_MinionPendingReaction::None;
	}
	/** The attacker of the Age-th latest hit (0 = latest), INDEX_NONE when the ring holds fewer */
	int32 GetLastAttacker(int32 Slot, int32 Age = 0) const;

	// Lag compensation (G5, spike 5)

	/**
	 * The minions a rewound hitbox could touch: candidates from the current position hash inside Bounds inflated by
	 * gstn.Minion.LagComp.MaxSpeed x the rewind age, rejected by the swept bound of the rewind frame (FGSTN_LagCompTime::FrameOfTime of
	 * RewindMs), and the survivors sampled at that frame. Ascending slot order. Only for a remote player's attack on the authority
	 * (GSTNLagComp::ShouldRewind): the host and AI attackers, minions included, hit the present and never call it.
	 */
	void GatherRewoundCandidates(const FBox& Bounds, float RewindMs, int32 FixedStepMS, TArray<FGSTN_MinionRewoundPose>& OutPoses);

	const FGSTN_MinionHistory& GetHistory() const { return History; }

	FGSTN_MinionMovementSettings& GetMovementSettings() { return MovementSettings; }
	const FGSTN_MinionMovementColumns& GetMovementColumns() const { return MoveColumns; }

	/**
	 * C++ hook at the end of stage 4: the frame, the slots it moved (ascending) and the store positions before the stage, by slot
	 * (only filled while someone is bound). Called from inside the batch: a listener may call LaunchMinion (applied next frame)
	 * but must not spawn, despawn or write intents. The bench's robustness counters use it.
	 */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnMinionMovementStepped, int32 /*Frame*/, TConstArrayView<int32> /*Slots*/, TConstArrayView<FVector3f> /*PreviousPositionBySlot*/);
	FOnMinionMovementStepped OnMovementStepped;

	/**
	 * C++ hook (G8): a slot became active (its spawn was drained and its actor acquired). The brain subsystem starts the type's brain
	 * from it. Fires inside the batch: a listener may write intents but must not spawn or despawn
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMinionActivated, const FGSTN_MinionHandle& /*Minion*/);
	FOnMinionActivated OnMinionActivated;

	/**
	 * C++ hook (G8): a minion went from Far or Dormant to Combat (a hit, a launch, a forced wake), bypassing hysteresis and dwell.
	 * The brain subsystem ticks that brain on the same world tick. Fires inside the batch (or from LaunchMinion)
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMinionWoken, const FGSTN_MinionHandle& /*Minion*/);
	FOnMinionWoken OnMinionWoken;

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

	/** True for a live slot whose queued spawn has been activated (a pending slot is claimed but not in the world yet) */
	bool IsSlotActive(int32 SlotIndex) const { return Store.IsSlotAlive(SlotIndex) && !PendingActivation[SlotIndex]; }

	/** The registered definition of a store TypeIndex, null when out of range */
	const UGSTN_MinionDefinition* GetDefinition(int32 TypeIndex) const;

	/** Number of pooled (free) actors */
	int32 GetNumPooledActors() const { return FreeActors.Num(); }

	/**
	 * C++ hook: an event track of a minion's timeline was crossed this frame (fires once per crossing, whatever frames the LOD
	 * skipped). Called from inside stage 3: a listener must not spawn, despawn or write intents. The presentation (G7) routes cues here
	 */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnMinionTimelineEvent, const FGSTN_MinionHandle& /*Minion*/, const FGSTN_TimelineTrack_Event& /*Track*/, int32 /*TimelineFrame*/);
	FOnMinionTimelineEvent OnTimelineEvent;

	// Replication (G6, spec 8.1-8.3). Authority only and outside the batch stages: TickBatch publishes after the events are flushed,
	// so no stage sees a pack and no stage needs an IsServer branch (the subsystem is only active with authority)

	/**
	 * The ONLY way pack items change. Order, enforced here (spec 8.1, R2): a dormant pack is woken (SetNetDormancy(DORM_Awake), when
	 * its ceiling is above 0) or flushed (FlushNetDormancy, when it stays all-T3) FIRST, then Mutate runs, then MarkItemDirty for
	 * every changed index (MarkArrayDirty for an add or remove), then the push-model property mark. Epic warns that modifying a
	 * dormant FastArray and flushing afterwards may replicate nothing; one entry point makes the wrong order impossible
	 */
	void MutatePack(AGSTN_MinionPack& Pack, TFunctionRef<void(FGSTN_MinionNetArray& /*Array*/, FGSTN_MinionPackMutation& /*Mutation*/)> Mutate);

	/** IsNetRelevantFor of a pack: the viewer's budgeted grant once evaluated, before that any member within the relevant distance */
	bool IsPackNetRelevantFor(const AGSTN_MinionPack& Pack, const AActor* RealViewer, const FVector& SrcLocation) const;

	AGSTN_MinionPack* GetPack(int32 PackId) const { return Packs.IsValidIndex(PackId) ? Packs[PackId].Get() : nullptr; }
	AGSTN_MinionPack* GetPackOfSlot(int32 Slot) const { return SlotPackId.IsValidIndex(Slot) ? GetPack(SlotPackId[Slot]) : nullptr; }
	int32 GetItemIdOfSlot(int32 Slot) const { return SlotItemId.IsValidIndex(Slot) ? SlotItemId[Slot] : INDEX_NONE; }
	int32 GetNumPacks() const;
	AGSTN_MinionNetManager* GetNetManager() const { return NetManager; }
	const FGSTN_MinionNetStats& GetNetStats() const { return NetStats; }

	/** Position origin of the wire (8.2). Set before the first spawn; the clients get it through the net manager */
	void SetNetWorldOrigin(const FVector& Origin);

	/** Runs the relevancy pass now (normally every gstn.Minion.Net.RelevancyFrames frames from TickBatch) */
	void UpdateNetRelevancy(int32 Frame);

	/** Log lines for the harness (gstn.Minion.Net.Dump): one per item, the grants per remote player and a summary */
	void DumpNetToLog() const;

	/** Test spy: when set, MutatePack and the dormancy changes append Wake, Flush, Mutate, MarkItem, MarkProperty, Sleep in order */
	TArray<FName>* NetTraceForTests = nullptr;

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
		/** Copied from the hit payload when there is one (0 otherwise): the third key of the drain order, and the hurtbox for the parts bridge */
		uint8 AttackerHitboxIndex = 0;
		uint8 HurtboxIndex = 0;
		FGSTN_InboxEntry Entry;
	};

	/**
	 * The victim-side identity of a hit (spec 9.2 item 4). The spec's key is (AttackerId, TimelineStartFrame, HitGroup); the inbox entry
	 * carries neither of the last two, so the batch keys on (AttackerId, the overlap frame, the attacker's hitbox index): a re-delivery of
	 * the same hit matches it exactly, and two distinct hits never do (the attacker's own target ring forbids the same hitbox hitting the
	 * same victim twice on one frame)
	 */
	struct FHitByKey
	{
		int32 AttackerId = INDEX_NONE;
		int32 TimelineKey = INDEX_NONE;
		uint8 HitGroup = 0;

		bool operator==(const FHitByKey& Other) const { return AttackerId == Other.AttackerId && TimelineKey == Other.TimelineKey && HitGroup == Other.HitGroup; }
	};

	/** One victim of a minion's running move: the HitTargets entry of a fighter (victim, move start, group key) */
	struct FAttackTarget
	{
		int32 VictimId = INDEX_NONE;
		int32 MoveStartFrame = INDEX_NONE;
		uint8 TargetKey = 0;
	};

	/** A Blueprint event a stage queued; broadcast after stage 5 */
	struct FQueuedEvent
	{
		enum class EKind : uint8 { Hit, Died, LODChanged, KO } Kind = EKind::Hit;
		FGSTN_MinionHandle Handle;
		float Damage = 0.f;
		int32 OtherId = INDEX_NONE;
		int32 Frame = INDEX_NONE;
		uint8 OldTier = 0;
		uint8 NewTier = 0;
	};

	/** A move a scheduled slot may start this frame, waiting for the token resolve of stage 3 */
	struct FStartCandidate
	{
		int32 Slot = INDEX_NONE;
		int32 MoveIndex = INDEX_NONE;
		int32 TargetId = INDEX_NONE;
		bool bNeedsToken = false;
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

	// Hits (G5)

	/** The world combat subsystem the minions are registered with, null when there is none */
	UGSTN_CombatWorldSubsystem* GetCombatWorld() const;

	/** The snapshot entry of a live, activated slot as the store holds it now (transform, hurt states, reaction state). False for a slot of no type */
	bool FillSnapshotEntry(int32 Slot, int32 Frame, FGSTN_SnapshotEntry& Out) const;

	/** The victim view of a slot from its snapshot entry, with the store's current health, percent and weight */
	FGSTN_HitVictimView MakeMinionView(int32 Slot, const FGSTN_SnapshotEntry& Entry) const;

	/** Root of a slot: the feet, yawed, tilted to the gravity up */
	FTransform RootOf(int32 Slot) const;
	FTransform MakeRoot(const FVector& Feet, float YawDegrees) const;
	FVector UpAt(const FVector& Position) const;

	/** The rules' resolved pipeline minus MeterGain, with the extra victim step before ComputeKnockback (see SetExtraVictimStep) */
	void RunVictimPipeline(FGSTN_HitResolutionContext& Context) const;

	/** One drained hit on a slot (pipeline, store writes, reaction, rings, events); Self is the slot's entry of this drain */
	void ApplyHit(int32 Slot, int32 Frame, const FGSTN_HitInboxEntry& Hit, const FGSTN_SnapshotEntry& Self);

	/** Turns a reaction tier into the action state (Hitstun) or a launch of the movement backend (PushedBack, Launched) */
	void ApplyReaction(int32 Slot, EGSTN_MinionPendingReaction Reaction, const FVector& Velocity);

	/** End of the victim's hitstop (the pending reaction goes out) and of a stagger in place (back to Idle). Start of stage 3 */
	void StepVictimReaction(int32 Slot, int32 Frame);

	/** Cancels a running move or spawn timeline (a staggered hit, a ricochet): cooldown stamp, token back, clock reset. Death is never cancelled */
	void CancelTimeline(int32 Slot, int32 Frame);

	/** Whether the slot may start its intent's command at Frame, all checks but the token; the move index and target when it may */
	bool CanStartCommand(int32 Slot, int32 Frame, const FGSTN_StepSnapshot* Snapshot, int32& OutMoveIndex, int32& OutTargetId) const;

	/** The fighters' snapshot of Frame (S(F)), built here when no fighter ticked; null without fighters or a combat world */
	const FGSTN_StepSnapshot* AcquireFighterSnapshot(int32 Frame);

	/** The open hitboxes of a slot's move at TimelineFrame against the fighters of Snapshot; posts the hits and opens the attacker hitstop */
	void RunAttackQueries(int32 Slot, int32 Frame, const FGSTN_CombatTimelineData& Timeline, int32 TimelineFrame, const FGSTN_StepSnapshot& Snapshot);

	bool HasAttackTarget(int32 Slot, int32 VictimId, int32 MoveStartFrame, uint8 TargetKey) const;
	int32 CountAttackTargets(int32 Slot, int32 MoveStartFrame, uint8 TargetKey) const;
	void AddAttackTarget(int32 Slot, int32 VictimId, int32 MoveStartFrame, uint8 TargetKey);

	bool HasHitBy(int32 Slot, const FHitByKey& Key) const;
	void AddHitBy(int32 Slot, const FHitByKey& Key);
	void PushLastAttacker(int32 Slot, int32 AttackerId);

	/** Resets the G5 columns of a slot (activation and free) */
	void ResetHitColumns(int32 Slot);

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
	/** Velocity, ground flag, pending launch and knockback mode of the movement backend */
	FGSTN_MinionMovementColumns MoveColumns;

	// Hit columns (G5). Authority only, never on the wire (the action state and HitCounter are what the clients get)

	/** Damage percent the minion took, in the percent damage modes: what the weight driven knockback grows with (0 in the Health mode) */
	TArray<float> KnockbackPercent;
	/** Victim hitstop: frames before this one are frozen (the window opens on the hit's frame, the fighter rule) */
	TArray<int32> HitstopEndFrame;
	/** End of a stagger in place (Hitstun back to Idle); push back and launch end when the movement does */
	TArray<int32> HitstunEndFrame;
	/** EGSTN_MinionPendingReaction and its velocity, released when the victim hitstop ends */
	TArray<uint8> PendingReaction;
	TArray<FVector3f> PendingReactionVelocity;
	/** The minion's own hitstop as an attacker: its move clock stands still up to and including this frame */
	TArray<int32> AttackerHitstopEndFrame;
	/** Frames the running timeline stood still so far (the attacker hitstop): timeline frame = Frame - MoveStartFrame - PausedFrames */
	TArray<int32> PausedFrames;
	/** [Slot * LastAttackerRingSize + i], newest at LastAttackerHead[Slot] */
	TArray<int32> LastAttackerRing;
	TArray<uint8> LastAttackerHead;
	TArray<uint8> LastAttackerCount;
	/** [Slot * HitByRingSize + i], oldest overwritten */
	TArray<FHitByKey> HitByRing;
	TArray<uint8> HitByHead;
	/** [Slot * AttackTargetRingSize + i], the running move's victims (keyed by its start frame, so a new move needs no clear) */
	TArray<FAttackTarget> AttackTargets;
	TArray<uint8> AttackTargetHead;

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_CombatRules> CombatRules;

	UPROPERTY(Transient)
	TSubclassOf<UGSTN_HitResolutionStep> ExtraVictimStep;

	TWeakObjectPtr<UGSTN_CombatWorldSubsystem> CombatWorld;
	FGSTN_MinionAttackTokens AttackTokens;
	FGSTN_MinionHistory History;

	// Movement (stage 4). Backend A (Mover, non-predicted) is not built: G4, spike 3 decision B
	FGSTN_MinionMovement_Lightweight LightweightMovement;
	FGSTN_MinionMovementSettings MovementSettings;
	TFunction<FVector(const FVector&)> GravitySource;
	FGSTN_MinionMoveContext MoveContext;
	TArray<int32> MoveSlots;
	TArray<FVector3f> PreviousPositions;
	bool bWarnedMoverBackend = false;

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
	/** Hit side of a timeline evaluation (attack queries); the snapshot's hurt states use their own, the provider is const */
	FGSTN_TimelineFrameResult HitScratch;
	mutable FGSTN_TimelineFrameResult HurtScratch;
	TArray<FStartCandidate> StartCandidates;
	TArray<int32> CandidateScratch;
	TArray<int32> HashSlots;

	FGSTN_MinionLODParams DefaultLODParams;
	int32 ForcedLODTier = INDEX_NONE;
	float FixedStepSeconds = 1.f / 62.5f;
	int32 LastBatchFrame = INDEX_NONE;
	/** Frame of the batch being run (equals LastBatchFrame between batches): the frame LOD log lines carry */
	int32 CurrentBatchFrame = INDEX_NONE;

	// Replication (G6), see GSTN_MinionCombatSubsystem_Net.cpp
	struct FViewerRelevancy
	{
		TWeakObjectPtr<const AActor> Viewer;
		FGSTN_MinionRelevancyState State;
	};

	void NetActivate();
	void NetDeinitialize();
	/** End of TickBatch: packs for new slots, the per-item sends of every pack, dormancy, and the relevancy pass on its cadence */
	void NetPublish(int32 Frame);
	void NetAssignSlot(int32 Slot, int32 Frame);
	/** FreeSlot: the slot's item leaves its pack (an emptied pack is destroyed) */
	void NetReleaseSlot(int32 Slot);
	/** RegisterDefinition: the type table changed, the manager gets the new paths and hash */
	void NetOnRegistryChanged();
	FGSTN_MinionNetWireItem NetBuildWire(int32 Slot, int32 Frame) const;
	AGSTN_MinionPack* NetSpawnPack(int32 SpawnGroup);
	void NetTrace(const TCHAR* Step);

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGSTN_MinionPack>> Packs;

	UPROPERTY(Transient)
	TObjectPtr<AGSTN_MinionNetManager> NetManager;

	/** Per slot: the spawn group (pack assignment), the pack and pack-local item id (INDEX_NONE before assignment) */
	TArray<int32> SlotSpawnGroup;
	TArray<int32> SlotPackId;
	TArray<int32> SlotItemId;
	/** Item updates marked since the last relevancy pass (over-inclusion metric) */
	TArray<uint16> SlotNetUpdates;
	/** Scratch of the relevancy pass: distance of each slot to the viewer being evaluated */
	TArray<float> SlotViewerDistance;
	TArray<FViewerRelevancy> ViewerRelevancy;
	TArray<FGSTN_MinionPackCandidate> RelevancyCandidates;
	TArray<int32> NetSentScratch;
	FVector3f NetWorldOrigin = FVector3f::ZeroVector;
	int32 LastRelevancyFrame = INDEX_NONE;
	FGSTN_MinionNetStats NetStats;

	FGSTN_MinionBatchStats LastBatch;
	FGSTN_MinionBenchTotals BenchTotals;
};
