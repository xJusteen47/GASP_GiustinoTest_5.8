// Minion runtime (authority batch, M8 G3; hit flows, attack tokens and lag compensation G5).

#include "Runtime/GSTN_MinionCombatSubsystem.h"

#include "Algo/StableSort.h"
#include "Engine/World.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GSTN_CombatTags.h"
#include "GSTN_MinionTags.h"
#include "GSTNMinionCoreModule.h"
#include "HAL/IConsoleManager.h"
#include "Hits/GSTN_CombatHitTags.h"
#include "Hits/GSTN_HitExtensions.h"
#include "Hits/GSTN_HitRunner.h"
#include "Hits/GSTN_HitShapeUtils.h"
#include "Hits/GSTN_HitSteps.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Input/GSTN_CombatSettings.h"
#include "NetworkPredictionWorldManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Rules/GSTN_CombatRules.h"
#include "GenericTeamAgentInterface.h"
#include "Runtime/GSTN_MinionActor.h"
#include "Runtime/GSTN_MinionCombatComponent.h"
#include "Runtime/GSTN_MinionPromotedComponent.h"
#include "Schedule/GSTN_MinionSchedule.h"
#include "Simulation/GSTN_CombatAttacker.h"
#include "Simulation/GSTN_CombatVictim.h"
#include "Simulation/GSTN_HitInbox.h"
#include "Simulation/GSTN_LagCompensation.h"
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
DECLARE_DWORD_COUNTER_STAT(TEXT("Ignored hit effects"), STAT_GSTN_MinionIgnoredEffects, STATGROUP_GSTNMinion);
DECLARE_DWORD_COUNTER_STAT(TEXT("Hits on fighters"), STAT_GSTN_MinionAttackHits, STATGROUP_GSTNMinion);
DECLARE_DWORD_COUNTER_STAT(TEXT("Attack tokens denied"), STAT_GSTN_MinionTokensDenied, STATGROUP_GSTNMinion);

namespace
{
	TAutoConsoleVariable<int32> CVarLODLog(
		TEXT("gstn.Minion.LOD.Log"), 0,
		TEXT("1 logs every LOD tier change as: MinionLOD id=<entity id> from=<tier> to=<tier> frame=<batch frame>. The harness reads it; off by default."));

	TAutoConsoleVariable<int32> CVarSpawnsPerFrame(
		TEXT("gstn.Minion.SpawnsPerFrame"), 4,
		TEXT("Most queued minion spawns the batch activates per frame (FIFO). Spreads a wave over frames instead of one hitch."));

	TAutoConsoleVariable<int32> CVarCorpseFrames(
		TEXT("gstn.Minion.CorpseFrames"), 60,
		TEXT("Frames a dying minion stays before it is freed when its type has no Death timeline."));

	TAutoConsoleVariable<int32> CVarMaxFightersFromMinions(
		TEXT("gstn.Minion.MaxFightersFromMinions"), 4,
		TEXT("Most fighters made by promoting minions that may be alive at once: each is a full Network Prediction entity (spike 1 numbers). A promotion above the cap is refused."));

	TAutoConsoleVariable<int32> CVarMaxAttackersPerPlayer(
		TEXT("gstn.Minion.MaxAttackersPerPlayer"), FGSTN_MinionAttackTokens::DefaultMaxPerTarget,
		TEXT("Most minions that may start an attack against one fighter at a time (attack tokens). Read when the minion subsystem activates."));

	TAutoConsoleVariable<int32> CVarMaxActiveT0(
		TEXT("gstn.Minion.MaxActiveT0"), 48,
		TEXT("Host budget (M8 4d.1): most minions kept in T0 Combat at once; the farthest non-attackers are demoted to T1 Near. 0 = no cap. Attackers (attack token holders, minions in a timeline or reaction) and minions exempt from the cap are never demoted."));

	TAutoConsoleVariable<float> CVarLagCompMaxSpeed(
		TEXT("gstn.Minion.LagComp.MaxSpeed"), 700.f,
		TEXT("Fastest a minion moves (cm/s): a rewound query inflates its search by this times the rewind age (spec 9.2: 700 x 0.25 s = 175 cm)."));

	/** The transform that carries a hitbox at a timeline frame; false for a space a minion cannot place (a projectile, a missing baked socket) */
	bool BuildCarrier(const FGSTN_HitboxSpec& Spec, const FGSTN_CombatTimelineData& Timeline, const FTransform& Root, int32 TimelineFrame, int32 TrackFrame, FTransform& OutCarrier)
	{
		const FTransform Key = UGSTN_HitShapeUtils::EvaluateKeys(Spec.Keys, TrackFrame);
		switch (Spec.Space)
		{
		case EGSTN_HitboxSpace::Root:
			OutCarrier = Key * Root;
			return true;
		case EGSTN_HitboxSpace::BakedSocket:
		{
			FTransform Socket;
			if (!Timeline.FindBakedSocket(Spec.Socket, TimelineFrame, Socket))
			{
				return false;
			}
			OutCarrier = Key * Socket * Root;
			return true;
		}
		case EGSTN_HitboxSpace::WorldAtMoveStart:
			// A minion keeps no move origin (one more column for a space no minion move uses yet): the root of the frame stands in
			OutCarrier = Key * Root;
			return true;
		case EGSTN_HitboxSpace::Projectile:
		default:
			return false;
		}
	}

	FVector ShapeCenterOf(const FGSTN_HitShape& Shape, const FTransform& Carrier)
	{
		return (Shape.Local * Carrier).GetLocation();
	}

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
	NumPlatformFlagged = 0;
	PublishedPoses.Reset();
	const int32 Capacity = Store.Capacity();
	PendingActivation.Init(false, Capacity);
	LastRunFrame.Init(INDEX_NONE, Capacity);
	TimelinePrevFrame.Init(INDEX_NONE, Capacity);
	NextMoveFrame.Init(0, Capacity * GSTNMinion::MaxMoves);
	MoveColumns.Reset(Capacity);
	SlotActors.Init(nullptr, Capacity);
	CorpseEndFrame.Init(INDEX_NONE, Capacity);
	Corpses.Reset();
	PromotedFighters.Reset();
	Scheduled.Reserve(Capacity);

	KnockbackPercent.Init(0.f, Capacity);
	HitstopEndFrame.Init(INDEX_NONE, Capacity);
	HitstunEndFrame.Init(INDEX_NONE, Capacity);
	PendingReaction.Init(0, Capacity);
	PendingReactionVelocity.Init(FVector3f::ZeroVector, Capacity);
	AttackerHitstopEndFrame.Init(INDEX_NONE, Capacity);
	PausedFrames.Init(0, Capacity);
	LastAttackerRing.Init(INDEX_NONE, Capacity * GSTNMinion::LastAttackerRingSize);
	LastAttackerHead.Init(0, Capacity);
	LastAttackerCount.Init(0, Capacity);
	HitByRing.Init(FHitByKey(), Capacity * GSTNMinion::HitByRingSize);
	HitByHead.Init(0, Capacity);
	AttackTargets.Init(FAttackTarget(), Capacity * GSTNMinion::AttackTargetRingSize);
	AttackTargetHead.Init(0, Capacity);
	History.Reset(Capacity);
	AttackTokens.Reset();
	AttackTokens.SetMaxPerTarget(CVarMaxAttackersPerPlayer.GetValueOnGameThread());

	// Rules resolved here, outside the simulation, so the batch only runs resolved lists. A rules asset set before activation wins
	if (!CombatRules)
	{
		// The settings hand out a const pointer to an asset or to their built-in copy; the batch only reads it, the property keeps it referenced
		CombatRules = const_cast<UGSTN_CombatRules*>(UGSTN_CombatSettings::Get().ResolveDefaultRules());
	}

	UWorld* World = GetWorld();
	if (UGSTN_CombatWorldSubsystem* Combat = UGSTN_CombatWorldSubsystem::Get(World))
	{
		CombatWorld = Combat;
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
	// Packs, the net manager and the per-slot replication columns (G6); after the store is sized
	NetActivate();
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
	NetDeinitialize();
	SpawnQueue.Reset();
	PendingHits.Reset();
	AttackTokens.Reset();
	CombatWorld.Reset();
	QueuedEvents.Reset();
	DespawnList.Reset();
	CorpseEndFrame.Reset();
	Corpses.Reset();
	PromotedFighters.Reset();
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
	// The clients check the type table against theirs (G6 registry hash); the manager carries the new table
	NetOnRegistryChanged();
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

bool UGSTN_MinionCombatSubsystem::IsAlive(const FGSTN_MinionHandle& Handle) const
{
	if (!bActive || !Store.IsValid(Handle))
	{
		return false;
	}
	// A queued spawn is alive (its wave must not count as cleared before it activates)
	return PendingActivation[Handle.Index] || !IsDyingOrDead(Store.ActionState[Handle.Index]);
}

int32 UGSTN_MinionCombatSubsystem::GetNumPromotedFighters()
{
	// A fighter being destroyed no longer counts (Destroy marks it before the object is collected)
	PromotedFighters.RemoveAll([](const TWeakObjectPtr<AActor>& Fighter) { return !Fighter.IsValid() || Fighter->IsActorBeingDestroyed(); });
	return PromotedFighters.Num();
}

AActor* UGSTN_MinionCombatSubsystem::PromoteToFighter(const FGSTN_MinionHandle& Handle, const FGSTN_MinionPromotionParams& Params)
{
	// The stages walk the columns this frees. LastBatchFrame is set after the last stage, so the post-batch events (the flush) are outside
	const bool bInsideStage = CurrentBatchFrame != LastBatchFrame;
	if (!ensureMsgf(!bInsideStage, TEXT("Minion Promote called from inside a batch stage")))
	{
		return nullptr;
	}
	if (!bActive || !Store.IsValid(Handle))
	{
		UE_LOG(LogGSTNMinion, Verbose, TEXT("Promote refused: %s is not a live minion"), *Handle.ToString());
		return nullptr;
	}
	const int32 Slot = Handle.Index;
	const EGSTN_MinionActionState State = static_cast<EGSTN_MinionActionState>(Store.ActionState[Slot]);
	if (PendingActivation[Slot] || State == EGSTN_MinionActionState::Spawning || State == EGSTN_MinionActionState::Dying || State == EGSTN_MinionActionState::Dead)
	{
		UE_LOG(LogGSTNMinion, Verbose, TEXT("Promote refused: %s is queued, spawning, dying or dead"), *Handle.ToString());
		return nullptr;
	}
	const UGSTN_MinionDefinition* Definition = GetDefinition(Store.TypeIndex[Slot]);
	UWorld* World = GetWorld();
	if (!Definition || !World)
	{
		return nullptr;
	}

	UClass* FighterClass = Params.FighterClassOverride.Get();
	if (!FighterClass)
	{
		FighterClass = Definition->PromotionFighterClass.Get();
		if (!FighterClass && !Definition->PromotionFighterClass.IsNull())
		{
			// Perf-debt: a synchronous load of a pawn class inside gameplay; a wave preloads it (GetAssetsToPreload), a lone promotion does not
			FighterClass = Definition->PromotionFighterClass.LoadSynchronous();
		}
	}
	if (!FighterClass)
	{
		UE_LOG(LogGSTNMinion, Verbose, TEXT("Promote refused: %s has no fighter class"), *GetNameSafe(Definition));
		return nullptr;
	}
	const int32 Cap = FMath::Max(CVarMaxFightersFromMinions.GetValueOnGameThread(), 0);
	if (GetNumPromotedFighters() >= Cap)
	{
		UE_LOG(LogGSTNMinion, Verbose, TEXT("Promote refused: %d fighters from minions are alive (gstn.Minion.MaxFightersFromMinions %d)"), PromotedFighters.Num(), Cap);
		return nullptr;
	}

	// The store holds the feet; a pawn's origin is its capsule centre
	const FVector Feet(Store.Position[Slot]);
	const FTransform Transform(FRotator(0.0, Store.YawDegrees[Slot], 0.0), Feet + FVector(0.0, 0.0, Definition->Stats.CapsuleHalfHeight));
	AActor* Fighter = World->SpawnActorDeferred<AActor>(FighterClass, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Fighter)
	{
		UE_LOG(LogGSTNMinion, Warning, TEXT("Promote failed: %s did not spawn"), *GetNameSafe(FighterClass));
		return nullptr;
	}

	UGSTN_MinionPromotedComponent* Promoted = NewObject<UGSTN_MinionPromotedComponent>(Fighter, TEXT("MinionPromoted"));
	Promoted->PromotedFromMinionId = SlotItemId.IsValidIndex(Slot) ? SlotItemId[Slot] : INDEX_NONE;
	Promoted->PromotedFromPackId = SlotPackId.IsValidIndex(Slot) ? SlotPackId[Slot] : INDEX_NONE;
	Promoted->SourceTypeTag = Definition->TypeTag;
	Promoted->HealthFraction = Params.bKeepHealthFraction && Store.MaxHealth[Slot] > 0.f ? FMath::Clamp(Store.Health[Slot] / Store.MaxHealth[Slot], 0.f, 1.f) : 1.f;
	Promoted->Team = Definition->TeamId;
	Fighter->AddInstanceComponent(Promoted);
	Promoted->RegisterComponent();

	if (IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(Fighter))
	{
		TeamAgent->SetGenericTeamId(FGenericTeamId(Definition->TeamId));
	}
	Fighter->FinishSpawning(Transform);

	PromotedFighters.Add(Fighter);
	UE_LOG(LogGSTNMinion, Verbose, TEXT("Minion %s promoted to %s (health fraction %.2f, tag %s)"), *Handle.ToString(), *GetNameSafe(Fighter), Promoted->HealthFraction,
		*GSTNMinionTags::Event_MinionPromoted.GetTag().ToString());
	// G9b: publish Event.MinionPromoted on the authority event bus (the spawn and despawn events are not published yet either)
	OnMinionPromoted.Broadcast(Handle, Fighter);

	// Same path as Despawn: the actor returns to the pool, the item leaves its pack, the generation is bumped. The brain drops with the slot
	FreeSlot(Slot);
	return Fighter;
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

bool UGSTN_MinionCombatSubsystem::LaunchMinion(const FGSTN_MinionHandle& Handle, const FVector& Velocity, bool bPushBack)
{
	if (!Store.IsValid(Handle) || PendingActivation[Handle.Index])
	{
		return false;
	}
	const int32 Slot = Handle.Index;
	MoveColumns.PendingLaunch[Slot] = FVector3f(Velocity);
	// A launch wakes a far or dormant minion at once (G8): it is about to fly, and the next scheduled LOD step of a dormant one is
	// up to DormantInterval frames away. Same effect as a hit in the inbox drain: Combat now, no hysteresis, no dwell
	if (Store.LODTier[Slot] != static_cast<uint8>(EGSTN_MinionLODTier::Combat))
	{
		const uint8 OldTier = Store.LODTier[Slot];
		Store.LODTier[Slot] = static_cast<uint8>(EGSTN_MinionLODTier::Combat);
		Store.FramesInTier[Slot] = 0;
		QueueLODChange(Slot, OldTier, Store.LODTier[Slot]);
	}
	MoveColumns.KnockbackMode[Slot] = static_cast<uint8>(bPushBack ? EGSTN_MinionKnockbackMode::PushedBack : EGSTN_MinionKnockbackMode::Launched);
	if (!IsDyingOrDead(Store.ActionState[Slot]))
	{
		Store.ActionState[Slot] = static_cast<uint8>(bPushBack ? EGSTN_MinionActionState::PushedBack : EGSTN_MinionActionState::Launched);
	}
	// A script launch leaves a running timeline alone; the drain cancels the move of a staggered minion itself (CancelTimeline)
	return true;
}

int32 UGSTN_MinionCombatSubsystem::GetLastAttacker(int32 Slot, int32 Age) const
{
	constexpr int32 Size = GSTNMinion::LastAttackerRingSize;
	if (!LastAttackerCount.IsValidIndex(Slot) || Age < 0 || Age >= LastAttackerCount[Slot])
	{
		return INDEX_NONE;
	}
	// Head is the next write position, so the newest entry sits one before it
	const int32 Position = (LastAttackerHead[Slot] - 1 - Age + 2 * Size) % Size;
	return LastAttackerRing[Slot * Size + Position];
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
		FGSTN_SnapshotEntry& Entry = OutEntries.AddDefaulted_GetRef();
		if (!FillSnapshotEntry(Slot, Frame, Entry))
		{
			OutEntries.Pop(EAllowShrinking::No);
		}
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
	if (const FGSTN_HitInboxEntry* Hit = FGSTN_HitInboxEntry::FromInboxEntry(Entry))
	{
		Pending.AttackerHitboxIndex = Hit->AttackerHitboxIndex;
		Pending.HurtboxIndex = Hit->HurtboxIndex;
	}
	Pending.Entry = Entry;
	return true;
}

UGSTN_CombatWorldSubsystem* UGSTN_MinionCombatSubsystem::GetCombatWorld() const
{
	return CombatWorld.Get();
}

void UGSTN_MinionCombatSubsystem::SetCombatRules(UGSTN_CombatRules* Rules)
{
	CombatRules = Rules;
}

const UGSTN_CombatRules* UGSTN_MinionCombatSubsystem::GetCombatRules() const
{
	return CombatRules;
}

FVector UGSTN_MinionCombatSubsystem::UpAt(const FVector& Position) const
{
	const UWorld* World = GetWorld();
	const FVector Gravity = GravitySource ? GravitySource(Position) : FVector(0.0, 0.0, World ? World->GetGravityZ() : -980.0);
	const FVector Up = -Gravity.GetSafeNormal();
	return Up.IsNearlyZero() ? FVector::UpVector : Up;
}

FTransform UGSTN_MinionCombatSubsystem::MakeRoot(const FVector& Feet, float YawDegrees) const
{
	// The capsule stands along the gravity up (the movement backend aligns it so); the yaw turns about that up
	const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, UpAt(Feet));
	const FQuat Yaw(FVector::UpVector, FMath::DegreesToRadians(static_cast<double>(YawDegrees)));
	return FTransform(Tilt * Yaw, Feet);
}

FTransform UGSTN_MinionCombatSubsystem::RootOf(int32 Slot) const
{
	return MakeRoot(FVector(Store.Position[Slot]), Store.YawDegrees[Slot]);
}

bool UGSTN_MinionCombatSubsystem::FillSnapshotEntry(int32 Slot, int32 Frame, FGSTN_SnapshotEntry& Out) const
{
	const int32 TypeIndex = Store.TypeIndex[Slot];
	const UGSTN_MinionDefinition* Definition = GetDefinition(TypeIndex);
	if (!Definition)
	{
		return false;
	}
	const FGSTN_MinionResolvedType& Resolved = ResolvedTypes[TypeIndex];
	const UGSTN_HurtboxSetDefinition* Set = Resolved.Hurtboxes;

	Out.EntityId = GSTNEntityKinds::MakeMinionEntityId(static_cast<uint16>(Slot));
	Out.Generation = Store.Generation[Slot];
	Out.Team = FGenericTeamId(Definition->TeamId);
	Out.Transform = RootOf(Slot);
	Out.Velocity = FVector(MoveColumns.Velocity[Slot]);
	Out.Facing = Out.Transform.GetUnitAxis(EAxis::X);
	Out.Up = Out.Transform.GetUnitAxis(EAxis::Z);
	Out.Hurtboxes = Set;
	Out.BoundRadius = Resolved.BoundRadius;
	Out.HealthFraction = Store.MaxHealth[Slot] > 0.f ? FMath::Clamp(Store.Health[Slot] / Store.MaxHealth[Slot], 0.f, 1.f) : 1.f;

	// Both hitstops freeze the minion: the victim's window (hits of the previous frames) and its own as an attacker
	const bool bVictimHitstop = Frame < HitstopEndFrame[Slot];
	const bool bAttackerHitstop = AttackerHitstopEndFrame[Slot] != INDEX_NONE && Frame <= AttackerHitstopEndFrame[Slot];
	Out.bInHitstop = bVictimHitstop || bAttackerHitstop;

	// The fighters' action state vocabulary, for steps that read it: any hit reaction is Hitstun, a running move InTimeline
	switch (static_cast<EGSTN_MinionActionState>(Store.ActionState[Slot]))
	{
	case EGSTN_MinionActionState::Hitstop:
	case EGSTN_MinionActionState::Hitstun:
	case EGSTN_MinionActionState::PushedBack:
	case EGSTN_MinionActionState::Launched:
	case EGSTN_MinionActionState::Knockdown:
	case EGSTN_MinionActionState::GetUp:
		Out.ActionState = EGSTN_ActionState::Hitstun;
		break;
	case EGSTN_MinionActionState::Dying:
	case EGSTN_MinionActionState::Dead:
		Out.ActionState = EGSTN_ActionState::KO;
		break;
	default:
		Out.ActionState = IsMoveId(Store.MoveId[Slot]) ? EGSTN_ActionState::InTimeline : EGSTN_ActionState::Actionable;
		break;
	}

	// How each hurtbox takes hits: the hurtbox modifier tracks of the running timeline at the frame it last evaluated (the end of the
	// previous frame, like the fighter's), so a dodge's intangible feet or an armored windup work on minions with the same data
	Out.HurtStates.Reset();
	if (Set)
	{
		Out.HurtStates.SetNum(Set->Hurtboxes.Num());
		const UGSTN_CombatTimeline* Timeline = Store.MoveId[Slot] != 0 && TimelinePrevFrame[Slot] != INDEX_NONE ? TimelineOfMoveId(Slot, Store.MoveId[Slot]) : nullptr;
		if (Timeline)
		{
			FGSTN_TimelinePlayer::EvaluateHits(Timeline->Data, TimelinePrevFrame[Slot], HurtScratch);
			for (int32 Index = 0; Index < Set->Hurtboxes.Num(); ++Index)
			{
				Out.HurtStates[Index] = FGSTN_TimelinePlayer::ResolveHurtState(HurtScratch, Set->Hurtboxes[Index]);
			}
		}
	}

	// The compact reaction state, built as the fighter's is: what the attacker's hitstop prediction and the lag-comp ring read
	uint8 Flags = 0;
	FGSTN_StaggerThreshold Threshold = Set ? Set->StaggerThreshold : FGSTN_StaggerThreshold();
	bool bAllIntangible = Out.HurtStates.Num() > 0;
	bool bAllInvincible = Out.HurtStates.Num() > 0;
	for (const FGSTN_HurtboxFrameState& Hurt : Out.HurtStates)
	{
		bAllIntangible &= Hurt.State == EGSTN_HurtState::Intangible;
		bAllInvincible &= Hurt.State == EGSTN_HurtState::Intangible || Hurt.State == EGSTN_HurtState::Invincible;
		if (Hurt.State == EGSTN_HurtState::Armor)
		{
			Flags |= GSTNVictimReactionFlags::Armor;
			Threshold = FGSTN_StaggerThreshold::Max(Threshold, Hurt.ArmorThreshold);
		}
	}
	Flags |= bAllIntangible ? GSTNVictimReactionFlags::Intangible : 0;
	Flags |= bAllInvincible ? GSTNVictimReactionFlags::Invincible : 0;
	Flags |= Out.bInHitstop ? GSTNVictimReactionFlags::InHitstop : 0;
	const double WeightScale = FGSTN_CombatVictim::ComputeWeightScale(CombatRules, Set, KnockbackPercent[Slot], Out.HealthFraction);
	Out.Reaction = FGSTN_VictimReactionState::Make(KnockbackPercent[Slot], WeightScale, Flags, Threshold);
	// No snapshot contributor runs for minions (no fragments, no parts code here): the parts bridge (M4b P16) uses the step slot, the victim view seam and the outcome seam
	return true;
}

FGSTN_HitVictimView UGSTN_MinionCombatSubsystem::MakeMinionView(int32 Slot, const FGSTN_SnapshotEntry& Entry) const
{
	FGSTN_HitVictimView View = FGSTN_CombatVictim::MakeVictimView(Entry, CombatRules);
	// The health model of a minion: its own hit points in every damage mode (it dies at 0 whatever the rules count), so the fraction is
	// always the store's; the percent modes still grow the knockback through the damage percent of the reaction state
	View.HealthFraction = Store.MaxHealth[Slot] > 0.f ? FMath::Clamp(Store.Health[Slot] / Store.MaxHealth[Slot], 0.f, 1.f) : 1.f;
	// The store as it is now: the earlier entries of the same drain already moved health and percent (the AdvanceVictim rule)
	const FGSTN_VictimReactionState& Old = Entry.Reaction;
	const double WeightScale = FGSTN_CombatVictim::ComputeWeightScale(CombatRules, Entry.Hurtboxes, KnockbackPercent[Slot], View.HealthFraction);
	View.ReactionState = FGSTN_VictimReactionState::Make(KnockbackPercent[Slot], WeightScale, Old.Flags, Old.EffectiveThresholdQ, Old.ExtMask);
	if (const UGSTN_MinionDefinition* Definition = GetDefinition(Store.TypeIndex[Slot]))
	{
		View.Weight = Definition->Stats.Weight;
	}
	// The kind, so a step can tell a minion from a fighter without an id test
	View.StateTags.AddTag(GSTNMinionTags::Entity_Minion);
	const FGameplayTag StateTag = GSTNMinionTags::GetActionStateTag(static_cast<EGSTN_MinionActionState>(Store.ActionState[Slot]));
	if (StateTag.IsValid())
	{
		View.StateTags.AddTag(StateTag);
	}
	// Bridge seam (G10b): a bridge's snapshot extension (the parts row of GSTNPartsMinion); inert while unbound
	VictimViewExtension.ExecuteIfBound(Slot, View);
	return View;
}

bool UGSTN_MinionCombatSubsystem::BuildVictimView(const FGSTN_MinionHandle& Handle, int32 Frame, FGSTN_HitVictimView& OutView) const
{
	if (!Store.IsValid(Handle) || PendingActivation[Handle.Index])
	{
		return false;
	}
	FGSTN_SnapshotEntry Entry;
	if (!FillSnapshotEntry(Handle.Index, Frame, Entry))
	{
		return false;
	}
	OutView = MakeMinionView(Handle.Index, Entry);
	return true;
}

void UGSTN_MinionCombatSubsystem::RunVictimPipeline(FGSTN_HitResolutionContext& Context) const
{
	const UGSTN_CombatRules* Rules = CombatRules;
	if (!Rules)
	{
		return;
	}
	FGSTN_HitContextView View;
	FGSTN_HitPipelineRunner::MakeView(Rules, Context, View);

	// Same loop as FGSTN_HitPipelineRunner::Run, with the two minion differences: MeterGain is skipped (a minion has no meter; the
	// fighter's own prediction gains it as attacker) and the optional extra step runs before ComputeKnockback
	auto RunStep = [&View, &Context, Rules](const UGSTN_HitResolutionStep* Step) -> bool
	{
		if (!Step)
		{
			return false;
		}
		FGSTN_StepResult Result;
		const EGSTN_StepResult Verdict = Step->Resolve(View, Result);
		FGSTN_HitPipelineRunner::Merge(Result, Context);
		if (Verdict == EGSTN_StepResult::Stop)
		{
			return true;
		}
		FGSTN_HitPipelineRunner::MakeView(Rules, Context, View);
		return false;
	};

	const UGSTN_HitResolutionStep* Extra = ExtraVictimStep ? GetDefault<UGSTN_HitResolutionStep>(ExtraVictimStep) : nullptr;
	bool bExtraDone = Extra == nullptr;
	for (const TSubclassOf<UGSTN_HitResolutionStep>& StepClass : Rules->GetResolvedHitPipeline())
	{
		if (!StepClass || StepClass->IsChildOf(UGSTN_HitStep_MeterGain::StaticClass()))
		{
			continue;
		}
		if (!bExtraDone && StepClass->IsChildOf(UGSTN_HitStep_ComputeKnockback::StaticClass()))
		{
			bExtraDone = true;
			if (RunStep(Extra))
			{
				return;
			}
		}
		if (RunStep(GetDefault<UGSTN_HitResolutionStep>(StepClass)))
		{
			return;
		}
	}
	if (!bExtraDone)
	{
		RunStep(Extra);
	}
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
	CurrentBatchFrame = Frame;

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
	INC_DWORD_STAT_BY(STAT_GSTN_MinionIgnoredEffects, LastBatch.IgnoredEffects);
	INC_DWORD_STAT_BY(STAT_GSTN_MinionAttackHits, LastBatch.AttackHits);
	INC_DWORD_STAT_BY(STAT_GSTN_MinionTokensDenied, LastBatch.TokensDenied);
	BenchTotals.IgnoredEffects += LastBatch.IgnoredEffects;
	BenchTotals.AttacksStarted += LastBatch.AttacksStarted;
	BenchTotals.TokensDenied += LastBatch.TokensDenied;
	BenchTotals.AttackHits += LastBatch.AttackHits;

	const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
	++BenchTotals.Batches;
	BenchTotals.TotalBatchSeconds += Elapsed;
	BenchTotals.MaxBatchSeconds = FMath::Max(BenchTotals.MaxBatchSeconds, Elapsed);

	// Replication after the stages and the despawns (G6): outside the batch timing so the G3-G5 bench numbers stay comparable
	NetPublish(Frame);
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
		// The pack is chosen by the publish after the batch (G6), by this group
		SlotSpawnGroup[Slot] = Queued.Params.SpawnGroup;
		LastRunFrame[Slot] = Frame - 1;
		TimelinePrevFrame[Slot] = INDEX_NONE;
		for (int32 Move = 0; Move < GSTNMinion::MaxMoves; ++Move)
		{
			NextMoveFrame[Slot * GSTNMinion::MaxMoves + Move] = 0;
		}
		MoveColumns.ResetSlot(Slot);
		ResetHitColumns(Slot);

		AcquireActor(Slot, Queued.Transform);
		OnMinionActivated.Broadcast(Queued.Handle);
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
	if (bUseTestPlayerPositions)
	{
		PlayerPositions.Append(TestPlayerPositions);
	}
	else if (const UWorld* World = GetWorld())
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
	// Host T0 cap (G11). T0 runs every frame (CombatInterval 1 by default), so every Combat slot is in Scheduled and the count below is the
	// whole T0 population; a slot just woken into Combat by the inbox from a farther tier is counted from its next step on (its
	// forced-Combat state makes it an attacker, which the cap never demotes anyway). A definition with a CombatInterval above 1 would be
	// counted only on its frames, so the cap is exact for the default interval and a lower bound otherwise
	const int32 Cap = FMath::Max(CVarMaxActiveT0.GetValueOnGameThread(), 0);
	int32 CombatCount = 0;
	CapScratch.Reset();
	for (const int32 Slot : Scheduled)
	{
		TraceStage(EStage::LODUpdate, Slot);
		const int32 DeltaFrames = FMath::Max(1, Frame - LastRunFrame[Slot]);
		const uint8 OldTier = Store.LODTier[Slot];

		if (ForcedLODTier >= 0 && ForcedLODTier < static_cast<int32>(EGSTN_MinionLODTier::Count))
		{
			// A bridge ceiling still holds under the test override
			const uint8 Forced = static_cast<uint8>(FMath::Min<int32>(ForcedLODTier, Store.MaxLODTier[Slot]));
			if (OldTier != Forced)
			{
				Store.LODTier[Slot] = Forced;
				Store.FramesInTier[Slot] = 0;
				QueueLODChange(Slot, OldTier, Store.LODTier[Slot]);
			}
			else
			{
				Store.FramesInTier[Slot] += DeltaFrames;
			}
			// The test override is not a distance rule: no demotion either
			CombatCount += Store.LODTier[Slot] == static_cast<uint8>(EGSTN_MinionLODTier::Combat) ? 1 : 0;
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
		const int32 OldFramesInTier = LODState.FramesInTier;
		FGSTN_MinionLODMath::Step(LODParamsOf(Slot), LODState, Distance, bForceCombat, DeltaFrames);
		Store.LODTier[Slot] = static_cast<uint8>(LODState.Tier);
		Store.FramesInTier[Slot] = LODState.FramesInTier;
		// Per-slot ceiling (G10 seam): never farther than a bridge allows. Held at the ceiling, the dwell keeps counting, so releasing the
		// ceiling does not restart the dwell; pulled back from a farther tier, it restarts like any tier change
		const uint8 Ceiling = Store.MaxLODTier[Slot];
		if (Store.LODTier[Slot] > Ceiling)
		{
			Store.LODTier[Slot] = Ceiling;
			Store.FramesInTier[Slot] = Ceiling == OldTier ? OldFramesInTier + DeltaFrames : 0;
		}
		if (Store.LODTier[Slot] == static_cast<uint8>(EGSTN_MinionLODTier::Combat))
		{
			++CombatCount;
			// A candidate for the cap: not an attacker by state (the tests below), not exempt (G10 seam: a platform a player stands on), and
			// free to sit in Near (a bridge ceiling at Combat keeps it in T0). Its tier change is held back until the cap pass has decided,
			// so a slot pulled back to Combat and demoted again in the same frame (Near -> Combat -> Near) emits no event at all
			if (Cap > 0 && !bForceCombat && !IsExemptFromCombatCap(Slot) && Store.MaxLODTier[Slot] >= static_cast<uint8>(EGSTN_MinionLODTier::Near))
			{
				FGSTN_MinionCapCandidate& Candidate = CapScratch.AddDefaulted_GetRef();
				Candidate.Slot = Slot;
				Candidate.Distance = Distance;
				Candidate.OldTier = OldTier;
				continue;
			}
		}
		if (Store.LODTier[Slot] != OldTier)
		{
			QueueLODChange(Slot, OldTier, Store.LODTier[Slot]);
		}
	}
	ApplyT0Cap(Cap, CombatCount);
}

void UGSTN_MinionCombatSubsystem::ApplyT0Cap(int32 Cap, int32 CombatCount)
{
	constexpr uint8 Combat = static_cast<uint8>(EGSTN_MinionLODTier::Combat);
	constexpr uint8 Near = static_cast<uint8>(EGSTN_MinionLODTier::Near);
	ActiveT0Count = CombatCount;
	if (CapScratch.IsEmpty())
	{
		return;
	}

	int32 Over = CombatCount - Cap;
	if (Over > 0)
	{
		// Only now, over the cap: a token holder is an attacker even while it still walks to its target, so it leaves the candidates
		for (int32 Index = CapScratch.Num() - 1; Index >= 0; --Index)
		{
			const FGSTN_MinionCapCandidate& Candidate = CapScratch[Index];
			if (AttackTokens.HoldsAny(Candidate.Slot))
			{
				if (Candidate.OldTier != Combat)
				{
					QueueLODChange(Candidate.Slot, Candidate.OldTier, Combat);
				}
				CapScratch.RemoveAtSwap(Index, 1, EAllowShrinking::No);
			}
		}
	}

	const int32 Demote = Over > 0 ? GSTNMinionLODCap::SelectFarthest(CapScratch, Over) : 0;
	for (int32 Index = 0; Index < CapScratch.Num(); ++Index)
	{
		const FGSTN_MinionCapCandidate& Candidate = CapScratch[Index];
		const uint8 Final = Index < Demote ? Near : Combat;
		if (Final == Near)
		{
			// The dwell restarts: the stepper keeps it in Near for MinDwellFrames, then pulls it back and this pass sends it back again
			// with no event (OldTier is Near), so a minion over the cap does not churn
			Store.LODTier[Candidate.Slot] = Near;
			Store.FramesInTier[Candidate.Slot] = 0;
		}
		if (Final != Candidate.OldTier)
		{
			QueueLODChange(Candidate.Slot, Candidate.OldTier, Final);
		}
	}
	// Cumulative: a demoted slot then dwells in Near and is not a candidate again, so a per-pass count would read 0 a frame later
	T0DemotedCount += Demote;
	ActiveT0Count = CombatCount - Demote;
}

void UGSTN_MinionCombatSubsystem::StageInboxDrain(int32 Frame)
{
	SCOPE_CYCLE_COUNTER(STAT_GSTN_MinionInbox);
	// Wall clock for the spike 5 drain cost only; nothing below reads it
	const double StartSeconds = FPlatformTime::Seconds();
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
	// Victim slot, then attacker, then the attacker's hitbox: the order every machine and a replay agree on, whatever order the attackers
	// ticked in (the M4 drain rule, so trades are order-independent). Stable, so equal keys keep their delivery order. Hits are drained
	// whatever the victim's schedule: a hit wakes a far minion (forced Combat below)
	Algo::StableSort(DrainScratch, [](const FPendingHit& A, const FPendingHit& B)
	{
		const int32 SlotA = GSTNEntityKinds::MinionIndexOf(A.EntityId);
		const int32 SlotB = GSTNEntityKinds::MinionIndexOf(B.EntityId);
		if (SlotA != SlotB)
		{
			return SlotA < SlotB;
		}
		return A.Entry.SenderId != B.Entry.SenderId ? A.Entry.SenderId < B.Entry.SenderId : A.AttackerHitboxIndex < B.AttackerHitboxIndex;
	});

	// The victim's entry is built once per victim, from the store as the previous frame left it (the hits of this drain move only health
	// and percent, which MakeMinionView reads from the store for each entry)
	int32 EntrySlot = INDEX_NONE;
	FGSTN_SnapshotEntry SelfEntry;

	for (const FPendingHit& Pending : DrainScratch)
	{
		const int32 Slot = GSTNEntityKinds::MinionIndexOf(Pending.EntityId);
		const FGSTN_MinionHandle Handle = Store.HandleOfSlot(Slot);
		// Generation check: a hit stamped for the previous occupant of a recycled slot, or for a minion that died earlier in this drain
		if (Handle.IsNull() || Handle.Generation != Pending.Generation || PendingActivation[Slot] || IsDyingOrDead(Store.ActionState[Slot]))
		{
			continue;
		}
		TraceStage(EStage::InboxDrain, Slot);
		++LastBatch.HitsDrained;

		// A hit forces Combat without dwell (spec 7)
		const uint8 OldTier = Store.LODTier[Slot];
		if (OldTier != static_cast<uint8>(EGSTN_MinionLODTier::Combat))
		{
			Store.LODTier[Slot] = static_cast<uint8>(EGSTN_MinionLODTier::Combat);
			Store.FramesInTier[Slot] = 0;
			QueueLODChange(Slot, OldTier, Store.LODTier[Slot]);
		}

		// Other inbox kinds have no minion meaning yet (a link request, a parry punish); a ricochet only informs the victim for its cue (G7)
		const FGSTN_HitInboxEntry* Hit = FGSTN_HitInboxEntry::FromInboxEntry(Pending.Entry);
		if (!Hit || Hit->bRicochet)
		{
			continue;
		}

		// HitBy dedupe (spec 9.2 item 4): a re-delivered hit is dropped before it touches anything
		FHitByKey Key;
		Key.AttackerId = Hit->AttackerId;
		Key.TimelineKey = Hit->Frame;
		Key.HitGroup = Hit->AttackerHitboxIndex;
		if (HasHitBy(Slot, Key))
		{
			++LastBatch.HitsDeduped;
			continue;
		}

		if (EntrySlot != Slot)
		{
			EntrySlot = Slot;
			SelfEntry = FGSTN_SnapshotEntry();
			FillSnapshotEntry(Slot, Frame, SelfEntry);
		}
		AddHitBy(Slot, Key);
		ApplyHit(Slot, Frame, *Hit, SelfEntry);
	}

	BenchTotals.DrainSeconds += FPlatformTime::Seconds() - StartSeconds;
	BenchTotals.DrainedHits += LastBatch.HitsDrained;
}

void UGSTN_MinionCombatSubsystem::ApplyHit(int32 Slot, int32 Frame, const FGSTN_HitInboxEntry& Hit, const FGSTN_SnapshotEntry& Self)
{
	const FGSTN_MinionHandle Handle = Store.HandleOfSlot(Slot);
	const UGSTN_CombatRules* Rules = CombatRules;

	// The context the fighter's victim step builds (FGSTN_CombatVictim::Resolve), on the minion's view
	FGSTN_HitResolutionContext Context;
	FGSTN_ResolvedHit& Incoming = Context.Incoming;
	Incoming.Hit = Hit.Hit;
	Incoming.Frame = Hit.Frame;
	Incoming.AttackerEntityId = Hit.AttackerId;
	Incoming.AttackerTeamId = Hit.AttackerTeam;
	const UGSTN_CombatWorldSubsystem* World = GetCombatWorld();
	FGSTN_CombatVictim::SetAttackerExtensions(Incoming, World ? World->GetSnapshot().Find(Hit.AttackerId) : nullptr);
	// The attacker predicted a Self hit as an ordinary enemy hit (TeamFilter refuses Self): the victim runs the same pipeline on the same input
	const FGameplayTag Relationship = GSTNHitRelationship::FromIndex(Hit.Relationship);
	Incoming.Relationship = Relationship == GSTNCombatHitTags::Relationship_Self.GetTag() ? GSTNCombatHitTags::Relationship_Enemy.GetTag() : Relationship;
	Incoming.bRelationshipForced = GSTNHitRelationship::IsForced(Hit.Relationship);
	Incoming.Up = Self.Up;
	Incoming.AttackerFacing = FRotator(0.0, Hit.AttackerYaw, 0.0).Vector();
	Incoming.HitboxVelocity = Hit.HitboxVelocity;
	Incoming.bVictimGrounded = MoveColumns.bGrounded[Slot] != 0;
	FVector VictimCenter = Self.Transform.GetLocation();
	if (const FGSTN_HitShape* HurtShape = Self.FindHurtboxShape(Hit.HurtboxIndex))
	{
		VictimCenter = ShapeCenterOf(*HurtShape, Self.Transform);
	}
	Incoming.HitboxToVictim = (VictimCenter - Hit.HitLocation).GetSafeNormal(UE_SMALL_NUMBER, Incoming.AttackerFacing);

	Context.Victim = MakeMinionView(Slot, Self);
	Context.HurtboxIndex = Hit.HurtboxIndex;
	Context.HurtState = static_cast<EGSTN_HurtState>(FMath::Min<uint8>(Hit.HurtState, static_cast<uint8>(EGSTN_HurtState::Armor)));
	if (Self.HurtStates.IsValidIndex(Hit.HurtboxIndex))
	{
		Context.ArmorThreshold = Self.HurtStates[Hit.HurtboxIndex].ArmorThreshold;
	}
	Context.HurtboxSet = Self.Hurtboxes;
	if (Rules)
	{
		RunVictimPipeline(Context);
	}
	else
	{
		// No rules at all (a world with no settings asset and no built-in copy): the hit's damage, nothing else
		Context.Outcome.Damage = Hit.Hit.Damage;
	}
	if (!Context.Outcome.bHit)
	{
		// A step refused it (team filter, intangible): it never happened for this minion
		return;
	}
	++LastBatch.HitsApplied;
	const FGSTN_HitOutcome& Outcome = Context.Outcome;

	// Extra effects (the hit's ExtraEffects through ApplyDamage) need an ability system a minion does not have: counted, ignored (v1)
	LastBatch.IgnoredEffects += Outcome.Effects.Num();

	// Damage: the minion's hit points in every mode; the percent modes also keep the damage percent the knockback grows with
	const float Damage = FMath::Max(Outcome.Damage, 0.f);
	Store.Health[Slot] = FMath::Max(0.f, Store.Health[Slot] - Damage);
	if (Rules && Rules->DamageMode != EGSTN_DamageMode::Health)
	{
		KnockbackPercent[Slot] += Damage;
		if (Rules->DamageMode == EGSTN_DamageMode::PercentCapped)
		{
			KnockbackPercent[Slot] = FMath::Min(KnockbackPercent[Slot], Rules->MaxHealth);
		}
	}
	Store.HitCounter[Slot] = static_cast<uint8>(Store.HitCounter[Slot] + 1);
	PushLastAttacker(Slot, Hit.AttackerId);

	// The reaction tier of M4 3. A hit below the stagger threshold changes no state (spec 5 stage 2.2): damage and HitCounter only
	const FGSTN_KnockbackResult& Knockback = Outcome.Knockback;
	const bool bStaggered = Outcome.bHasKnockback && Knockback.bStaggered;
	EGSTN_MinionPendingReaction Reaction = EGSTN_MinionPendingReaction::None;
	FVector ReactionVelocity = FVector::ZeroVector;
	if (bStaggered)
	{
		switch (Knockback.Reaction)
		{
		case EGSTN_HitReaction::PushBack:
			Reaction = EGSTN_MinionPendingReaction::PushBack;
			// Horizontal by construction: the backend slides it on the ground plane and never lets it leave the floor
			ReactionVelocity = FVector::VectorPlaneProject(Knockback.LaunchVelocity, Self.Up);
			break;
		case EGSTN_HitReaction::Launch:
			Reaction = EGSTN_MinionPendingReaction::Launch;
			ReactionVelocity = Knockback.LaunchVelocity;
			break;
		default:
			Reaction = EGSTN_MinionPendingReaction::StaggerInPlace;
			break;
		}
	}

	// Bridge seam (G10b): the bridge applies the outcome extensions to its own columns and may ask for death (a lethal part broke). The
	// death then takes the same path as a health kill below; inert while unbound
	if (ApplyOutcomeExtensions.IsBound() && ApplyOutcomeExtensions.Execute(Handle, Context, Frame))
	{
		Store.Health[Slot] = 0.f;
	}

	const bool bDies = Store.Health[Slot] <= 0.f;
	if (bStaggered && !bDies)
	{
		// The stagger takes the minion out of its move (spec 5 stage 2.2) and gives its attack token back to the crowd
		CancelTimeline(Slot, Frame);
		AttackTokens.ReleaseSlot(Slot);

		const int32 HitstopFrames = FMath::Max(Knockback.HitstopFrames, 0);
		// The window opens on the frame of the hit (the fighter rule), so the drain at F+1 is already its second frame
		const int32 End = Hit.Frame + HitstopFrames;
		HitstopEndFrame[Slot] = Frame < HitstopEndFrame[Slot] ? FMath::Max(HitstopEndFrame[Slot], End) : End;
		int32 HitstunFrames = Knockback.HitstunFrames;
		if (Reaction != EGSTN_MinionPendingReaction::Launch && Rules)
		{
			// The locked part of a stagger is never shorter than the floor of the rules (the fighter's rule)
			HitstunFrames = FMath::Max(HitstunFrames, Rules->MinStaggerLockFrames);
		}
		HitstunEndFrame[Slot] = FMath::Max(HitstopEndFrame[Slot], Frame) + FMath::Max(HitstunFrames, 0);

		if (HitstopEndFrame[Slot] > Frame)
		{
			// Frozen until the window ends; the tier goes out then (StepVictimReaction, start of stage 3). A newer stagger replaces an older pending one
			Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Hitstop);
			PendingReaction[Slot] = static_cast<uint8>(Reaction);
			PendingReactionVelocity[Slot] = FVector3f(ReactionVelocity);
			MoveColumns.Velocity[Slot] = FVector3f::ZeroVector;
			MoveColumns.PendingLaunch[Slot] = FVector3f::ZeroVector;
		}
		else
		{
			ApplyReaction(Slot, Reaction, ReactionVelocity);
		}
	}

	if (OnOutcomeApplied.IsBound())
	{
		FGSTN_HitContextView Resolved;
		FGSTN_HitPipelineRunner::MakeView(Rules, Context, Resolved);
		OnOutcomeApplied.Broadcast(Handle, Hit, Resolved, Frame);
	}

	FQueuedEvent& HitEvent = QueuedEvents.AddDefaulted_GetRef();
	HitEvent.Kind = FQueuedEvent::EKind::Hit;
	HitEvent.Handle = Handle;
	HitEvent.Damage = Damage;
	HitEvent.OtherId = Hit.AttackerId;

	if (bDies)
	{
		// Death replaces whatever the minion was playing; the death timeline (or the corpse frames) ends in the despawn. A killing blow that
		// pushes or launches still carries the body away (a KO flies), with no hitstop wait: the state stays Dying
		AttackTokens.ReleaseSlot(Slot);
		CancelTimeline(Slot, Frame);
		PendingReaction[Slot] = 0;
		PendingReactionVelocity[Slot] = FVector3f::ZeroVector;
		Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Dying);
		StartTimeline(Slot, GSTNMinion::ReactionMoveIds::Death, Frame);
		if ((Reaction == EGSTN_MinionPendingReaction::Launch || Reaction == EGSTN_MinionPendingReaction::PushBack) && !ReactionVelocity.IsNearlyZero())
		{
			LaunchMinion(Handle, ReactionVelocity, Reaction == EGSTN_MinionPendingReaction::PushBack);
		}

		FQueuedEvent& DiedEvent = QueuedEvents.AddDefaulted_GetRef();
		DiedEvent.Kind = FQueuedEvent::EKind::Died;
		DiedEvent.Handle = Handle;
		DiedEvent.OtherId = Hit.AttackerId;

		// The authoritative KO for the attacker's credit (Combat.Event.KO + Combat.Entity.Minion), published after the stages
		FQueuedEvent& KOEvent = QueuedEvents.AddDefaulted_GetRef();
		KOEvent.Kind = FQueuedEvent::EKind::KO;
		KOEvent.Handle = Handle;
		KOEvent.OtherId = Hit.AttackerId;
		KOEvent.Frame = Frame;
	}
}

void UGSTN_MinionCombatSubsystem::ApplyReaction(int32 Slot, EGSTN_MinionPendingReaction Reaction, const FVector& Velocity)
{
	PendingReaction[Slot] = 0;
	PendingReactionVelocity[Slot] = FVector3f::ZeroVector;
	if (IsDyingOrDead(Store.ActionState[Slot]))
	{
		return;
	}
	const FGSTN_MinionHandle Handle = Store.HandleOfSlot(Slot);
	const bool bMoves = (Reaction == EGSTN_MinionPendingReaction::PushBack || Reaction == EGSTN_MinionPendingReaction::Launch) && !Velocity.IsNearlyZero();
	if (bMoves)
	{
		// The movement backend owns the knockback from here: it returns the minion to Idle when the slide stops or the flight lands
		LaunchMinion(Handle, Velocity, Reaction == EGSTN_MinionPendingReaction::PushBack);
	}
	else if (Reaction != EGSTN_MinionPendingReaction::None)
	{
		// A stagger in place (or a push with no speed): held in place until HitstunEndFrame
		Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Hitstun);
	}
	else
	{
		Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Idle);
	}
}

void UGSTN_MinionCombatSubsystem::StepVictimReaction(int32 Slot, int32 Frame)
{
	const EGSTN_MinionActionState State = static_cast<EGSTN_MinionActionState>(Store.ActionState[Slot]);
	if (State == EGSTN_MinionActionState::Hitstop && Frame >= HitstopEndFrame[Slot])
	{
		ApplyReaction(Slot, static_cast<EGSTN_MinionPendingReaction>(PendingReaction[Slot]), FVector(PendingReactionVelocity[Slot]));
	}
	if (static_cast<EGSTN_MinionActionState>(Store.ActionState[Slot]) == EGSTN_MinionActionState::Hitstun && Frame >= HitstunEndFrame[Slot])
	{
		Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Idle);
	}
}

void UGSTN_MinionCombatSubsystem::CancelTimeline(int32 Slot, int32 Frame)
{
	const uint8 MoveId = Store.MoveId[Slot];
	if (MoveId == 0 || MoveId == GSTNMinion::ReactionMoveIds::Death)
	{
		return;
	}
	if (IsMoveId(MoveId))
	{
		// A cancelled move still pays its cooldown, so a staggered minion does not swing again the frame it recovers
		const UGSTN_MinionDefinition* Definition = GetDefinition(Store.TypeIndex[Slot]);
		if (Definition && Definition->Moves.IsValidIndex(MoveId - 1))
		{
			NextMoveFrame[Slot * GSTNMinion::MaxMoves + (MoveId - 1)] = Frame + Definition->Moves[MoveId - 1].CooldownFrames;
		}
		AttackTokens.ReleaseSlot(Slot);
	}
	else if (Store.ActionState[Slot] == static_cast<uint8>(EGSTN_MinionActionState::Spawning))
	{
		Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Idle);
	}
	Store.MoveId[Slot] = 0;
	TimelinePrevFrame[Slot] = INDEX_NONE;
	PausedFrames[Slot] = 0;
	AttackerHitstopEndFrame[Slot] = INDEX_NONE;
}

bool UGSTN_MinionCombatSubsystem::HasHitBy(int32 Slot, const FHitByKey& Key) const
{
	const int32 Base = Slot * GSTNMinion::HitByRingSize;
	for (int32 Index = 0; Index < GSTNMinion::HitByRingSize; ++Index)
	{
		if (HitByRing[Base + Index] == Key)
		{
			return true;
		}
	}
	return false;
}

void UGSTN_MinionCombatSubsystem::AddHitBy(int32 Slot, const FHitByKey& Key)
{
	HitByRing[Slot * GSTNMinion::HitByRingSize + HitByHead[Slot]] = Key;
	HitByHead[Slot] = static_cast<uint8>((HitByHead[Slot] + 1) % GSTNMinion::HitByRingSize);
}

void UGSTN_MinionCombatSubsystem::PushLastAttacker(int32 Slot, int32 AttackerId)
{
	constexpr int32 Size = GSTNMinion::LastAttackerRingSize;
	LastAttackerRing[Slot * Size + LastAttackerHead[Slot]] = AttackerId;
	LastAttackerHead[Slot] = static_cast<uint8>((LastAttackerHead[Slot] + 1) % Size);
	LastAttackerCount[Slot] = static_cast<uint8>(FMath::Min<int32>(LastAttackerCount[Slot] + 1, Size));
}

bool UGSTN_MinionCombatSubsystem::HasAttackTarget(int32 Slot, int32 VictimId, int32 MoveStartFrame, uint8 TargetKey) const
{
	const int32 Base = Slot * GSTNMinion::AttackTargetRingSize;
	for (int32 Index = 0; Index < GSTNMinion::AttackTargetRingSize; ++Index)
	{
		const FAttackTarget& Target = AttackTargets[Base + Index];
		if (Target.VictimId == VictimId && Target.MoveStartFrame == MoveStartFrame && Target.TargetKey == TargetKey)
		{
			return true;
		}
	}
	return false;
}

int32 UGSTN_MinionCombatSubsystem::CountAttackTargets(int32 Slot, int32 MoveStartFrame, uint8 TargetKey) const
{
	int32 Count = 0;
	const int32 Base = Slot * GSTNMinion::AttackTargetRingSize;
	for (int32 Index = 0; Index < GSTNMinion::AttackTargetRingSize; ++Index)
	{
		const FAttackTarget& Target = AttackTargets[Base + Index];
		Count += Target.VictimId != INDEX_NONE && Target.MoveStartFrame == MoveStartFrame && Target.TargetKey == TargetKey ? 1 : 0;
	}
	return Count;
}

void UGSTN_MinionCombatSubsystem::AddAttackTarget(int32 Slot, int32 VictimId, int32 MoveStartFrame, uint8 TargetKey)
{
	// Full ring: the oldest is evicted (the fighters' rule since G1)
	FAttackTarget& Target = AttackTargets[Slot * GSTNMinion::AttackTargetRingSize + AttackTargetHead[Slot]];
	Target.VictimId = VictimId;
	Target.MoveStartFrame = MoveStartFrame;
	Target.TargetKey = TargetKey;
	AttackTargetHead[Slot] = static_cast<uint8>((AttackTargetHead[Slot] + 1) % GSTNMinion::AttackTargetRingSize);
}

void UGSTN_MinionCombatSubsystem::ResetHitColumns(int32 Slot)
{
	KnockbackPercent[Slot] = 0.f;
	HitstopEndFrame[Slot] = INDEX_NONE;
	HitstunEndFrame[Slot] = INDEX_NONE;
	PendingReaction[Slot] = 0;
	PendingReactionVelocity[Slot] = FVector3f::ZeroVector;
	AttackerHitstopEndFrame[Slot] = INDEX_NONE;
	PausedFrames[Slot] = 0;
	for (int32 Index = 0; Index < GSTNMinion::LastAttackerRingSize; ++Index)
	{
		LastAttackerRing[Slot * GSTNMinion::LastAttackerRingSize + Index] = INDEX_NONE;
	}
	LastAttackerHead[Slot] = 0;
	LastAttackerCount[Slot] = 0;
	for (int32 Index = 0; Index < GSTNMinion::HitByRingSize; ++Index)
	{
		HitByRing[Slot * GSTNMinion::HitByRingSize + Index] = FHitByKey();
	}
	HitByHead[Slot] = 0;
	for (int32 Index = 0; Index < GSTNMinion::AttackTargetRingSize; ++Index)
	{
		AttackTargets[Slot * GSTNMinion::AttackTargetRingSize + Index] = FAttackTarget();
	}
	AttackTargetHead[Slot] = 0;
	// Tokens name slots: a recycled slot must not inherit the previous occupant's token or wait
	AttackTokens.ReleaseSlot(Slot);
	History.ClearSlot(Slot);
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
	// A new clock: no paused frames, no attacker hitstop carried over from the previous move
	PausedFrames[Slot] = 0;
	AttackerHitstopEndFrame[Slot] = INDEX_NONE;
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
		// The attack is over: its token goes back to the crowd
		AttackTokens.ReleaseSlot(Slot);
	}
	else if (MoveId == GSTNMinion::ReactionMoveIds::Spawn)
	{
		Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Idle);
	}
	else if (MoveId == GSTNMinion::ReactionMoveIds::Death)
	{
		Store.ActionState[Slot] = static_cast<uint8>(EGSTN_MinionActionState::Dead);
		// The body stays CorpseSeconds (G9a), counted in whole fixed frames rounded up (the small bias keeps 2 s at exactly 125 frames
		// despite the float step). 0 s frees on this frame as before. A corpse keeps no token (the death already released it)
		AttackTokens.ReleaseSlot(Slot);
		const UGSTN_MinionDefinition* Definition = GetDefinition(Store.TypeIndex[Slot]);
		const float CorpseSeconds = Definition ? FMath::Max(Definition->CorpseSeconds, 0.f) : 0.f;
		const int32 CorpseFrames = FMath::CeilToInt(CorpseSeconds / FixedStepSeconds - 1.0e-3f);
		if (CorpseFrames <= 0)
		{
			DespawnList.Add(Slot);
		}
		else
		{
			CorpseEndFrame[Slot] = Frame + CorpseFrames;
			Corpses.AddUnique(Slot);
		}
	}
	Store.MoveId[Slot] = 0;
	TimelinePrevFrame[Slot] = INDEX_NONE;
	PausedFrames[Slot] = 0;
	AttackerHitstopEndFrame[Slot] = INDEX_NONE;
}

void UGSTN_MinionCombatSubsystem::StageTimelineAdvance(int32 Frame)
{
	SCOPE_CYCLE_COUNTER(STAT_GSTN_MinionTimeline);

	// S(F), the fighters at the end of F-1: what the range checks and the attack queries read. Built lazily (most frames no minion attacks)
	const FGSTN_StepSnapshot* Snapshot = nullptr;
	bool bSnapshotAcquired = false;
	auto GetSnapshot = [this, Frame, &Snapshot, &bSnapshotAcquired]()
	{
		if (!bSnapshotAcquired)
		{
			bSnapshotAcquired = true;
			Snapshot = AcquireFighterSnapshot(Frame);
		}
		return Snapshot;
	};

	// a. The end of the victims' hitstops and staggers, then the moves that may start. A move against a fighter needs an attack token
	// (spec 9.3): the requests of the frame are collected first and granted together in ascending slot order, so who gets the token does
	// not depend on which slot asked first
	StartCandidates.Reset();
	for (const int32 Slot : Scheduled)
	{
		if (!Store.IsSlotAlive(Slot))
		{
			continue;
		}
		StepVictimReaction(Slot, Frame);
		if (Store.MoveId[Slot] != 0)
		{
			continue;
		}
		// A slot that plays nothing holds no token; a wait left from an earlier frame is dropped and asked again below if still wanted
		AttackTokens.ReleaseSlot(Slot);
		if (!Store.Intent[Slot].HasCommand())
		{
			continue;
		}
		int32 MoveIndex = INDEX_NONE;
		int32 TargetId = INDEX_NONE;
		if (!CanStartCommand(Slot, Frame, GetSnapshot(), MoveIndex, TargetId))
		{
			continue;
		}
		FStartCandidate& Candidate = StartCandidates.AddDefaulted_GetRef();
		Candidate.Slot = Slot;
		Candidate.MoveIndex = MoveIndex;
		Candidate.TargetId = TargetId;
		Candidate.bNeedsToken = GSTNEntityKinds::KindOfEntityId(TargetId) == GSTNEntityKinds::Fighter;
		if (Candidate.bNeedsToken)
		{
			AttackTokens.Request(TargetId, Slot);
		}
	}
	AttackTokens.Resolve();
	for (const FStartCandidate& Candidate : StartCandidates)
	{
		if (Candidate.bNeedsToken && !AttackTokens.Holds(Candidate.TargetId, Candidate.Slot))
		{
			// The fighter has its attackers already: the command stays, the brain circles and the next run asks again
			++LastBatch.TokensDenied;
			continue;
		}
		StartTimeline(Candidate.Slot, static_cast<uint8>(Candidate.MoveIndex + 1), Frame);
		Store.Intent[Candidate.Slot].Command = FGameplayTag();
		++LastBatch.AttacksStarted;
	}

	// b. Advance every running timeline, pausing the attacker hitstop, and query the open hitboxes of the moves
	for (const int32 Slot : Scheduled)
	{
		if (!Store.IsSlotAlive(Slot))
		{
			continue;
		}
		TraceStage(EStage::TimelineAdvance, Slot);

		const uint8 MoveId = Store.MoveId[Slot];
		if (MoveId == 0)
		{
			continue;
		}

		// The minion's own hitstop as an attacker stands its clock still (the frames in (last run, Frame] up to the window's end), however
		// many frames the LOD let pass; its victim hitstop needs no pause because a staggering hit cancels the move
		const int32 PreviousRun = LastRunFrame[Slot] != INDEX_NONE ? LastRunFrame[Slot] : Frame - 1;
		const int32 HitstopEnd = AttackerHitstopEndFrame[Slot];
		if (HitstopEnd != INDEX_NONE)
		{
			PausedFrames[Slot] += FMath::Clamp(HitstopEnd - PreviousRun, 0, FMath::Max(Frame - PreviousRun, 0));
		}
		const bool bInAttackerHitstop = HitstopEnd != INDEX_NONE && Frame <= HitstopEnd;

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
		Params.PausedFrames = PausedFrames[Slot];
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

		// Hits against the fighters of S(F) (end-of-F-1 transforms: the store has not moved this frame yet). A timeline that stands still
		// in its hitstop hits nothing new, like the fighter's attacker step
		if (IsMoveId(MoveId) && !bInAttackerHitstop)
		{
			if (const FGSTN_StepSnapshot* Fighters = GetSnapshot())
			{
				RunAttackQueries(Slot, Frame, Timeline->Data, TimelineOutput.Frame, *Fighters);
			}
		}

		// A ricochet may have cancelled the move inside the queries
		if (Store.MoveId[Slot] == MoveId && TimelineOutput.bFinished)
		{
			EndTimeline(Slot, Frame);
		}
	}
}

bool UGSTN_MinionCombatSubsystem::CanStartCommand(int32 Slot, int32 Frame, const FGSTN_StepSnapshot* Snapshot, int32& OutMoveIndex, int32& OutTargetId) const
{
	// A known command, no timeline playing, a locomotion state, off cooldown, and T0/T1 (attack timelines never start at T2/T3, spec 7)
	const FGSTN_MinionIntent& Intent = Store.Intent[Slot];
	if (Store.MoveId[Slot] != 0 || !IsLocomotionState(Store.ActionState[Slot])
		|| Store.LODTier[Slot] > static_cast<uint8>(EGSTN_MinionLODTier::Near)
		|| !Intent.HasCommand() || Intent.IsStale(Frame))
	{
		return false;
	}
	const UGSTN_MinionDefinition* Definition = GetDefinition(Store.TypeIndex[Slot]);
	const int32 MoveIndex = Definition ? Definition->FindMoveIndex(Intent.Command) : INDEX_NONE;
	if (MoveIndex == INDEX_NONE || MoveIndex >= GSTNMinion::MaxMoves
		|| TimelineOfMoveId(Slot, static_cast<uint8>(MoveIndex + 1)) == nullptr
		|| Frame < NextMoveFrame[Slot * GSTNMinion::MaxMoves + MoveIndex])
	{
		return false;
	}

	// Range to the target, when the brain named one: a fighter from S(F) (where the hit query will look), a minion from the store. A target
	// that is not there (dead, left, a stale id) refuses the start, so a brain cannot swing at nothing
	const int32 TargetId = Intent.TargetEntityId;
	if (TargetId != INDEX_NONE)
	{
		FVector TargetLocation = FVector::ZeroVector;
		bool bFound = false;
		if (GSTNEntityKinds::IsMinionEntityId(TargetId))
		{
			const FGSTN_MinionHandle Target = Store.HandleFromEntityId(TargetId);
			if (!Target.IsNull() && !PendingActivation[Target.Index])
			{
				TargetLocation = FVector(Store.Position[Target.Index]);
				bFound = true;
			}
		}
		else if (Snapshot)
		{
			if (const FGSTN_SnapshotEntry* Entry = Snapshot->Find(TargetId))
			{
				TargetLocation = Entry->Transform.GetLocation();
				bFound = true;
			}
		}
		if (!bFound)
		{
			return false;
		}
		const FGSTN_MinionMove& Move = Definition->Moves[MoveIndex];
		const double Distance = FVector::Dist(TargetLocation, FVector(Store.Position[Slot]));
		if (Distance < Move.MinRange || Distance > Move.MaxRange)
		{
			return false;
		}
	}
	OutMoveIndex = MoveIndex;
	OutTargetId = TargetId;
	return true;
}

const FGSTN_StepSnapshot* UGSTN_MinionCombatSubsystem::AcquireFighterSnapshot(int32 Frame)
{
	UGSTN_CombatWorldSubsystem* World = GetCombatWorld();
	// No registered fighter: nothing to hit or to range-check against
	if (!World || World->GetNumRegisteredEntities() == 0)
	{
		return nullptr;
	}
	// The first combat tick of F built S(F) already (the hook runs after every model); the frame numbers are the same Step.Frame
	const FGSTN_StepSnapshot& Held = World->GetSnapshot();
	if (Held.Frame == Frame)
	{
		return &Held;
	}
	// No fighter ticked this frame (an empty server, a test world): build S(F) as that tick would, from end-of-F-1 fighters. The minion
	// entries in it carry this frame's drain, which is harmless: minions only query fighters, and the next frame builds its own
	return &World->EnsureSnapshot(Frame);
}

void UGSTN_MinionCombatSubsystem::RunAttackQueries(int32 Slot, int32 Frame, const FGSTN_CombatTimelineData& Timeline, int32 TimelineFrame, const FGSTN_StepSnapshot& Snapshot)
{
	UGSTN_CombatWorldSubsystem* World = GetCombatWorld();
	if (!World)
	{
		return;
	}
	FGSTN_TimelinePlayer::EvaluateHits(Timeline, TimelineFrame, HitScratch);
	if (HitScratch.Hitboxes.IsEmpty())
	{
		return;
	}

	const UGSTN_CombatRules* Rules = CombatRules;
	const UGSTN_MinionDefinition* Definition = GetDefinition(Store.TypeIndex[Slot]);
	const int32 AttackerId = GSTNEntityKinds::MakeMinionEntityId(static_cast<uint16>(Slot));
	const uint8 AttackerTeam = Definition ? Definition->TeamId : 255;
	const int32 MaxHitsPerVictim = Definition ? FMath::Max(Definition->MaxHitsPerFrame, 1) : 1;
	const FTransform Root = RootOf(Slot);
	const FVector Up = Root.GetUnitAxis(EAxis::Z);
	const FVector AttackerFacing = Root.GetUnitAxis(EAxis::X);
	const int32 StartFrame = Store.MoveStartFrame[Slot];

	/** A hitbox that touches a fighter this frame (the fighter attacker's candidate, same rules) */
	struct FCandidate
	{
		int32 EntryIndex = INDEX_NONE;
		int32 HitboxIndex = 0;
		const FGSTN_HitboxSpec* Spec = nullptr;
		FGSTN_HitData Hit;
		FGameplayTag Relationship;
		bool bRelationshipForced = false;
		int32 HurtboxIndex = 0;
		EGSTN_HurtState HurtState = EGSTN_HurtState::Normal;
		FVector HitLocation = FVector::ZeroVector;
		FVector HitboxCenter = FVector::ZeroVector;
		FVector HitboxVelocity = FVector::ZeroVector;
		uint8 TargetKey = 0;
	};
	TArray<FCandidate, TInlineAllocator<8>> Candidates;
	TArray<int32, TInlineAllocator<8>> Overlapped;

	for (int32 HitboxIndex = 0; HitboxIndex < HitScratch.Hitboxes.Num(); ++HitboxIndex)
	{
		const FGSTN_ActiveHitbox& Active = HitScratch.Hitboxes[HitboxIndex];
		if (!Active.Spec)
		{
			continue;
		}
		const FGSTN_HitboxSpec& Spec = *Active.Spec;
		FTransform Carrier;
		if (!BuildCarrier(Spec, Timeline, Root, TimelineFrame, Active.TrackFrame, Carrier))
		{
			continue;
		}
		FTransform PreviousCarrier = Carrier;
		if (!BuildCarrier(Spec, Timeline, Root, FMath::Max(TimelineFrame - 1, 0), FMath::Max(Active.TrackFrame - 1, 0), PreviousCarrier))
		{
			PreviousCarrier = Carrier;
		}
		const FVector HitboxCenter = ShapeCenterOf(Spec.Shape, Carrier);
		const FVector PreviousCenter = ShapeCenterOf(Spec.Shape, PreviousCarrier);
		const FVector HitboxVelocity = (HitboxCenter - PreviousCenter) / FMath::Max<double>(FixedStepSeconds, UE_SMALL_NUMBER);

		double VariantScale = 1.0;
		for (const TPair<FGameplayTag, FGSTN_HitVariant>& Pair : Spec.RelationshipVariants)
		{
			VariantScale = FMath::Max<double>(VariantScale, Pair.Value.ShapeScale);
		}
		const double Reach = (Spec.Shape.Extent.GetMax() * FMath::Max(Carrier.GetScale3D().GetAbsMax(), PreviousCarrier.GetScale3D().GetAbsMax())
			+ Spec.Shape.Local.GetLocation().Size()) * VariantScale;
		FBox Bounds(ForceInit);
		Bounds += HitboxCenter;
		Bounds += PreviousCenter;
		Bounds = Bounds.ExpandBy(Reach);
		CandidateScratch.Reset();
		Snapshot.QueryCandidates(Bounds, CandidateScratch);

		for (const int32 EntryIndex : CandidateScratch)
		{
			const FGSTN_SnapshotEntry& Victim = Snapshot.Entries[EntryIndex];
			// The Fighter kind mask: minion-minion is never tested (spec 5 stage 2.3), whatever the teams
			if (GSTNEntityKinds::KindOfEntityId(Victim.EntityId) != GSTNEntityKinds::Fighter || !Victim.Hurtboxes || Victim.Hurtboxes->Hurtboxes.IsEmpty())
			{
				continue;
			}

			// Relationship, the rules' hittable list and the hitbox's variant, exactly as a fighter attacker decides
			FGSTN_RelationshipQuery Query;
			Query.AttackerEntityId = AttackerId;
			Query.AttackerTeamId = AttackerTeam;
			Query.VictimEntityId = Victim.EntityId;
			Query.VictimTeamId = Victim.Team.GetId();
			const FGameplayTag Relationship = Rules ? Rules->ResolveRelationship(Query) : UGSTN_RelationshipResolver::DefaultRule(Query);
			const FGSTN_HitVariant* Variant = Spec.RelationshipVariants.Find(Relationship);
			if (Variant && Variant->bIgnore)
			{
				continue;
			}
			if (!(Variant && Variant->bForceHittable) && (Rules ? !Rules->IsRelationshipHittable(Relationship)
				: (Relationship != GSTNCombatHitTags::Relationship_Enemy.GetTag() && Relationship != GSTNCombatHitTags::Relationship_Neutral.GetTag())))
			{
				continue;
			}
			// Attacker-side dedupe: a fighter is hit once per group (or re-hit interval) and move execution
			const uint8 TargetKey = Spec.MakeTargetKey(Frame - StartFrame);
			if (HasAttackTarget(Slot, Victim.EntityId, StartFrame, TargetKey))
			{
				continue;
			}

			const FGSTN_HitShape HitShape = Variant && !FMath::IsNearlyEqual(Variant->ShapeScale, 1.f) ? UGSTN_HitShapeUtils::ScaleShape(Spec.Shape, Variant->ShapeScale) : Spec.Shape;
			const TArray<FGSTN_Hurtbox>& Hurtboxes = Victim.Hurtboxes->Hurtboxes;
			Overlapped.Reset();
			for (int32 HurtIndex = 0; HurtIndex < Hurtboxes.Num(); ++HurtIndex)
			{
				if (Victim.HurtStates.IsValidIndex(HurtIndex) && Victim.HurtStates[HurtIndex].State == EGSTN_HurtState::Intangible)
				{
					continue;
				}
				const bool bTouches = Spec.bSweepBetweenFrames
					? UGSTN_HitShapeUtils::SweptOverlap(HitShape, PreviousCarrier, Carrier, Hurtboxes[HurtIndex].Shape, Victim.Transform)
					: UGSTN_HitShapeUtils::Overlap(HitShape, Carrier, Hurtboxes[HurtIndex].Shape, Victim.Transform);
				if (bTouches)
				{
					Overlapped.Add(HurtIndex);
				}
			}
			for (int32 ExtraPosition = 0; ExtraPosition < Victim.ExtraHurtboxes.Num(); ++ExtraPosition)
			{
				const FGSTN_HitShape& ExtraShape = Victim.ExtraHurtboxes[ExtraPosition].Shape;
				const bool bTouches = Spec.bSweepBetweenFrames
					? UGSTN_HitShapeUtils::SweptOverlap(HitShape, PreviousCarrier, Carrier, ExtraShape, Victim.Transform)
					: UGSTN_HitShapeUtils::Overlap(HitShape, Carrier, ExtraShape, Victim.Transform);
				if (bTouches)
				{
					Overlapped.Add(GSTNHurtboxIndex::FromExtraPosition(ExtraPosition));
				}
			}
			if (Overlapped.IsEmpty())
			{
				continue;
			}
			const FGSTN_HitData VariantHit = Variant ? UGSTN_HitDataUtils::ApplyVariant(Spec.Data, *Variant) : Spec.Data;
			const int32 Picked = UGSTN_HitShapeUtils::PickHurtboxIndex(Overlapped, Hurtboxes, Victim.ExtraHurtboxes, VariantHit.HitType);
			const FGSTN_HitShape* PickedShape = Victim.FindHurtboxShape(Picked);
			if (Picked == INDEX_NONE || !PickedShape)
			{
				continue;
			}

			FCandidate Candidate;
			Candidate.EntryIndex = EntryIndex;
			Candidate.HitboxIndex = HitboxIndex;
			Candidate.Spec = &Spec;
			Candidate.Hit = VariantHit;
			Candidate.Relationship = Relationship;
			Candidate.bRelationshipForced = Variant && Variant->bForceHittable;
			Candidate.HurtboxIndex = Picked;
			Candidate.HurtState = Victim.HurtStates.IsValidIndex(Picked) ? Victim.HurtStates[Picked].State : EGSTN_HurtState::Normal;
			Candidate.HitboxCenter = HitboxCenter;
			Candidate.HitLocation = (HitboxCenter + ShapeCenterOf(*PickedShape, Victim.Transform)) * 0.5;
			Candidate.HitboxVelocity = HitboxVelocity;
			Candidate.TargetKey = TargetKey;

			// Sweetspots: one hit per (victim, group) per frame, the highest priority (ties: the lower hitbox index, which came first)
			bool bReplaced = false;
			for (FCandidate& Existing : Candidates)
			{
				if (Existing.EntryIndex == EntryIndex && Existing.Spec->HitGroup == Spec.HitGroup)
				{
					if (Candidate.Hit.Priority > Existing.Hit.Priority)
					{
						Existing = Candidate;
					}
					bReplaced = true;
					break;
				}
			}
			if (!bReplaced)
			{
				Candidates.Add(Candidate);
			}
		}
	}
	if (Candidates.IsEmpty())
	{
		return;
	}

	// Victims by id, hitboxes by index: the order the target ring and the fighters' inboxes are written in
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		return A.EntryIndex != B.EntryIndex ? A.EntryIndex < B.EntryIndex : A.HitboxIndex < B.HitboxIndex;
	});

	int32 LongestHitstop = 0;
	bool bRicochet = false;
	int32 CurrentEntry = INDEX_NONE;
	int32 HitsOnVictim = 0;
	for (const FCandidate& Candidate : Candidates)
	{
		if (Candidate.EntryIndex != CurrentEntry)
		{
			CurrentEntry = Candidate.EntryIndex;
			HitsOnVictim = 0;
		}
		// The type's cap of hits on one victim in one frame (several groups touching at once)
		if (HitsOnVictim >= MaxHitsPerVictim)
		{
			continue;
		}
		if (CountAttackTargets(Slot, StartFrame, Candidate.TargetKey) >= FMath::Clamp<int32>(Candidate.Spec->MaxTargetsPerGroup, 1, GSTNMinion::AttackTargetRingSize))
		{
			continue;
		}
		const FGSTN_SnapshotEntry& Victim = Snapshot.Entries[Candidate.EntryIndex];

		// The attacker's prediction of the hit on the fighter's view: the same function the fighters use, for the minion's own hitstop.
		// The fighter runs the authoritative pipeline when it drains the entry at F+1
		FGSTN_HitResolutionContext Context;
		FGSTN_ResolvedHit& Incoming = Context.Incoming;
		Incoming.Hit = Candidate.Hit;
		Incoming.AttackerEntityId = AttackerId;
		Incoming.AttackerTeamId = AttackerTeam;
		Incoming.Relationship = Candidate.Relationship == GSTNCombatHitTags::Relationship_Self.GetTag() ? GSTNCombatHitTags::Relationship_Enemy.GetTag() : Candidate.Relationship;
		Incoming.bRelationshipForced = Candidate.bRelationshipForced;
		Incoming.Up = Up;
		Incoming.AttackerFacing = AttackerFacing;
		Incoming.Frame = Frame;
		const FGSTN_HitShape* VictimShape = Victim.FindHurtboxShape(Candidate.HurtboxIndex);
		const FVector VictimCenter = VictimShape ? ShapeCenterOf(*VictimShape, Victim.Transform) : Victim.Transform.GetLocation();
		Incoming.HitboxToVictim = (VictimCenter - Candidate.HitboxCenter).GetSafeNormal(UE_SMALL_NUMBER, AttackerFacing);
		Incoming.HitboxVelocity = Candidate.HitboxVelocity;
		Incoming.bVictimGrounded = false;
		Context.Victim = FGSTN_CombatVictim::MakeVictimView(Victim, Rules);
		Context.HurtboxIndex = static_cast<uint8>(Candidate.HurtboxIndex);
		Context.HurtState = Candidate.HurtState;
		if (Victim.HurtStates.IsValidIndex(Candidate.HurtboxIndex))
		{
			Context.ArmorThreshold = Victim.HurtStates[Candidate.HurtboxIndex].ArmorThreshold;
		}
		Context.HurtboxSet = Victim.Hurtboxes;
		if (Rules)
		{
			FGSTN_HitPipelineRunner::Run(*Rules, Context);
		}
		if (!Context.Outcome.bHit)
		{
			continue;
		}
		AddAttackTarget(Slot, Victim.EntityId, StartFrame, Candidate.TargetKey);
		++HitsOnVictim;

		FGSTN_HitContextView ContextView;
		FGSTN_HitPipelineRunner::MakeView(Rules, Context, ContextView);
		const bool bHitRicochet = FGSTN_AttackerOutcomeRegistry::Get().Evaluate(Rules, ContextView) == EGSTN_AttackerHitOutcome::Ricochet;
		bRicochet |= bHitRicochet;
		if (!bHitRicochet && Context.Outcome.bHasKnockback)
		{
			LongestHitstop = FMath::Max(LongestHitstop, FMath::Max(Context.Outcome.Knockback.AttackerHitstopFrames, 0));
		}

		FGSTN_HitInboxEntry Entry;
		Entry.Frame = Frame;
		Entry.AttackerId = AttackerId;
		Entry.AttackerTeam = AttackerTeam;
		Entry.Hit = Candidate.Hit;
		Entry.HitLocation = Candidate.HitLocation;
		Entry.HitboxVelocity = Candidate.HitboxVelocity;
		Entry.AttackerYaw = static_cast<float>(AttackerFacing.Rotation().Yaw);
		Entry.Relationship = GSTNHitRelationship::ToIndex(Candidate.Relationship);
		if (Candidate.bRelationshipForced && Entry.Relationship != 255)
		{
			Entry.Relationship |= GSTNHitRelationship::ForcedBit;
		}
		Entry.HurtboxIndex = static_cast<uint8>(Candidate.HurtboxIndex);
		Entry.AttackerHitboxIndex = static_cast<uint8>(FMath::Min(Candidate.HitboxIndex, 255));
		Entry.HurtState = static_cast<uint8>(Candidate.HurtState);
		Entry.bRicochet = bHitRicochet;
		// Into the fighter's inbox (the server ticks that instance), stamped F: drained at F+1, a correction on the player's client
		if (World->PostHit(Victim.EntityId, Entry))
		{
			++LastBatch.AttackHits;
		}
	}

	if (LongestHitstop > 0)
	{
		// The frame of the hit runs on, the next LongestHitstop frames stand still (the fighter's window)
		AttackerHitstopEndFrame[Slot] = FMath::Max(AttackerHitstopEndFrame[Slot], Frame + LongestHitstop);
	}
	if (bRicochet)
	{
		// No recoil timeline for minions yet: the move just stops (its cooldown applies)
		CancelTimeline(Slot, Frame);
	}
}

void UGSTN_MinionCombatSubsystem::StageMovement(int32 Frame)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(GSTN_MinionMovement);
	SCOPE_CYCLE_COUNTER(STAT_GSTN_MinionMovement);
	// Wall clock for the bench's per-minion-frame mean only; nothing below reads it
	const double StartSeconds = FPlatformTime::Seconds();

	MoveSlots.Reset();
	for (const int32 Slot : Scheduled)
	{
		if (!Store.IsSlotAlive(Slot))
		{
			continue;
		}
		TraceStage(EStage::Movement, Slot);

		// A corpse (G9a) stays where its death left it: no gravity, no collision, no steering
		if (Store.ActionState[Slot] == static_cast<uint8>(EGSTN_MinionActionState::Dead))
		{
			Store.Speed[Slot] = 0.f;
			continue;
		}
		// Dormant minions are frozen where they are (spec 7): no gravity, no collision
		if (Store.LODTier[Slot] == static_cast<uint8>(EGSTN_MinionLODTier::Dormant))
		{
			Store.Speed[Slot] = 0.f;
			continue;
		}
		const UGSTN_MinionDefinition* Definition = GetDefinition(Store.TypeIndex[Slot]);
		if (!Definition)
		{
			continue;
		}
		if (Definition->MovementBackend == EGSTN_MinionMovementBackend::Mover && !bWarnedMoverBackend)
		{
			// G4: backend A (Mover, non-predicted) is not built (spike 3 decision B); the type runs the lightweight backend
			bWarnedMoverBackend = true;
			UE_LOG(LogGSTNMinion, Warning, TEXT("Minion type %s asks for the Mover backend, which is not built: using the lightweight one"), *GetNameSafe(Definition));
		}
		MoveSlots.Add(Slot);
	}

	UWorld* World = GetWorld();
	const bool bObserved = OnMovementStepped.IsBound();
	if (bObserved)
	{
		PreviousPositions = Store.Position;
	}

	if (World && !MoveSlots.IsEmpty())
	{
		MoveContext.World = World;
		MoveContext.Store = &Store;
		MoveContext.Columns = &MoveColumns;
		MoveContext.Definitions = Definitions;
		MoveContext.LastRunFrame = LastRunFrame;
		MoveContext.Frame = Frame;
		MoveContext.FixedStepSeconds = FixedStepSeconds;
		MoveContext.GravityAt = GravitySource;
		MoveContext.Settings = MovementSettings;
		MoveContext.ObjectQuery = FCollisionObjectQueryParams();
		for (const TEnumAsByte<ECollisionChannel> Channel : MovementSettings.BlockingObjectTypes)
		{
			MoveContext.ObjectQuery.AddObjectTypesToQuery(Channel);
		}
		MoveContext.QueryParams = FCollisionQueryParams(SCENE_QUERY_STAT(GSTN_MinionMove), false);
		const AWorldSettings* WorldSettings = World->GetWorldSettings();
		MoveContext.KillZ = WorldSettings ? WorldSettings->KillZ : -UE_BIG_NUMBER;
		MoveContext.FellOut.Reset();
		MoveContext.MinionFrames = 0;

		LightweightMovement.StepBatch(MoveContext, MoveSlots);

		for (const int32 Slot : MoveContext.FellOut)
		{
			// Fell out of the world: a robustness failure (spike 3 counts it), freed with the dead after the events
			++BenchTotals.FellOut;
			DespawnList.AddUnique(Slot);
			UE_LOG(LogGSTNMinion, Verbose, TEXT("Minion slot %d fell below KillZ at frame %d"), Slot, Frame);
		}
		BenchTotals.MovementMinionFrames += MoveContext.MinionFrames;
	}
	BenchTotals.MovementSeconds += FPlatformTime::Seconds() - StartSeconds;

	if (bObserved)
	{
		OnMovementStepped.Broadcast(Frame, MoveSlots, PreviousPositions);
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

		// History of frame F (end of F) for the rewound queries: pose, swept bound, and the flags a rewound hit reads. A dying minion is
		// recorded intangible (the snapshot leaves it out), so a rewind to its last frames cannot hit it either
		const int32 TypeIndex = Store.TypeIndex[Slot];
		const float BoundRadius = ResolvedTypes.IsValidIndex(TypeIndex) ? ResolvedTypes[TypeIndex].BoundRadius : 0.f;
		uint8 Flags = IsDyingOrDead(Store.ActionState[Slot]) ? GSTNVictimReactionFlags::Intangible : 0;
		Flags |= (Frame + 1 < HitstopEndFrame[Slot] || Frame + 1 <= AttackerHitstopEndFrame[Slot]) ? GSTNVictimReactionFlags::InHitstop : 0;
		History.Write(Slot, Frame, Store.Position[Slot], Store.YawDegrees[Slot], BoundRadius, Flags);
		// G6: pack item dirtying at the tier's rate
	}

	// The candidate hash holds every activated minion where it is now (a skipped slot has not moved)
	HashSlots.Reset();
	for (int32 Slot = Store.NextAliveSlot(0); Slot != INDEX_NONE; Slot = Store.NextAliveSlot(Slot + 1))
	{
		if (!PendingActivation[Slot])
		{
			HashSlots.Add(Slot);
		}
	}
	History.RebuildHash(HashSlots, Store.Position);

	NotifyPublishObservers(Frame);

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
	UGSTN_SimEventSubsystem* SimEvents = nullptr;
	for (const FQueuedEvent& Event : Events)
	{
		if (!Store.IsValid(Event.Handle))
		{
			continue;
		}
		AGSTN_MinionActor* Actor = SlotActors[Event.Handle.Index];
		UGSTN_MinionCombatComponent* Component = Actor ? Actor->GetMinionComponent() : nullptr;
		switch (Event.Kind)
		{
		case FQueuedEvent::EKind::Hit:
			if (Component)
			{
				Component->OnMinionHit.Broadcast(Event.Damage, Event.OtherId);
			}
			break;
		case FQueuedEvent::EKind::Died:
			if (Component)
			{
				Component->OnMinionDied.Broadcast(Event.OtherId);
			}
			break;
		case FQueuedEvent::EKind::LODChanged:
			if (Component)
			{
				Component->OnLODChanged.Broadcast(static_cast<EGSTN_MinionLODTier>(Event.OldTier), static_cast<EGSTN_MinionLODTier>(Event.NewTier));
			}
			break;
		case FQueuedEvent::EKind::KO:
		{
			// The authoritative stream (quests, kill feeds): published once, here, outside the stages. The subsystem only exists on the
			// authority, so no role check is needed. Instigator = the attacker's actor when it is a registered fighter
			SimEvents = SimEvents ? SimEvents : UGSTN_SimEventSubsystem::Get(this);
			if (SimEvents)
			{
				FGSTN_SimEvent KO;
				KO.EventType = GSTNCombatTags::Event_KO;
				KO.Frame = Event.Frame;
				const UGSTN_CombatWorldSubsystem* World = GetCombatWorld();
				KO.Instigator = World ? World->FindOwnerActor(Event.OtherId) : nullptr;
				KO.Target = Actor;
				KO.Tags.AddTag(GSTNCombatTags::KOReason_Health);
				KO.Tags.AddTag(GSTNMinionTags::Entity_Minion);
				SimEvents->Broadcast(KO);
			}
			break;
		}
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
	FreeDueCorpses(CurrentBatchFrame);
}

void UGSTN_MinionCombatSubsystem::FreeDueCorpses(int32 Frame)
{
	// Insertion order is the order the deaths ended in (ascending slot within a frame), so the frees are deterministic. The list is the
	// corpses only: a handful, never a walk of the store
	for (int32 Index = 0; Index < Corpses.Num();)
	{
		const int32 Slot = Corpses[Index];
		const int32 EndFrame = CorpseEndFrame.IsValidIndex(Slot) ? CorpseEndFrame[Slot] : INDEX_NONE;
		if (EndFrame == INDEX_NONE || !Store.IsSlotAlive(Slot))
		{
			// Freed by something else (Despawn, the bench, a fall out of the world) since it died
			Corpses.RemoveAt(Index);
		}
		else if (Frame >= EndFrame)
		{
			Corpses.RemoveAt(Index);
			FreeSlot(Slot);
		}
		else
		{
			++Index;
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
	NetReleaseSlot(Slot);
	CorpseEndFrame[Slot] = INDEX_NONE;
	// A freed platform slot no longer counts (the store resets the flags with the slot)
	if (Store.ExtFlags[Slot] & GSTNMinion::ExtFlag::PlatformActor)
	{
		--NumPlatformFlagged;
	}
	// Pending hits of this occupant are dropped by the generation check of the drain
	Store.Free(Handle);
	TimelinePrevFrame[Slot] = INDEX_NONE;
	LastRunFrame[Slot] = INDEX_NONE;
	MoveColumns.ResetSlot(Slot);
	// Tokens, rings and history name the slot: they go with the occupant (the token goes back on death, here at the latest)
	ResetHitColumns(Slot);
	++LastBatch.Despawned;
	// G9: publish Event.MinionDespawned
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Lag compensation (spike 5)

void UGSTN_MinionCombatSubsystem::GatherRewoundCandidates(const FBox& Bounds, float RewindMs, int32 FixedStepMS, TArray<FGSTN_MinionRewoundPose>& OutPoses)
{
	OutPoses.Reset();
	if (!bActive || FixedStepMS <= 0 || LastBatchFrame == INDEX_NONE || !Bounds.IsValid)
	{
		return;
	}
	// Wall clock for the spike 5 report only; nothing below reads it
	const double StartSeconds = FPlatformTime::Seconds();

	// The frame whose end the attacker's client displayed (one conversion shared with the fighters' RewindActors), never in the future
	const int32 RewindFrame = FMath::Min(FGSTN_LagCompTime::FrameOfTime(RewindMs, FixedStepMS), LastBatchFrame);
	const double AgeSeconds = static_cast<double>(FMath::Max(LastBatchFrame - RewindFrame, 0) * FixedStepMS) / 1000.0;

	// Candidates come from where minions are NOW: a minion that was inside the bounds at the rewind frame cannot have moved further than
	// MaxSpeed x age since. The hash holds feet, so the hurt bound of the largest type is added too
	float LargestBound = 0.f;
	for (const FGSTN_MinionResolvedType& Resolved : ResolvedTypes)
	{
		LargestBound = FMath::Max(LargestBound, Resolved.BoundRadius);
	}
	const double Inflate = FMath::Max(CVarLagCompMaxSpeed.GetValueOnGameThread(), 0.f) * AgeSeconds + LargestBound;
	CandidateScratch.Reset();
	History.QueryHash(Bounds.ExpandBy(Inflate), CandidateScratch);

	const FBox3f Query(FVector3f(Bounds.Min), FVector3f(Bounds.Max));
	int32 Rejected = 0;
	for (const int32 Slot : CandidateScratch)
	{
		if (!Store.IsSlotAlive(Slot) || PendingActivation[Slot])
		{
			continue;
		}
		// The record of the rewind frame (or the latest before it): no record = the minion did not exist then
		const FGSTN_MinionHistoryRecord* Record = History.Find(Slot, RewindFrame);
		if (!Record || !Record->SweptBound.Intersect(Query) || (Record->ReactionFlags & GSTNVictimReactionFlags::Intangible) != 0)
		{
			++Rejected;
			continue;
		}
		FGSTN_MinionRewoundPose& Pose = OutPoses.AddDefaulted_GetRef();
		Pose.EntityId = GSTNEntityKinds::MakeMinionEntityId(static_cast<uint16>(Slot));
		Pose.Generation = Store.Generation[Slot];
		Pose.Frame = Record->Frame;
		Pose.Transform = MakeRoot(FVector(Record->Position), Record->YawDegrees);
		Pose.ReactionFlags = Record->ReactionFlags;
	}

	++BenchTotals.LagCompQueries;
	BenchTotals.LagCompCandidates += CandidateScratch.Num();
	BenchTotals.LagCompRejected += Rejected;
	BenchTotals.LagCompSeconds += FPlatformTime::Seconds() - StartSeconds;
}

void UGSTN_MinionCombatSubsystem::SetExtFlag(int32 Slot, uint8 Bit, bool bOn)
{
	const bool bWasOn = (Store.ExtFlags[Slot] & Bit) != 0;
	if (bWasOn == bOn)
	{
		return;
	}
	Store.ExtFlags[Slot] = bOn ? static_cast<uint8>(Store.ExtFlags[Slot] | Bit) : static_cast<uint8>(Store.ExtFlags[Slot] & ~Bit);
	if (Bit == GSTNMinion::ExtFlag::PlatformActor)
	{
		NumPlatformFlagged += bOn ? 1 : -1;
	}
}

void UGSTN_MinionCombatSubsystem::SetPlatformActor(const FGSTN_MinionHandle& Handle, bool bPlatformActor)
{
	if (Store.IsValid(Handle))
	{
		SetExtFlag(Handle.Index, GSTNMinion::ExtFlag::PlatformActor, bPlatformActor);
	}
}

bool UGSTN_MinionCombatSubsystem::IsPlatformActor(const FGSTN_MinionHandle& Handle) const
{
	return Store.IsValid(Handle) && (Store.ExtFlags[Handle.Index] & GSTNMinion::ExtFlag::PlatformActor) != 0;
}

void UGSTN_MinionCombatSubsystem::SetExemptFromCombatCap(const FGSTN_MinionHandle& Handle, bool bExempt)
{
	if (Store.IsValid(Handle))
	{
		SetExtFlag(Handle.Index, GSTNMinion::ExtFlag::ExemptFromCombatCap, bExempt);
	}
}

void UGSTN_MinionCombatSubsystem::SetMaxLODTier(const FGSTN_MinionHandle& Handle, EGSTN_MinionLODTier MaxTier)
{
	if (!Store.IsValid(Handle))
	{
		return;
	}
	const int32 Slot = Handle.Index;
	const uint8 Ceiling = static_cast<uint8>(FMath::Min<int32>(static_cast<int32>(MaxTier), static_cast<int32>(GSTNMinion::NoLODCeiling)));
	Store.MaxLODTier[Slot] = Ceiling;
	// Pulled back at once, not at the slot next scheduled LOD step (a dormant one is up to DormantInterval frames away): a player is
	// about to stand on it
	if (!PendingActivation[Slot] && Store.LODTier[Slot] > Ceiling)
	{
		const uint8 OldTier = Store.LODTier[Slot];
		Store.LODTier[Slot] = Ceiling;
		Store.FramesInTier[Slot] = 0;
		QueueLODChange(Slot, OldTier, Ceiling);
	}
}

EGSTN_MinionLODTier UGSTN_MinionCombatSubsystem::GetMaxLODTier(const FGSTN_MinionHandle& Handle) const
{
	return Store.IsValid(Handle) ? static_cast<EGSTN_MinionLODTier>(Store.MaxLODTier[Handle.Index]) : EGSTN_MinionLODTier::Dormant;
}

void UGSTN_MinionCombatSubsystem::AddPublishObserver(IGSTN_MinionPublishObserver* Observer)
{
	if (Observer)
	{
		PublishObservers.AddUnique(Observer);
	}
}

void UGSTN_MinionCombatSubsystem::RemovePublishObserver(IGSTN_MinionPublishObserver* Observer)
{
	PublishObservers.Remove(Observer);
}

void UGSTN_MinionCombatSubsystem::NotifyPublishObservers(int32 Frame)
{
	// Inert unbound: two integer tests, no pass over the slots, no allocation
	if (NumPlatformFlagged == 0 || PublishObservers.IsEmpty())
	{
		return;
	}
	// HashSlots is every activated slot in ascending order (built just above in StagePublish), so the walk is in slot order and skips pending slots
	PublishedPoses.Reset();
	for (const int32 Slot : HashSlots)
	{
		if (Store.ExtFlags[Slot] & GSTNMinion::ExtFlag::PlatformActor)
		{
			FGSTN_MinionPublishedPose& Pose = PublishedPoses.AddDefaulted_GetRef();
			Pose.Handle = Store.HandleOfSlot(Slot);
			Pose.Position = Store.Position[Slot];
			Pose.YawDegrees = Store.YawDegrees[Slot];
			Pose.Velocity = MoveColumns.Velocity[Slot];
		}
	}
	if (PublishedPoses.IsEmpty())
	{
		return;
	}
	// Backwards by index: an observer removing itself in the callback must not invalidate the loop
	for (int32 Index = PublishObservers.Num() - 1; Index >= 0; --Index)
	{
		if (PublishObservers.IsValidIndex(Index))
		{
			PublishObservers[Index]->OnMinionPosesPublished(Frame, PublishedPoses);
		}
	}
}

bool UGSTN_MinionCombatSubsystem::SamplePose(const FGSTN_MinionHandle& Handle, int32 Frame, FVector3f& OutPosition, float& OutYaw, bool& bOutClamped) const
{
	if (!Store.IsValid(Handle) || PendingActivation[Handle.Index])
	{
		return false;
	}
	const FGSTN_MinionHistoryRecord* Record = History.Find(Handle.Index, Frame);
	if (!Record)
	{
		// Older than the ring (or before the first record): the oldest one is the nearest the history can say
		Record = History.FindOldest(Handle.Index);
	}
	if (!Record)
	{
		return false;
	}
	OutPosition = Record->Position;
	OutYaw = Record->YawDegrees;
	bOutClamped = Record->Frame != Frame;
	return true;
}

void UGSTN_MinionCombatSubsystem::SetExtByte(const FGSTN_MinionHandle& Handle, uint8 ExtByte)
{
	if (Store.IsValid(Handle))
	{
		Store.ExtByte[Handle.Index] = ExtByte;
	}
}

uint8 UGSTN_MinionCombatSubsystem::GetExtByte(const FGSTN_MinionHandle& Handle) const
{
	return Store.IsValid(Handle) ? Store.ExtByte[Handle.Index] : 0;
}

void UGSTN_MinionCombatSubsystem::QueueLODChange(int32 Slot, uint8 OldTier, uint8 NewTier)
{
	++LODChangeCount;
	if (CVarLODLog.GetValueOnGameThread() != 0)
	{
		const auto TierName = [](uint8 Tier) -> const TCHAR*
		{
			switch (static_cast<EGSTN_MinionLODTier>(Tier))
			{
			case EGSTN_MinionLODTier::Combat: return TEXT("Combat");
			case EGSTN_MinionLODTier::Near: return TEXT("Near");
			case EGSTN_MinionLODTier::Far: return TEXT("Far");
			default: return TEXT("Dormant");
			}
		};
		UE_LOG(LogGSTNMinion, Log, TEXT("MinionLOD id=%d from=%s to=%s frame=%d"), GSTNEntityKinds::MakeMinionEntityId(static_cast<uint16>(Slot)), TierName(OldTier), TierName(NewTier), CurrentBatchFrame);
	}
	// Far or Dormant straight to Combat is a wake (a hit, a launch, a forced state): the brain must think this frame
	if (NewTier == static_cast<uint8>(EGSTN_MinionLODTier::Combat) && OldTier >= static_cast<uint8>(EGSTN_MinionLODTier::Far))
	{
		OnMinionWoken.Broadcast(Store.HandleOfSlot(Slot));
	}
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
