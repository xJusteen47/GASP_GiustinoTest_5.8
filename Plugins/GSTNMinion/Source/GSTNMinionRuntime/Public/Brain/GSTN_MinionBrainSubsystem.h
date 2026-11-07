// Minion runtime (M8 G8): the brain, authority only.

#pragma once

#include "CoreMinimal.h"
#include "Brain/GSTN_MinionBrain.h"
#include "StateTreeInstanceData.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_MinionBrainSubsystem.generated.h"

class UGSTN_MinionCombatSubsystem;
class UGSTN_MinionDefinition;
class UStateTree;
struct FStateTreeDataView;
struct FStateTreeExecutionContext;
struct FStateTreeExternalDataDesc;

/** A thing a brain may target: a pipeline entity id and where it stands */
struct FGSTN_MinionBrainTarget
{
	int32 Id = INDEX_NONE;
	FVector Position = FVector::ZeroVector;
};

/** Numbers for gstn.Minion.Brain.Report and the spike 6 test. Wall clock only: nothing in the simulation reads them */
struct FGSTN_MinionBrainStats
{
	int32 Brained = 0;
	/** Slots whose brain is a StateTree (the rest run the native brain) */
	int32 StateTreeSlots = 0;
	/** Brain ticks in the last full second of real time */
	int32 TicksLastSec = 0;
	int64 TotalTicks = 0;
	double TotalSeconds = 0.0;
	double MaxTickSeconds = 0.0;
	/** Wakes (Far or Dormant straight to Combat) of brained minions */
	int32 Wakes = 0;
};

/**
 * Runs the minions' brains (M8 G8). A brain is a StateTree of UGSTN_MinionStateTreeSchema, or the hand-written native chase and
 * swing; either way it reads a FGSTN_MinionBrainContext and writes a pending intent, which this subsystem commits with SetIntent
 * (one call per ticked slot). The brain never touches the store: the batch reads the intent and decides, so a stale or dead brain
 * is harmless (the intent goes stale after IntentStaleFrames and the minion idles).
 *
 * Why a subsystem and not a StateTree component per minion: a crowd of 40 to 150 would pay a ticking component, an AI controller
 * and a per-component execution context each, and the cost would not follow the LOD. Here one world tick walks the brained slots
 * (SoA arrays by slot, one FStateTreeInstanceData each), runs only the slots the budget allows (T0 every frame, T1 every 4th, T2
 * every 16th, Dormant never, spread by slot so the cost per frame is flat; a wake forces one tick) and builds one execution
 * context per ticked slot. It runs in the world tick, outside the fixed tick, exactly like the bench: latest intent wins.
 *
 * Authority only: inactive wherever UGSTN_MinionCombatSubsystem::GetActive is null (a client world). Target selection is the
 * nearest registered fighter (or the scripted provider), re-chosen at most twice a second per slot. Movement toward the target
 * follows an async navigation path at most twice a second when a navigation system with a navmesh exists, else seeks directly.
 *
 * Console: gstn.Minion.Brain.Native <0|1> (force the native brain), .IntervalNear, .IntervalFar, .Report, .Dump.
 */
UCLASS()
class GSTNMINIONRUNTIME_API UGSTN_MinionBrainSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UGSTN_MinionBrainSubsystem* Get(const UWorld* World);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/**
	 * Gives a minion a brain: Tree is a UStateTree (cast inside; anything else, or null, means the native brain). The slot may still be
	 * a queued spawn: it is skipped until it activates. Restarts the brain when it already had one. False for a stale handle
	 */
	bool StartBrain(const FGSTN_MinionHandle& Handle, UObject* Tree);

	/** Starts the brain the definition of the minion's type names (UGSTN_MinionDefinition::Brain). False when the type has none */
	bool StartBrainForDefinition(const FGSTN_MinionHandle& Handle);

	/** Takes the brain away. Its last intent stays and goes stale on its own */
	bool StopBrain(const FGSTN_MinionHandle& Handle);

	bool HasBrain(const FGSTN_MinionHandle& Handle) const;

	/**
	 * One brain pass (what the world tick does). Public so a test can drive it with a world that does not tick; DeltaSeconds is the
	 * time since the previous pass, handed to the trees
	 */
	void TickBrains(float DeltaSeconds);

	/** Test and tooling: replaces the target source (default: the registered fighters of the combat world). Pass an empty function to restore it */
	void SetTargetProvider(TFunction<void(TArray<FGSTN_MinionBrainTarget>&)> Provider) { TargetProvider = MoveTemp(Provider); }

	const FGSTN_MinionBrainStats& GetStats() const { return Stats; }
	void ResetStats();

	/** Brain ticks of the slot since its brain started (0 for a slot without one) */
	int32 GetSlotTickCount(int32 Slot) const { return SlotTicks.IsValidIndex(Slot) ? SlotTicks[Slot] : 0; }

	/** Engine tick counter value of the slot's last brain tick (INDEX_NONE if never), and the current counter */
	int32 GetSlotLastTick(int32 Slot) const { return SlotLastTick.IsValidIndex(Slot) ? SlotLastTick[Slot] : INDEX_NONE; }
	int32 GetTickCounter() const { return TickCounter; }

	/** Pipeline id of the slot's current target, INDEX_NONE for none */
	int32 GetSlotTarget(int32 Slot) const { return TargetIds.IsValidIndex(Slot) ? TargetIds[Slot] : INDEX_NONE; }

	/** The state name the dump prints: the active StateTree state, or the native phase; "NotStarted" before the first tick */
	FString GetSlotStateName(int32 Slot);

	/** True when the slot runs a StateTree this pass (a tree is set and the native console variable is off) */
	bool IsSlotUsingStateTree(int32 Slot) const;

	/** Logs `MinionBrain mode=<StateTree|Native> brained=.. ticksLastSec=.. usPerTick=.. maxUsPerTick=.. wakes=..` */
	void Report() const;

	/** Logs one line per brained slot: `MinionBrain slot=.. id=.. tier=.. state=.. target=.. ticks=.. lastTick=.. now=..` */
	void Dump();

private:
	void EnsureBound();
	void EnsureCapacity(int32 Capacity);
	void ClearSlot(int32 Slot);
	void HandleActivated(const FGSTN_MinionHandle& Handle);
	void HandleWoken(const FGSTN_MinionHandle& Handle);
	void RunSlot(UGSTN_MinionCombatSubsystem& Minions, int32 Slot, float DeltaSeconds);
	void ResolveTarget(int32 Slot, FGSTN_MinionBrainContext& Context);
	void GatherNeighbours(const UGSTN_MinionCombatSubsystem& Minions, int32 Slot, const FVector2D& Position);
	void UpdateNavigation(int32 Slot, FGSTN_MinionBrainContext& Context);
	void BuildTargets();
	UStateTree* ResolveTypeTree(const UGSTN_MinionDefinition* Definition, int32 TypeIndex);
	bool CollectExternalData(const FStateTreeExecutionContext& Context, const UStateTree* StateTree, TArrayView<const FStateTreeExternalDataDesc> Descs, TArrayView<FStateTreeDataView> OutViews);

	TWeakObjectPtr<UGSTN_MinionCombatSubsystem> BoundMinions;
	FDelegateHandle ActivatedHandle;
	FDelegateHandle WokenHandle;

	// Per-slot columns (store capacity entries). Brained says which slots have a brain at all
	TBitArray<> Brained;
	TBitArray<> Woken;
	TBitArray<> TreeStarted;
	TBitArray<> HasWaypoint;
	TBitArray<> PathPending;
	TArray<uint16> BrainGen;
	TArray<FGSTN_MinionNativeBrain> Native;
	TArray<int32> TargetIds;
	TArray<double> NextTargetPick;
	TArray<double> LastTickClock;
	TArray<double> NextPathTime;
	TArray<FVector> Waypoint;
	TArray<int32> SlotTicks;
	TArray<int32> SlotLastTick;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStateTree>> Trees;

	UPROPERTY(Transient)
	TArray<FStateTreeInstanceData> Instances;

	/** Tree of each minion type (index = store TypeIndex), resolved once: loading is synchronous, so it is paid on the first spawn of a type */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStateTree>> TypeTrees;
	TBitArray<> TypeResolved;

	TFunction<void(TArray<FGSTN_MinionBrainTarget>&)> TargetProvider;
	TArray<FGSTN_MinionBrainTarget> Targets;
	bool bTargetsBuilt = false;

	FGSTN_MinionBrainContext ContextScratch;
	TArray<FVector2D> NeighbourScratch;
	TArray<int32> DropScratch;
	FGSTN_MinionBrainContext* ActiveContext = nullptr;
	FGSTN_MinionBrainBudget Budget;

	int32 BrainedCount = 0;
	int32 TickCounter = 0;
	/** Sum of the DeltaSeconds handed to TickBrains: the brain's own clock (target refresh, path refresh), independent of the world's */
	double Clock = 0.0;

	FGSTN_MinionBrainStats Stats;
	double WindowStartSeconds = 0.0;
	int32 WindowTicks = 0;

	// Navigation (async path at <= 2 Hz). Checked once per pass
	bool bNavChecked = false;
	bool bNavReady = false;
};
