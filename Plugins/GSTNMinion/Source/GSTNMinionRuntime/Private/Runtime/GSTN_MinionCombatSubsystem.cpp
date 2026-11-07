// Minion runtime (authority batch, M8 G3).

#include "Runtime/GSTN_MinionCombatSubsystem.h"

#include "Algo/StableSort.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GSTNMinionCoreModule.h"
#include "HAL/IConsoleManager.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "NetworkPredictionWorldManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Runtime/GSTN_MinionActor.h"
#include "Runtime/GSTN_MinionCombatComponent.h"
#include "Schedule/GSTN_MinionSchedule.h"
#include "Simulation/GSTN_HitInbox.h"
#include "Stats/Stats.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "World/GSTN_CombatWorldSubsystem.h"

DECLARE_STATS_GROUP(TEXT("GSTNMinion"), STATGROUP_GSTNMinion, STATCAT_Advanced);

DECLARE_CYCLE_STAT(TEXT("Batch"), STAT_GSTN_MinionBatch, STATGROUP_GSTNMinion);
DECLARE_CYCLE_STAT(TEXT("0 Spawn drain"), STAT_GSTN_MinionSpawnDrain, STATGROUP_GSTNMinion);
DECLARE_CYCLE_STAT(TEXT("1 LOD update"), STAT_GSTN_MinionLOD, STATGROUP_GSTNMinion);
DECLARE_CYCLE_STAT(TEXT("2 Inbox drain"), STAT_GSTN_MinionInbox, STATGROUP_GSTNMinion);
DECLARE_CYCLE_STAT(TEXT("3 Timeline advance"), STAT_GSTN_MinionTimeline, STATGROUP_GSTNMinion);
DECLARE_CYCLE_STAT(TEXT("4 Movement"), STAT_GSTN_MinionMovement, STATGROUP_GSTNMinion);
DECLARE_CYCLE_STAT(TEXT("5 Publish"), STAT_GSTN_MinionPublish, STATGROUP_GSTNMinion);
DECLARE_CYCLE_STAT(TEXT("Snapshot append"), STAT_GSTN_MinionSnapshot, STATGROUP_GSTNMinion);

DECLARE_DWORD_COUNTER_STAT(TEXT("Alive"), STAT_GSTN_MinionAlive, STATGROUP_GSTNMinion);
DECLARE_DWORD_COUNTER_STAT(TEXT("Scheduled this frame"), STAT_GSTN_MinionScheduled, STATGROUP_GSTNMinion);
DECLARE_DWORD_COUNTER_STAT(TEXT("Hits drained"), STAT_GSTN_MinionHitsDrained, STATGROUP_GSTNMinion);
DECLARE_DWORD_COUNTER_STAT(TEXT("Spawns this frame"), STAT_GSTN_MinionSpawns, STATGROUP_GSTNMinion);
DECLARE_DWORD_COUNTER_STAT(TEXT("Timeline events"), STAT_GSTN_MinionEvents, STATGROUP_GSTNMinion);
DECLARE_DWORD_COUNTER_STAT(TEXT("T0 Combat"), STAT_GSTN_MinionT0, STATGROUP_GSTNMinion);
DECLARE_DWORD_COUNTER_STAT(TEXT("T1 Near"), STAT_GSTN_MinionT1, STATGROUP_GSTNMinion);
DECLARE_DWORD_COUNTER_STAT(TEXT("T2 Far"), STAT_GSTN_MinionT2, STATGROUP_GSTNMinion);
DECLARE_DWORD_COUNTER_STAT(TEXT("T3 Dormant"), STAT_GSTN_MinionT3, STATGROUP_GSTNMinion);

namespace
{
	TAutoConsoleVariable<int32> CVarSpawnsPerFrame(
		TEXT("gstn.Minion.SpawnsPerFrame"), 4,
		TEXT("Most queued minion spawns the batch activates per frame (FIFO). Spreads a wave over frames instead of one hitch."));

	TAutoConsoleVariable<int32> CVarCorpseFrames(
		TEXT("gstn.Minion.CorpseFrames"), 60,
		TEXT("Frames a dying minion stays before it is freed when its type has no Death timeline."));

	/** States in which the minion follows its intent (anything else is a reaction or a timeline that owns the minion) */
	bool IsLocomotionState(uint8 State)
	{
		const EGSTN_MinionActionState S = static_cast<EGSTN_MinionActionState>(State);
		return S == EGSTN_MinionActionState::Idle || S == EGSTN_MinionActionState::Walk || S == EGSTN_MinionActionState::Move;
	}

	bool IsMoveId(uint8 MoveId)
	{
		return MoveId >= 1 && MoveId <= GSTNMinion::MaxMoves;
	}

	bool IsDyingOrDead(uint8 State)
	{
		const EGSTN_MinionActionState S = static_cast<EGSTN_MinionActionState>(State);
		return S == EGSTN_MinionActionState::Dying || S == EGSTN_MinionActionState::Dead;
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Lifetime

UGSTN_MinionCombatSubsystem* UGSTN_MinionCombatSubsystem::GetActive(const UWorld* World)
{
	UGSTN_MinionCombatSubsystem* Subsystem = World ? World->GetSubsystem<UGSTN_MinionCombatSubsystem>() : nullptr;
	return Subsystem && Subsystem->bActive ? Subsystem : nullptr;
}

bool UGSTN_MinionCombatSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_MinionCombatSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// The net mode is only known here. A client never activates: minions are an authority simulation, clients get packs (G6)
	if (InWorld.GetNetMode() != NM_Client)
	{
		Activate();
	}
}

void UGSTN_MinionCombatSubsystem::Activate()
{
	if (bActive)
	{
		return;
	}
	bActive = true;

	Store.ResetToConfiguredCapacity();
	const int32 Capacity = Store.Capacity();
	PendingActivation.Init(false, Capacity);
	LastRunFrame.Init(INDEX_NONE, Capacity);
	TimelinePrevFrame.Init(INDEX_NONE, Capacity);
	NextMoveFrame.Init(0, Capacity * GSTNMinion::MaxMoves);
	LastAttackerId.Init(INDEX_NONE, Capacity);
	SlotActors.Init(nullptr, Capacity);
	Scheduled.Reserve(Capacity);

	UWorld* World = GetWorld();
	if (UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(World))
	{
		bProviderRegistered = Combat->RegisterProvider(this);
		if (!bProviderRegistered)
		{
			UE_LOG(LogGSTNMinion, Warning, TEXT("Minion subsystem: provider id %d is taken; minions cannot be hit in this world"), GetProviderId());
		}
	}
	if (UNetworkPredictionWorldManager* Manager = World ? World->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr)
	{
		PostFixedTickHandle = Manager->OnPostFixedTick.AddUObject(this, &UGSTN_MinionCombatSubsystem::HandlePostFixedTick);
	}
	UE_LOG(LogGSTNMinion, Log, TEXT("Minion subsystem active (capacity %d, provider %s, NP hook %s)"), Capacity,
		bProviderRegistered ? TEXT("registered") : TEXT("none"), PostFixedTickHandle.IsValid() ? TEXT("bound") : TEXT("none"));
}

void UGSTN_MinionCombatSubsystem::Deinitialize()
{
	if (bActive)
	{
		UWorld* World = GetWorld();
		if (bProviderRegistered)
		{
			if (UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(World))
			{
				Combat->UnregisterProvider(this);
			}
			bProviderRegistered = false;
		}
		if (PostFixedTickHandle.IsValid())
		{
			if (UNetworkPredictionWorldManager* Manager = World ? World->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr)
			{
				Manager->OnPostFixedTick.Remove(PostFixedTickHandle);
			}
			PostFixedTickHandle.Reset();
		}
		bActive = false;
	}
	SpawnQueue.Reset();
	PendingHits.Reset();
	QueuedEvents.Reset();
	DespawnList.Reset();
	SlotActors.Reset();
	FreeActors.Reset();
	Super::Deinitialize();
}

void UGSTN_MinionCombatSubsystem::HandlePostFixedTick(int32 ServerFrame, int32 EndTimeMS, bool bResimulating)
{
	if (const UNetworkPredictionWorldManager* Manager = GetWorld() ? GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr)
	{
		const int32 StepMs = Manager->GetFixedTickState().FixedStepMS;
		if (StepMs > 0)
		{
			SetFixedStepSeconds(static_cast<float>(StepMs) / 1000.f);
		}
	}
	TickBatch(ServerFrame, bResimulating);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Registry, spawning, API

int32 UGSTN_MinionCombatSubsystem::RegisterDefinition(UGSTN_MinionDefinition* Definition)
{
	const int32 Existing = Definitions.Find(Definition);
	if (Existing != INDEX_NONE)
	{
		return Existing;
	}
	// TypeIndex is a byte in the store and on the wire
	if (Definitions.Num() > MAX_uint8)
	{
		UE_LOG(LogGSTNMinion, Error, TEXT("Minion registry full (256 types); %s not registered"), *GetNameSafe(Definition));
		return INDEX_NONE;
	}

	// Resolved once here so the batch never touches a soft path. A synchronous load on the first spawn of a type can hitch: a wave
	// (G9) preloads its types before it spawns
	FGSTN_MinionResolvedType Resolved;
	Resolved.MoveTimelines.Reserve(Definition->Moves.Num());
	for (const FGSTN_MinionMove& Move : Definition->Moves)
	{
		Resolved.MoveTimelines.Add(Move.Timeline.LoadSynchronous());
	}
	Resolved.SpawnTimeline = Definition->Reactions.Spawn.LoadSynchronous();
	Resolved.DeathTimeline = Definition->Reactions.Death.LoadSynchronous();
	Resolved.Hurtboxes = Definition->HurtboxSet.LoadSynchronous();
	Resolved.BoundRadius = static_cast<float>(UGSTN_CombatWorldSubsystem::ComputeBoundRadius(Resolved.Hurtboxes));

	Definitions.Add(Definition);
	ResolvedTypes.Add(MoveTemp(Resolved));
	return Definitions.Num() - 1;
}

FGSTN_MinionHandle UGSTN_MinionCombatSubsystem::EnqueueSpawn(UGSTN_MinionDefinition* Definition, const FTransform& Transform, const FGSTN_MinionSpawnParams& Params)
{
	if (!bActive || !Definition)
	{
		return FGSTN_MinionHandle();
	}
	const int32 TypeIndex = RegisterDefinition(Definition);
	if (TypeIndex == INDEX_NONE)
	{
		return FGSTN_MinionHandle();
	}
	const FGSTN_MinionHandle Handle = Store.Allocate();
	if (Handle.IsNull())
	{
		UE_LOG(LogGSTNMinion, Warning, TEXT("Minion store full (%d); spawn of %s refused"), Store.Capacity(), *GetNameSafe(Definition));
		return Handle;
	}
	PendingActivation[Handle.Index] = true;
	Store.TypeIndex[Handle.Index] = static_cast<uint8>(TypeIndex);
	Store.Position[Handle.Index] = FVector3f(Transform.GetLocation());

	FQueuedSpawn& Queued = SpawnQueue.AddDefaulted_GetRef();
	Queued.Handle = Handle;
	Queued.TypeIndex = static_cast<uint8>(TypeIndex);
	Queued.Transform = Transform;
	Queued.Params = Params;
	return Handle;
}

bool UGSTN_MinionCombatSubsystem::Despawn(const FGSTN_MinionHandle& Handle)
{
	if (!bActive || !Store.IsValid(Handle))
	{
		return false;
	}
	FreeSlot(Handle.Index);
	return true;
}

bool UGSTN_MinionCombatSubsystem::SetIntent(const FGSTN_MinionHandle& Handle, const FGSTN_MinionIntent& Intent)
{
	if (!Store.IsValid(Handle))
	{
		return false;
	}
	FGSTN_MinionIntent& Stored = Store.Intent[Handle.Index];
	Stored = Intent;
	// Stamped with the last batch frame (0 before the first batch, which restamps): the brain runs outside the fixed tick and cannot know it
	Stored.StampFrame = FMath::Max(LastBatchFrame, 0);
	return true;
}

bool UGSTN_MinionCombatSubsystem::RequestCommand(const FGSTN_MinionHandle& Handle, const FGameplayTag& Command)
{
	if (!Store.IsValid(Handle))
	{
		return false;
	}
	FGSTN_MinionIntent& Stored = Store.Intent[Handle.Index];
	Stored.Command = Command;
	Stored.StampFrame = FMath::Max(LastBatchFrame, 0);
	return true;
}

float UGSTN_MinionCombatSubsystem::GetHealth(const FGSTN_MinionHandle& Handle) const
{
	return Store.IsValid(Handle) ? Store.Health[Handle.Index] : 0.f;
}

AGSTN_MinionActor* UGSTN_MinionCombatSubsystem::GetActorOfSlot(int32 SlotIndex) const
{
	return SlotActors.IsValidIndex(SlotIndex) && Store.IsSlotAlive(SlotIndex) && !PendingActivation[SlotIndex] ? SlotActors[SlotIndex].Get() : nullptr;
}

const UGSTN_MinionDefinition* UGSTN_MinionCombatSubsystem::GetDefinition(int32 TypeIndex) const
{
	return Definitions.IsValidIndex(TypeIndex) ? Definitions[TypeIndex].Get() : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Hit provider

void UGSTN_MinionCombatSubsystem::AppendSnapshotEntities(int32 Frame, TArray<FGSTN_SnapshotEntry>& OutEntries)
{
	SCOPE_CYCLE_COUNTER(STAT_GSTN_MinionSnapshot);
	if (!bActive)
	{
		return;
	}
	// The batch of this frame runs after the models, so the store holds the end of frame F-1 here (spec 5, step 1)
	for (int32 Slot = Store.NextAliveSlot(0); Slot != INDEX_NONE; Slot = Store.NextAliveSlot(Slot + 1))
	{
		// A queued spawn is not in the world yet; a dying minion is intangible (its death timeline plays out)
		if (PendingActivation[Slot] || IsDyingOrDead(Store.ActionState[Slot]))
		{
			continue;
		}
		const int32 TypeIndex = Store.TypeIndex[Slot];
		const UGSTN_MinionDefinition* Definition = GetDefinition(TypeIndex);
		if (!Definition)
		{
			continue;
		}
		const FGSTN_MinionResolvedType& Resolved = ResolvedTypes[TypeIndex];

		FGSTN_SnapshotEntry& Entry = OutEntries.AddDefaulted_GetRef();
		Entry.EntityId = GSTNEntityKinds::MakeMinionEntityId(static_cast<uint16>(Slot));
		Entry.Generation = Store.Generation[Slot];
		Entry.Team = FGenericTeamId(Definition->TeamId);
		const FRotator Rotation(0.f, Store.YawDegrees[Slot], 0.f);
		Entry.Transform = FTransform(Rotation, FVector(Store.Position[Slot]));
		Entry.Facing = Rotation.Vector();
		Entry.Hurtboxes = Resolved.Hurtboxes;
		// Every hurtbox takes hits normally until G5 derives armor and intangibility from the minion's timeline
		Entry.HurtStates.SetNum(Resolved.Hurtboxes ? Resolved.Hurtboxes->Hurtboxes.Num() : 0);
		Entry.BoundRadius = Resolved.BoundRadius;
		Entry.HealthFraction = Store.MaxHealth[Slot] > 0.f ? Store.Health[Slot] / Store.MaxHealth[Slot] : 1.f;
		// G5: Velocity, ActionState/Reaction (hitstop, stagger threshold, weight) for the attacker's hitstop prediction
	}
}

bool UGSTN_MinionCombatSubsystem::PostToEntityInbox(int32 EntityId, uint16 Generation, const FGSTN_InboxEntry& Entry)
{
	if (!bActive || !GSTNEntityKinds::IsMinionEntityId(EntityId))
	{
		return false;
	}
	const int32 Slot = GSTNEntityKinds::MinionIndexOf(EntityId);
	const FGSTN_MinionHandle Handle = Store.HandleOfSlot(Slot);
	// The sender acted on a snapshot of another occupant of the slot (freed and reused since): the hit is not this minion's
	if (Handle.IsNull() || Handle.Generation != Generation || PendingActivation[Slot])
	{
		return false;
	}
	FPendingHit& Pending = PendingHits.AddDefaulted_GetRef();
	Pending.Frame = Entry.Frame;
	Pending.EntityId = EntityId;
	Pending.Generation = Generation;
	Pending.Entry = Entry;
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Batch

void UGSTN_MinionCombatSubsystem::TickBatch(int32 Frame, bool bResimulating)
{
	// The authority never re-simulates; the hook also fires for re-simulated frames on a client manager, which never activates.
	// The guard keeps a misconfigured world from stepping the store twice for one frame
	if (!bActive || bResimulating)
	{
		return;
	}
	if (LastBatchFrame != INDEX_NONE && Frame <= LastBatchFrame)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(GSTN_MinionBatch);
	SCOPE_CYCLE_COUNTER(STAT_GSTN_MinionBatch);
	// Wall clock for the bench report only; nothing below reads it
	const double StartSeconds = FPlatformTime::Seconds();

	if (LastBatchFrame == INDEX_NONE)
	{
		// Intents written before the first batch were stamped 0: give them the first frame so they are not stale at once
		for (int32 Slot = Store.NextAliveSlot(0); Slot != INDEX_NONE; Slot = Store.NextAliveSlot(Slot + 1))
		{
			if (Store.Intent[Slot].StampFrame != INDEX_NONE)
			{
				Store.Intent[Slot].StampFrame = Frame;
			}
		}
	}

	LastBatch = FGSTN_MinionBatchStats();
	LastBatch.Frame = Frame;

	StageSpawnDrain(Frame);
	BuildSchedule(Frame);
	StageLODUpdate(Frame);
	StageInboxDrain(Frame);
	StageTimelineAdvance(Frame);
	StageMovement(Frame);
	StagePublish(Frame);

	LastBatchFrame = Frame;
	FlushEventsAndDespawns();

	int32 Alive = 0;
	for (int32 Slot = Store.NextAliveSlot(0); Slot != INDEX_NONE; Slot = Store.NextAliveSlot(Slot + 1))
	{
		Alive += PendingActivation[Slot] ? 0 : 1;
	}
	LastBatch.Alive = Alive;
	SET_DWORD_STAT(STAT_GSTN_MinionAlive, Alive);
	INC_DWORD_STAT_BY(STAT_GSTN_MinionScheduled, LastBatch.Scheduled);
	INC_DWORD_STAT_BY(STAT_GSTN_MinionHitsDrained, LastBatch.HitsDrained);
	INC_DWORD_STAT_BY(STAT_GSTN_MinionSpawns, LastBatch.SpawnsActivated);
	INC_DWORD_STAT_BY(STAT_GSTN_MinionEvents, LastBatch.TimelineEvents);

	const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
	++BenchTotals.Batches;
	BenchTotals.TotalBatchSeconds += Elapsed;
	BenchTotals.MaxBatchSeconds = FMath::Max(BenchTotals.MaxBatchSeconds, Elapsed);
}

void UGSTN_MinionCombatSubsystem::StageSpawnDrain(int32 Frame)
{
	SCOPE_CYCLE_COUNTER(STAT_GSTN_MinionSpawnDrain);
	const int32 Budget = FMath::Min(FMath::Max(CVarSpawnsPerFrame.GetValueOnGameThread(), 0), SpawnQueue.Num());
	// FIFO: the oldest requests first, the rest wait for the next frames in the same order
	for (int32 QueueIndex = 0; QueueIndex < Budget; ++QueueIndex)
	{
		const FQueuedSpawn& Queued = SpawnQueue[QueueIndex];
		if (!Store.IsValid(Queued.Handle) || !PendingActivation[Queued.Handle.Index])
		{
			continue;
		}
		const int32 Slot = Queued.Handle.Index;
		TraceStage(EStage::SpawnDrain, Slot);
		const UGSTN_MinionDefinition* Definition = Definitions[Queued.TypeIndex];

		Store.TypeIndex[Slot] = Queued.TypeIndex;
		Store.Position[Slot] = FVector3f(Queued.Transform.GetLocation());
		Store.YawDegrees[Slot] = static_cast<float>(FRotator::NormalizeAxis(Queued.Transform.Rotator().Yaw));
		Store.Speed[Slot] = 0.f;
		Store.MaxHealth[Slot] = Definition->Stats.MaxHealth;
		Store.Health[Slot] = Definition->Stats.MaxHealth;
		Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Idle);
		Store.MoveId[Slot] = 0;
		Store.MoveStartFrame[Slot] = Frame;
		Store.HitCounter[Slot] = 0;
		// Spawning forces Combat without dwell (spec 7)
		Store.LODTier[Slot] = static_cast<uint8>(EGSTN_MinionLODTier::Combat);
		Store.FramesInTier[Slot] = 0;
		// An intent written while the spawn was queued is kept

		PendingActivation[Slot] = false;
		LastRunFrame[Slot] = Frame - 1;
		TimelinePrevFrame[Slot] = INDEX_NONE;
		for (int32 Move = 0; Move < GSTNMinion::MaxMoves; ++Move)
		{
			NextMoveFrame[Slot * GSTNMinion::MaxMoves + Move] = 0;
		}
		LastAttackerId[Slot] = INDEX_NONE;

		AcquireActor(Slot, Queued.Transform);
		if (Queued.Params.bPlaySpawnTimeline && ResolvedTypes[Queued.TypeIndex].SpawnTimeline)
		{
			StartTimeline(Slot, GSTNMinion::ReactionMoveIds::Spawn, Frame);
		}
		++LastBatch.SpawnsActivated;
		// G9: publish Event.MinionSpawned on the authority event bus
	}
	SpawnQueue.RemoveAt(0, Budget, EAllowShrinking::No);
}

void UGSTN_MinionCombatSubsystem::BuildSchedule(int32 Frame)
{
	Scheduled.Reset();
	for (int32 Slot = Store.NextAliveSlot(0); Slot != INDEX_NONE; Slot = Store.NextAliveSlot(Slot + 1))
	{
		if (!PendingActivation[Slot] && GSTNMinionSchedule::ShouldRun(Frame, Slot, IntervalOf(Slot)))
		{
			Scheduled.Add(Slot);
		}
	}
	LastBatch.Scheduled = Scheduled.Num();

	// Nearest-player distance input, read once per batch. Server-only input: it is not part of any rollback
	PlayerPositions.Reset();
	if (const UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* Controller = It->Get();
			const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
			if (Pawn)
			{
				const FVector Location = Pawn->GetActorLocation();
				PlayerPositions.Add(FVector2D(Location.X, Location.Y));
			}
		}
	}
}

const FGSTN_MinionLODParams& UGSTN_MinionCombatSubsystem::LODParamsOf(int32 Slot) const
{
	const UGSTN_MinionDefinition* Definition = GetDefinition(Store.TypeIndex[Slot]);
	return Definition ? Definition->ResolveLODParams(DefaultLODParams) : DefaultLODParams;
}

int32 UGSTN_MinionCombatSubsystem::IntervalOf(int32 Slot) const
{
	return LODParamsOf(Slot).GetInterval(static_cast<EGSTN_MinionLODTier>(Store.LODTier[Slot]));
}

void UGSTN_MinionCombatSubsystem::StageLODUpdate(int32 Frame)
{
	SCOPE_CYCLE_COUNTER(STAT_GSTN_MinionLOD);
	for (const int32 Slot : Scheduled)
	{
		TraceStage(EStage::LODUpdate, Slot);
		const int32 DeltaFrames = FMath::Max(1, Frame - LastRunFrame[Slot]);
		const uint8 OldTier = Store.LODTier[Slot];

		if (ForcedLODTier >= 0 && ForcedLODTier < static_cast<int32>(EGSTN_MinionLODTier::Count))
		{
			if (OldTier != ForcedLODTier)
			{
				Store.LODTier[Slot] = static_cast<uint8>(ForcedLODTier);
				Store.FramesInTier[Slot] = 0;
				QueueLODChange(Slot, OldTier, Store.LODTier[Slot]);
			}
			else
			{
				Store.FramesInTier[Slot] += DeltaFrames;
			}
			continue;
		}

		// Distance on the ground plane to the nearest player pawn; none at all reads as infinitely far (Dormant after the dwell)
		const FVector2D Position(Store.Position[Slot].X, Store.Position[Slot].Y);
		double NearestSquared = TNumericLimits<double>::Max();
		for (const FVector2D& Player : PlayerPositions)
		{
			NearestSquared = FMath::Min(NearestSquared, FVector2D::DistSquared(Position, Player));
		}
		const float Distance = PlayerPositions.IsEmpty() ? TNumericLimits<float>::Max() : static_cast<float>(FMath::Sqrt(NearestSquared));

		const uint8 State = Store.ActionState[Slot];
		const bool bForceCombat = !IsLocomotionState(State) || IsMoveId(Store.MoveId[Slot]);

		FGSTN_MinionLODState LODState;
		LODState.Tier = static_cast<EGSTN_MinionLODTier>(OldTier);
		LODState.FramesInTier = Store.FramesInTier[Slot];
		FGSTN_MinionLODMath::Step(LODParamsOf(Slot), LODState, Distance, bForceCombat, DeltaFrames);
		Store.LODTier[Slot] = static_cast<uint8>(LODState.Tier);
		Store.FramesInTier[Slot] = LODState.FramesInTier;
		if (Store.LODTier[Slot] != OldTier)
		{
			QueueLODChange(Slot, OldTier, Store.LODTier[Slot]);
		}
		// G11: MaxActiveCombat demotion of the farthest non-attacking T0 minions
	}
}

void UGSTN_MinionCombatSubsystem::StageInboxDrain(int32 Frame)
{
	SCOPE_CYCLE_COUNTER(STAT_GSTN_MinionInbox);
	// Entries stamped before this frame (the M4 rule: a hit of frame F reacts at F+1). Later stamps wait
	DrainScratch.Reset();
	for (int32 Index = 0; Index < PendingHits.Num();)
	{
		if (PendingHits[Index].Frame < Frame)
		{
			DrainScratch.Add(MoveTemp(PendingHits[Index]));
			PendingHits.RemoveAt(Index, EAllowShrinking::No);
		}
		else
		{
			++Index;
		}
	}
	// Victim slot, then attacker: the order every machine and a replay agree on, whatever order the attackers ticked in.
	// Stable, so the hitboxes of one attacker keep their delivery order. Hits are drained whatever the victim's schedule: a hit
	// wakes a far minion (forced Combat below)
	Algo::StableSort(DrainScratch, [](const FPendingHit& A, const FPendingHit& B)
	{
		const int32 SlotA = GSTNEntityKinds::MinionIndexOf(A.EntityId);
		const int32 SlotB = GSTNEntityKinds::MinionIndexOf(B.EntityId);
		return SlotA != SlotB ? SlotA < SlotB : A.Entry.SenderId < B.Entry.SenderId;
	});

	for (const FPendingHit& Pending : DrainScratch)
	{
		const int32 Slot = GSTNEntityKinds::MinionIndexOf(Pending.EntityId);
		const FGSTN_MinionHandle Handle = Store.HandleOfSlot(Slot);
		if (Handle.IsNull() || Handle.Generation != Pending.Generation || PendingActivation[Slot] || IsDyingOrDead(Store.ActionState[Slot]))
		{
			continue;
		}
		TraceStage(EStage::InboxDrain, Slot);
		++LastBatch.HitsDrained;

		// G5: the rules pipeline (TeamFilter, HurtState, HurtboxReaction, ComputeKnockback, ApplyDamage, Hitstop) on a victim view built
		// from the store, reaction tiers (Hitstop -> Hitstun / PushedBack / Launched), move cancel, HitBy ring. G3 only subtracts the
		// hit's flat damage when the entry is a plain hit; other kinds are counted and dropped
		float Damage = 0.f;
		if (Pending.Entry.Kind == GSTNInboxKinds::Hit)
		{
			if (const FGSTN_HitInboxEntry* Hit = Pending.Entry.Payload.GetPtr<FGSTN_HitInboxEntry>())
			{
				Damage = Hit->Hit.Damage;
			}
		}
		Store.Health[Slot] = FMath::Max(0.f, Store.Health[Slot] - Damage);
		Store.HitCounter[Slot] = static_cast<uint8>(Store.HitCounter[Slot] + 1);
		LastAttackerId[Slot] = Pending.Entry.SenderId;

		// A hit forces Combat without dwell (spec 7)
		const uint8 OldTier = Store.LODTier[Slot];
		if (OldTier != static_cast<uint8>(EGSTN_MinionLODTier::Combat))
		{
			Store.LODTier[Slot] = static_cast<uint8>(EGSTN_MinionLODTier::Combat);
			Store.FramesInTier[Slot] = 0;
			QueueLODChange(Slot, OldTier, Store.LODTier[Slot]);
		}

		FQueuedEvent& HitEvent = QueuedEvents.AddDefaulted_GetRef();
		HitEvent.Kind = FQueuedEvent::EKind::Hit;
		HitEvent.Handle = Handle;
		HitEvent.Damage = Damage;
		HitEvent.OtherId = Pending.Entry.SenderId;

		if (Store.Health[Slot] <= 0.f)
		{
			// Death replaces whatever the minion was playing; the death timeline (or the corpse frames) ends in the despawn
			Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Dying);
			StartTimeline(Slot, GSTNMinion::ReactionMoveIds::Death, Frame);

			FQueuedEvent& DiedEvent = QueuedEvents.AddDefaulted_GetRef();
			DiedEvent.Kind = FQueuedEvent::EKind::Died;
			DiedEvent.Handle = Handle;
			DiedEvent.OtherId = LastAttackerId[Slot];
			// G5: KO event (Combat.Event.KO with Combat.Entity.Minion) for the attacker's credit
		}
	}
}

const UGSTN_CombatTimeline* UGSTN_MinionCombatSubsystem::TimelineOfMoveId(int32 Slot, uint8 MoveId) const
{
	const int32 TypeIndex = Store.TypeIndex[Slot];
	if (!ResolvedTypes.IsValidIndex(TypeIndex))
	{
		return nullptr;
	}
	const FGSTN_MinionResolvedType& Resolved = ResolvedTypes[TypeIndex];
	if (IsMoveId(MoveId))
	{
		return Resolved.MoveTimelines.IsValidIndex(MoveId - 1) ? Resolved.MoveTimelines[MoveId - 1].Get() : nullptr;
	}
	if (MoveId == GSTNMinion::ReactionMoveIds::Spawn)
	{
		return Resolved.SpawnTimeline;
	}
	if (MoveId == GSTNMinion::ReactionMoveIds::Death)
	{
		return Resolved.DeathTimeline;
	}
	return nullptr;
}

void UGSTN_MinionCombatSubsystem::StartTimeline(int32 Slot, uint8 MoveId, int32 Frame)
{
	Store.MoveId[Slot] = MoveId;
	Store.MoveStartFrame[Slot] = Frame;
	TimelinePrevFrame[Slot] = INDEX_NONE;
	if (MoveId == GSTNMinion::ReactionMoveIds::Spawn)
	{
		Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Spawning);
	}
	else if (MoveId == GSTNMinion::ReactionMoveIds::Death)
	{
		Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Dying);
	}
	// A move keeps the locomotion state: MoveId is what says "attacking" (on the wire too); the movement stage holds the minion still
}

void UGSTN_MinionCombatSubsystem::EndTimeline(int32 Slot, int32 Frame)
{
	const uint8 MoveId = Store.MoveId[Slot];
	if (IsMoveId(MoveId))
	{
		const UGSTN_MinionDefinition* Definition = GetDefinition(Store.TypeIndex[Slot]);
		if (Definition && Definition->Moves.IsValidIndex(MoveId - 1))
		{
			NextMoveFrame[Slot * GSTNMinion::MaxMoves + (MoveId - 1)] = Frame + Definition->Moves[MoveId - 1].CooldownFrames;
		}
		Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Idle);
	}
	else if (MoveId == GSTNMinion::ReactionMoveIds::Spawn)
	{
		Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Idle);
	}
	else if (MoveId == GSTNMinion::ReactionMoveIds::Death)
	{
		Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Dead);
		DespawnList.Add(Slot);
	}
	Store.MoveId[Slot] = 0;
	TimelinePrevFrame[Slot] = INDEX_NONE;
}

void UGSTN_MinionCombatSubsystem::StageTimelineAdvance(int32 Frame)
{
	SCOPE_CYCLE_COUNTER(STAT_GSTN_MinionTimeline);
	for (const int32 Slot : Scheduled)
	{
		if (!Store.IsSlotAlive(Slot))
		{
			continue;
		}
		TraceStage(EStage::TimelineAdvance, Slot);

		// Start the requested move: a known command, no timeline playing, a locomotion state, off cooldown, and T0/T1 (attack
		// timelines never start at T2/T3, spec 7)
		FGSTN_MinionIntent& Intent = Store.Intent[Slot];
		if (Store.MoveId[Slot] == 0 && IsLocomotionState(Store.ActionState[Slot])
			&& Store.LODTier[Slot] <= static_cast<uint8>(EGSTN_MinionLODTier::Near)
			&& Intent.HasCommand() && !Intent.IsStale(Frame))
		{
			const UGSTN_MinionDefinition* Definition = GetDefinition(Store.TypeIndex[Slot]);
			const int32 MoveIndex = Definition ? Definition->FindMoveIndex(Intent.Command) : INDEX_NONE;
			if (MoveIndex != INDEX_NONE && MoveIndex < GSTNMinion::MaxMoves
				&& TimelineOfMoveId(Slot, static_cast<uint8>(MoveIndex + 1)) != nullptr
				&& Frame >= NextMoveFrame[Slot * GSTNMinion::MaxMoves + MoveIndex])
			{
				// G5: range check against the target and the attack token (FGSTN_MinionAttackTokens) before the start
				StartTimeline(Slot, static_cast<uint8>(MoveIndex + 1), Frame);
				Intent.Command = FGameplayTag();
			}
		}

		const uint8 MoveId = Store.MoveId[Slot];
		if (MoveId == 0)
		{
			continue;
		}
		const int32 TimelineFrame = Frame - Store.MoveStartFrame[Slot];
		const UGSTN_CombatTimeline* Timeline = TimelineOfMoveId(Slot, MoveId);
		if (!Timeline)
		{
			// Only a death with no Death timeline gets here (moves and Spawn start only with one): the corpse stays a fixed time
			if (MoveId != GSTNMinion::ReactionMoveIds::Death || TimelineFrame >= FMath::Max(0, CVarCorpseFrames.GetValueOnGameThread()))
			{
				EndTimeline(Slot, Frame);
			}
			continue;
		}

		// One evaluation covers every frame since the last run: events in (previous, current] fire exactly once, so a T1/T2 minion
		// stepping 2 or 4 frames at a time fires the same events as a T0 one
		FGSTN_TimelineEvalParams Params;
		Params.Frame = TimelineFrame;
		Params.PausedFrames = 0; // G5: hitstop pauses
		Params.PreviousFrame = TimelinePrevFrame[Slot];
		FGSTN_TimelinePlayer::Evaluate(Timeline->Data, Params, TimelineOutput);

		if (!TimelineOutput.Events.IsEmpty())
		{
			const FGSTN_MinionHandle Handle = Store.HandleOfSlot(Slot);
			for (const FGSTN_TimelineTrack_Event* Event : TimelineOutput.Events)
			{
				++LastBatch.TimelineEvents;
				OnTimelineEvent.Broadcast(Handle, *Event, TimelineOutput.Frame);
			}
		}
		TimelinePrevFrame[Slot] = TimelineOutput.Frame;

		// G5: FGSTN_TimelinePlayer::EvaluateHits and the attack queries against fighters (end-of-F-1 transforms, Fighter kind mask,
		// attacker-side hit groups), delivered to the fighter inbox with Frame = F

		if (TimelineOutput.bFinished)
		{
			EndTimeline(Slot, Frame);
		}
	}
}

void UGSTN_MinionCombatSubsystem::StageMovement(int32 Frame)
{
	SCOPE_CYCLE_COUNTER(STAT_GSTN_MinionMovement);
	// G4: the movement backend (lightweight or Mover) replaces this placeholder: gravity, collision, ground snap, knockback
	// integration, the timeline's movement tracks and minion-minion separation. Until then: a kinematic step on flat ground
	for (const int32 Slot : Scheduled)
	{
		if (!Store.IsSlotAlive(Slot))
		{
			continue;
		}
		TraceStage(EStage::Movement, Slot);

		const bool bDormant = Store.LODTier[Slot] == static_cast<uint8>(EGSTN_MinionLODTier::Dormant);
		if (bDormant || !IsLocomotionState(Store.ActionState[Slot]) || Store.MoveId[Slot] != 0)
		{
			Store.Speed[Slot] = 0.f;
			continue;
		}
		const UGSTN_MinionDefinition* Definition = GetDefinition(Store.TypeIndex[Slot]);
		if (!Definition)
		{
			continue;
		}
		const FGSTN_MinionStats& Stats = Definition->Stats;
		// A slot stepped every N frames integrates N frames at once (dt x N, spec 7)
		const float DeltaSeconds = FixedStepSeconds * static_cast<float>(FMath::Max(1, Frame - LastRunFrame[Slot]));

		const FGSTN_MinionIntent& Intent = Store.Intent[Slot];
		const bool bStale = Intent.IsStale(Frame);
		FVector2D Direction = bStale ? FVector2D::ZeroVector : Intent.MoveDir;
		if (Direction.SizeSquared() > 1.0)
		{
			Direction.Normalize();
		}
		const float MaxSpeed = Stats.RunSpeed > 0.f ? Stats.RunSpeed : TNumericLimits<float>::Max();
		const float WantedSpeed = bStale ? 0.f : FMath::Clamp(Intent.DesiredSpeed, 0.f, MaxSpeed);
		const FVector2D Velocity = Direction * WantedSpeed;
		const float Speed = static_cast<float>(Velocity.Size());

		// Facing: the requested yaw, else the move direction; turned at the type's rate
		float Yaw = Store.YawDegrees[Slot];
		bool bHasTarget = false;
		float TargetYaw = Yaw;
		if (!bStale && Intent.bFaceRequested)
		{
			TargetYaw = Intent.DesiredYaw;
			bHasTarget = true;
		}
		else if (Speed > 1.f)
		{
			TargetYaw = FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(Velocity.Y, Velocity.X)));
			bHasTarget = true;
		}
		if (bHasTarget)
		{
			const float MaxTurn = Stats.TurnRateDegPerSec * DeltaSeconds;
			const float Delta = FMath::FindDeltaAngleDegrees(Yaw, TargetYaw);
			Yaw = static_cast<float>(FRotator::NormalizeAxis(Yaw + FMath::Clamp(Delta, -MaxTurn, MaxTurn)));
		}
		Store.YawDegrees[Slot] = Yaw;

		FVector3f& Position = Store.Position[Slot];
		Position.X += static_cast<float>(Velocity.X) * DeltaSeconds;
		Position.Y += static_cast<float>(Velocity.Y) * DeltaSeconds;
		Store.Speed[Slot] = Speed;
		Store.ActionState[Slot] = static_cast<uint8>(Speed < 1.f ? EGSTN_MinionActionState::Idle
			: (Speed <= Stats.WalkSpeed + 1.f ? EGSTN_MinionActionState::Walk : EGSTN_MinionActionState::Move));
	}
}

void UGSTN_MinionCombatSubsystem::StagePublish(int32 Frame)
{
	SCOPE_CYCLE_COUNTER(STAT_GSTN_MinionPublish);
	for (const int32 Slot : Scheduled)
	{
		if (!Store.IsSlotAlive(Slot))
		{
			continue;
		}
		TraceStage(EStage::Publish, Slot);
		LastRunFrame[Slot] = Frame;

		// T0/T1 only: a far minion's actor keeps a stale transform on purpose (nothing in the simulation reads the actor)
		AGSTN_MinionActor* Actor = SlotActors[Slot];
		if (Actor && Store.LODTier[Slot] <= static_cast<uint8>(EGSTN_MinionLODTier::Near))
		{
			Actor->SetActorLocationAndRotation(FVector(Store.Position[Slot]), FRotator(0.f, Store.YawDegrees[Slot], 0.f), false, nullptr, ETeleportType::TeleportPhysics);
		}
		// G5: history ring write for frame F (pose, swept bound). G6: pack item dirtying at the tier's rate
	}

#if STATS
	int32 PerTier[4] = {};
	for (int32 Slot = Store.NextAliveSlot(0); Slot != INDEX_NONE; Slot = Store.NextAliveSlot(Slot + 1))
	{
		if (!PendingActivation[Slot] && Store.LODTier[Slot] < 4)
		{
			++PerTier[Store.LODTier[Slot]];
		}
	}
	SET_DWORD_STAT(STAT_GSTN_MinionT0, PerTier[0]);
	SET_DWORD_STAT(STAT_GSTN_MinionT1, PerTier[1]);
	SET_DWORD_STAT(STAT_GSTN_MinionT2, PerTier[2]);
	SET_DWORD_STAT(STAT_GSTN_MinionT3, PerTier[3]);
#endif
}

void UGSTN_MinionCombatSubsystem::FlushEventsAndDespawns()
{
	// Broadcast after the stages: a listener may spawn, despawn or write intents without touching a column a stage is walking
	TArray<FQueuedEvent> Events = MoveTemp(QueuedEvents);
	QueuedEvents.Reset();
	for (const FQueuedEvent& Event : Events)
	{
		if (!Store.IsValid(Event.Handle))
		{
			continue;
		}
		AGSTN_MinionActor* Actor = SlotActors[Event.Handle.Index];
		UGSTN_MinionCombatComponent* Component = Actor ? Actor->GetMinionComponent() : nullptr;
		if (!Component)
		{
			continue;
		}
		switch (Event.Kind)
		{
		case FQueuedEvent::EKind::Hit:
			Component->OnMinionHit.Broadcast(Event.Damage, Event.OtherId);
			break;
		case FQueuedEvent::EKind::Died:
			Component->OnMinionDied.Broadcast(Event.OtherId);
			break;
		case FQueuedEvent::EKind::LODChanged:
			Component->OnLODChanged.Broadcast(static_cast<EGSTN_MinionLODTier>(Event.OldTier), static_cast<EGSTN_MinionLODTier>(Event.NewTier));
			break;
		}
	}

	// Death timelines that ended this frame: the slot is freed (generation bumped) and the actor returns to the pool
	TArray<int32> Despawns = MoveTemp(DespawnList);
	DespawnList.Reset();
	for (const int32 Slot : Despawns)
	{
		if (Store.IsSlotAlive(Slot))
		{
			FreeSlot(Slot);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Pool

void UGSTN_MinionCombatSubsystem::AcquireActor(int32 Slot, const FTransform& Transform)
{
	AGSTN_MinionActor* Actor = nullptr;
	while (!Actor && !FreeActors.IsEmpty())
	{
		Actor = FreeActors.Pop(EAllowShrinking::No);
	}
	if (!Actor)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return;
		}
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.ObjectFlags |= RF_Transient;
		Actor = World->SpawnActor<AGSTN_MinionActor>(AGSTN_MinionActor::StaticClass(), Transform, SpawnParams);
		if (!Actor)
		{
			UE_LOG(LogGSTNMinion, Error, TEXT("Minion actor spawn failed for slot %d"), Slot);
			return;
		}
	}
	Actor->OnAcquired(Transform);
	SlotActors[Slot] = Actor;
	if (UGSTN_MinionCombatComponent* Component = Actor->GetMinionComponent())
	{
		Component->BindToSlot(this, Store.HandleOfSlot(Slot));
	}
}

void UGSTN_MinionCombatSubsystem::ReleaseActor(int32 Slot)
{
	if (!SlotActors.IsValidIndex(Slot))
	{
		return;
	}
	if (AGSTN_MinionActor* Actor = SlotActors[Slot])
	{
		Actor->OnReleased();
		FreeActors.Push(Actor);
	}
	SlotActors[Slot] = nullptr;
}

void UGSTN_MinionCombatSubsystem::FreeSlot(int32 Slot)
{
	const FGSTN_MinionHandle Handle = Store.HandleOfSlot(Slot);
	if (Handle.IsNull())
	{
		return;
	}
	if (PendingActivation[Slot])
	{
		SpawnQueue.RemoveAll([&Handle](const FQueuedSpawn& Queued) { return Queued.Handle == Handle; });
		PendingActivation[Slot] = false;
	}
	ReleaseActor(Slot);
	// Pending hits of this occupant are dropped by the generation check of the drain
	Store.Free(Handle);
	TimelinePrevFrame[Slot] = INDEX_NONE;
	LastRunFrame[Slot] = INDEX_NONE;
	++LastBatch.Despawned;
	// G6: release the pack item. G9: publish Event.MinionDespawned
}

void UGSTN_MinionCombatSubsystem::QueueLODChange(int32 Slot, uint8 OldTier, uint8 NewTier)
{
	FQueuedEvent& Event = QueuedEvents.AddDefaulted_GetRef();
	Event.Kind = FQueuedEvent::EKind::LODChanged;
	Event.Handle = Store.HandleOfSlot(Slot);
	Event.OldTier = OldTier;
	Event.NewTier = NewTier;
}

void UGSTN_MinionCombatSubsystem::TraceStage(EStage Stage, int32 Slot)
{
	if (StageTraceForTests)
	{
		StageTraceForTests->Add(FIntPoint(static_cast<int32>(Stage), Slot));
	}
}
