// Minion runtime (M8 G8): the brain, authority only.

#include "Brain/GSTN_MinionBrainSubsystem.h"

#include "AI/Navigation/NavigationTypes.h"
#include "Data/GSTN_MinionDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GSTNMinionCoreModule.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"
#include "World/GSTN_CombatWorldSubsystem.h"

namespace
{
	TAutoConsoleVariable<int32> CVarBrainNative(
		TEXT("gstn.Minion.Brain.Native"), 0,
		TEXT("1 runs the hand-written native chase-and-swing brain instead of the StateTree for every brained minion (spike 6 comparison)."));

	TAutoConsoleVariable<int32> CVarBrainIntervalNear(
		TEXT("gstn.Minion.Brain.IntervalNear"), 4,
		TEXT("Engine frames between two brain ticks of a T1 Near minion (spread by slot)."));

	TAutoConsoleVariable<int32> CVarBrainIntervalFar(
		TEXT("gstn.Minion.Brain.IntervalFar"), 16,
		TEXT("Engine frames between two brain ticks of a T2 Far minion (spread by slot). T3 Dormant minions never think until woken."));

	/** Radius within which another minion counts as a neighbour for separation, and the most neighbours one brain reads */
	constexpr double NeighbourGatherRadius = 200.0;
	constexpr int32 MaxNeighbours = 8;

	/** A navigation path is asked for at most this often per slot (2 Hz), and only when the target is further than this */
	constexpr double PathRefreshSeconds = 0.5;
	constexpr double PathMinDistance = 300.0;
	/** A waypoint closer than this counts as reached (steer at the next one or the target) */
	constexpr double WaypointReachedRadius = 60.0;

	const TCHAR* TierName(EGSTN_MinionLODTier Tier)
	{
		switch (Tier)
		{
		case EGSTN_MinionLODTier::Combat: return TEXT("Combat");
		case EGSTN_MinionLODTier::Near: return TEXT("Near");
		case EGSTN_MinionLODTier::Far: return TEXT("Far");
		default: return TEXT("Dormant");
		}
	}

	FAutoConsoleCommandWithWorld BrainReportCommand(
		TEXT("gstn.Minion.Brain.Report"),
		TEXT("Logs one line: MinionBrain mode=<StateTree|Native> brained=.. ticksLastSec=.. usPerTick=.. maxUsPerTick=.. wakes=.."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const UGSTN_MinionBrainSubsystem* Brain = UGSTN_MinionBrainSubsystem::Get(World))
			{
				Brain->Report();
			}
			else
			{
				UE_LOG(LogGSTNMinion, Log, TEXT("MinionBrain inactive"));
			}
		}));

	FAutoConsoleCommandWithWorld BrainDumpCommand(
		TEXT("gstn.Minion.Brain.Dump"),
		TEXT("Logs one line per brained slot: MinionBrain slot=.. id=.. tier=.. state=<active state or native phase> target=.. ticks=.. lastTick=.. now=.."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UGSTN_MinionBrainSubsystem* Brain = UGSTN_MinionBrainSubsystem::Get(World))
			{
				Brain->Dump();
			}
			else
			{
				UE_LOG(LogGSTNMinion, Log, TEXT("MinionBrain inactive"));
			}
		}));
}

UGSTN_MinionBrainSubsystem* UGSTN_MinionBrainSubsystem::Get(const UWorld* World)
{
	// Null where the minion simulation is not active (a client world): the brain exists only beside the authority batch
	return World && UGSTN_MinionCombatSubsystem::GetActive(World) ? World->GetSubsystem<UGSTN_MinionBrainSubsystem>() : nullptr;
}

bool UGSTN_MinionBrainSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UGSTN_MinionBrainSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UGSTN_MinionCombatSubsystem>();
	Super::Initialize(Collection);
	TypeTrees.SetNum(256);
	TypeResolved.Init(false, 256);
	WindowStartSeconds = FPlatformTime::Seconds();
}

void UGSTN_MinionBrainSubsystem::Deinitialize()
{
	if (UGSTN_MinionCombatSubsystem* Minions = BoundMinions.Get())
	{
		Minions->OnMinionActivated.Remove(ActivatedHandle);
		Minions->OnMinionWoken.Remove(WokenHandle);
	}
	ActivatedHandle.Reset();
	WokenHandle.Reset();
	BoundMinions.Reset();
	Super::Deinitialize();
}

TStatId UGSTN_MinionBrainSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTN_MinionBrainSubsystem, STATGROUP_Tickables);
}

void UGSTN_MinionBrainSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	TickBrains(DeltaTime);
}

void UGSTN_MinionBrainSubsystem::EnsureBound()
{
	UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(GetWorld());
	if (Minions == BoundMinions.Get())
	{
		return;
	}
	if (UGSTN_MinionCombatSubsystem* Previous = BoundMinions.Get())
	{
		Previous->OnMinionActivated.Remove(ActivatedHandle);
		Previous->OnMinionWoken.Remove(WokenHandle);
	}
	BoundMinions = Minions;
	if (Minions)
	{
		ActivatedHandle = Minions->OnMinionActivated.AddUObject(this, &UGSTN_MinionBrainSubsystem::HandleActivated);
		WokenHandle = Minions->OnMinionWoken.AddUObject(this, &UGSTN_MinionBrainSubsystem::HandleWoken);
		EnsureCapacity(Minions->GetStore().Capacity());
	}
}

void UGSTN_MinionBrainSubsystem::EnsureCapacity(int32 Capacity)
{
	if (Instances.Num() >= Capacity)
	{
		return;
	}
	Instances.SetNum(Capacity);
	Trees.SetNum(Capacity);
	BrainGen.SetNumZeroed(Capacity);
	Native.SetNum(Capacity);
	TargetIds.Init(INDEX_NONE, Capacity);
	NextTargetPick.SetNumZeroed(Capacity);
	LastTickClock.SetNumZeroed(Capacity);
	NextPathTime.SetNumZeroed(Capacity);
	Waypoint.SetNumZeroed(Capacity);
	SlotTicks.SetNumZeroed(Capacity);
	SlotLastTick.Init(INDEX_NONE, Capacity);
	Brained.SetNum(Capacity, false);
	Woken.SetNum(Capacity, false);
	TreeStarted.SetNum(Capacity, false);
	HasWaypoint.SetNum(Capacity, false);
	PathPending.SetNum(Capacity, false);
}

void UGSTN_MinionBrainSubsystem::ClearSlot(int32 Slot)
{
	if (Brained.IsValidIndex(Slot) && Brained[Slot])
	{
		--BrainedCount;
	}
	Brained[Slot] = false;
	Woken[Slot] = false;
	TreeStarted[Slot] = false;
	HasWaypoint[Slot] = false;
	PathPending[Slot] = false;
	BrainGen[Slot] = 0;
	Native[Slot] = FGSTN_MinionNativeBrain();
	TargetIds[Slot] = INDEX_NONE;
	Trees[Slot] = nullptr;
	Instances[Slot] = FStateTreeInstanceData();
	SlotTicks[Slot] = 0;
	SlotLastTick[Slot] = INDEX_NONE;
}

bool UGSTN_MinionBrainSubsystem::StartBrain(const FGSTN_MinionHandle& Handle, UObject* Tree)
{
	EnsureBound();
	UGSTN_MinionCombatSubsystem* Minions = BoundMinions.Get();
	if (!Minions || !Minions->IsValidHandle(Handle))
	{
		return false;
	}
	const int32 Slot = Handle.Index;
	if (Brained[Slot])
	{
		ClearSlot(Slot);
	}
	Brained[Slot] = true;
	++BrainedCount;
	BrainGen[Slot] = Handle.Generation;
	Trees[Slot] = Cast<UStateTree>(Tree);
	NextTargetPick[Slot] = 0.0;
	LastTickClock[Slot] = Clock;
	return true;
}

bool UGSTN_MinionBrainSubsystem::StartBrainForDefinition(const FGSTN_MinionHandle& Handle)
{
	EnsureBound();
	UGSTN_MinionCombatSubsystem* Minions = BoundMinions.Get();
	if (!Minions || !Minions->IsValidHandle(Handle))
	{
		return false;
	}
	const int32 TypeIndex = Minions->GetStore().TypeIndex[Handle.Index];
	const UGSTN_MinionDefinition* Definition = Minions->GetDefinition(TypeIndex);
	if (!Definition || Definition->Brain.IsNull())
	{
		return false;
	}
	return StartBrain(Handle, ResolveTypeTree(Definition, TypeIndex));
}

UStateTree* UGSTN_MinionBrainSubsystem::ResolveTypeTree(const UGSTN_MinionDefinition* Definition, int32 TypeIndex)
{
	if (!TypeResolved.IsValidIndex(TypeIndex))
	{
		return nullptr;
	}
	if (!TypeResolved[TypeIndex])
	{
		TypeResolved[TypeIndex] = true;
		TypeTrees[TypeIndex] = Cast<UStateTree>(Definition->Brain.LoadSynchronous());
		if (!TypeTrees[TypeIndex])
		{
			UE_LOG(LogGSTNMinion, Warning, TEXT("MinionBrain: the brain %s of a minion type is not a StateTree; its minions run the native brain"), *Definition->Brain.ToString());
		}
	}
	return TypeTrees[TypeIndex];
}

bool UGSTN_MinionBrainSubsystem::StopBrain(const FGSTN_MinionHandle& Handle)
{
	if (!HasBrain(Handle))
	{
		return false;
	}
	ClearSlot(Handle.Index);
	return true;
}

bool UGSTN_MinionBrainSubsystem::HasBrain(const FGSTN_MinionHandle& Handle) const
{
	return Brained.IsValidIndex(Handle.Index) && Brained[Handle.Index] && BrainGen[Handle.Index] == Handle.Generation;
}

void UGSTN_MinionBrainSubsystem::HandleActivated(const FGSTN_MinionHandle& Handle)
{
	StartBrainForDefinition(Handle);
}

void UGSTN_MinionBrainSubsystem::HandleWoken(const FGSTN_MinionHandle& Handle)
{
	if (HasBrain(Handle))
	{
		Woken[Handle.Index] = true;
		++Stats.Wakes;
	}
}

bool UGSTN_MinionBrainSubsystem::IsSlotUsingStateTree(int32 Slot) const
{
	return Trees.IsValidIndex(Slot) && Trees[Slot] != nullptr && CVarBrainNative.GetValueOnGameThread() == 0;
}

void UGSTN_MinionBrainSubsystem::ResetStats()
{
	Stats = FGSTN_MinionBrainStats();
	WindowTicks = 0;
	WindowStartSeconds = FPlatformTime::Seconds();
}

void UGSTN_MinionBrainSubsystem::BuildTargets()
{
	bTargetsBuilt = true;
	Targets.Reset();
	if (TargetProvider)
	{
		TargetProvider(Targets);
		return;
	}
	const UWorld* World = GetWorld();
	const UGSTN_CombatWorldSubsystem* Combat = World ? World->GetSubsystem<UGSTN_CombatWorldSubsystem>() : nullptr;
	if (!Combat)
	{
		return;
	}
	TArray<int32> Ids;
	Combat->GetRegisteredEntityIds(Ids);
	Ids.Sort();
	for (const int32 Id : Ids)
	{
		if (const AActor* Actor = Combat->FindOwnerActor(Id))
		{
			FGSTN_MinionBrainTarget& Target = Targets.AddDefaulted_GetRef();
			Target.Id = Id;
			Target.Position = Actor->GetActorLocation();
		}
	}
}

void UGSTN_MinionBrainSubsystem::ResolveTarget(int32 Slot, FGSTN_MinionBrainContext& Context)
{
	if (!bTargetsBuilt)
	{
		BuildTargets();
	}
	const FGSTN_MinionBrainTarget* Chosen = nullptr;
	if (TargetIds[Slot] != INDEX_NONE && Clock < NextTargetPick[Slot])
	{
		for (const FGSTN_MinionBrainTarget& Target : Targets)
		{
			if (Target.Id == TargetIds[Slot])
			{
				Chosen = &Target;
				break;
			}
		}
	}
	if (!Chosen)
	{
		// Re-pick the nearest on the ground plane: at most twice a second per slot, or at once when the old target is gone
		double Best = TNumericLimits<double>::Max();
		for (const FGSTN_MinionBrainTarget& Target : Targets)
		{
			const double DistSquared = FVector2D::DistSquared(Context.Position2D(), FVector2D(Target.Position.X, Target.Position.Y));
			if (DistSquared < Best)
			{
				Best = DistSquared;
				Chosen = &Target;
			}
		}
		TargetIds[Slot] = Chosen ? Chosen->Id : INDEX_NONE;
		NextTargetPick[Slot] = Clock + GSTNMinion::BrainTargetRefreshSeconds;
	}
	if (Chosen)
	{
		Context.bHasTarget = true;
		Context.TargetId = Chosen->Id;
		Context.TargetPosition = Chosen->Position;
		Context.MovePoint = Chosen->Position;
		Context.TargetDistance = static_cast<float>(FVector2D::Distance(Context.Position2D(), FVector2D(Chosen->Position.X, Chosen->Position.Y)));
	}
}

void UGSTN_MinionBrainSubsystem::GatherNeighbours(const UGSTN_MinionCombatSubsystem& Minions, int32 Slot, const FVector2D& Position)
{
	// PERF-DEBT: a linear walk of the store per brain tick (<= capacity entries). The batch's position hash would make it constant
	NeighbourScratch.Reset();
	const FGSTN_MinionStore& Store = Minions.GetStore();
	const double RadiusSquared = NeighbourGatherRadius * NeighbourGatherRadius;
	for (int32 Other = Store.NextAliveSlot(0); Other != INDEX_NONE && NeighbourScratch.Num() < MaxNeighbours; Other = Store.NextAliveSlot(Other + 1))
	{
		if (Other == Slot || !Minions.IsSlotActive(Other))
		{
			continue;
		}
		const FVector2D OtherPosition(Store.Position[Other].X, Store.Position[Other].Y);
		if (FVector2D::DistSquared(Position, OtherPosition) < RadiusSquared)
		{
			NeighbourScratch.Add(OtherPosition);
		}
	}
}

void UGSTN_MinionBrainSubsystem::UpdateNavigation(int32 Slot, FGSTN_MinionBrainContext& Context)
{
	if (!bNavChecked)
	{
		bNavChecked = true;
		const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		bNavReady = NavSys && NavSys->GetDefaultNavDataInstance() != nullptr;
	}
	// The harness map has no navmesh: this whole function is a no-op there and the minion seeks the target directly
	if (!bNavReady || !Context.bHasTarget)
	{
		return;
	}
	if (HasWaypoint[Slot])
	{
		if (FVector::Dist2D(Context.Position, Waypoint[Slot]) > WaypointReachedRadius)
		{
			Context.MovePoint = Waypoint[Slot];
		}
		else
		{
			HasWaypoint[Slot] = false;
		}
	}
	if (PathPending[Slot] || Clock < NextPathTime[Slot] || Context.TargetDistance < PathMinDistance || Context.Tier != EGSTN_MinionLODTier::Combat)
	{
		return;
	}
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ANavigationData* NavData = NavSys ? NavSys->GetDefaultNavDataInstance() : nullptr;
	if (!NavData)
	{
		return;
	}
	NextPathTime[Slot] = Clock + PathRefreshSeconds;
	PathPending[Slot] = true;
	const uint16 Generation = BrainGen[Slot];
	const FPathFindingQuery Query(this, *NavData, Context.Position, Context.TargetPosition);
	NavSys->FindPathAsync(FNavAgentProperties::DefaultProperties, Query,
		FNavPathQueryDelegate::CreateWeakLambda(this, [this, Slot, Generation](uint32, ENavigationQueryResult::Type Result, FNavPathSharedPtr Path)
		{
			if (!Brained.IsValidIndex(Slot) || BrainGen[Slot] != Generation)
			{
				return;
			}
			PathPending[Slot] = false;
			HasWaypoint[Slot] = false;
			if (Result == ENavigationQueryResult::Success && Path.IsValid() && Path->GetPathPoints().Num() > 1)
			{
				// Point 0 is the start: steer at the next corner
				Waypoint[Slot] = Path->GetPathPoints()[1].Location;
				HasWaypoint[Slot] = true;
			}
		}));
}

bool UGSTN_MinionBrainSubsystem::CollectExternalData(const FStateTreeExecutionContext& Context, const UStateTree* StateTree, TArrayView<const FStateTreeExternalDataDesc> Descs, TArrayView<FStateTreeDataView> OutViews)
{
	for (int32 Index = 0; Index < Descs.Num(); ++Index)
	{
		if (Descs[Index].Struct == FGSTN_MinionBrainContext::StaticStruct())
		{
			OutViews[Index] = FStateTreeDataView(FGSTN_MinionBrainContext::StaticStruct(), reinterpret_cast<uint8*>(ActiveContext));
		}
	}
	return true;
}

void UGSTN_MinionBrainSubsystem::RunSlot(UGSTN_MinionCombatSubsystem& Minions, int32 Slot, float DeltaSeconds)
{
	const FGSTN_MinionStore& Store = Minions.GetStore();
	FGSTN_MinionBrainContext& Context = ContextScratch;
	Context = FGSTN_MinionBrainContext();
	Context.Handle = Store.HandleOfSlot(Slot);
	Context.Slot = Slot;
	Context.Frame = FMath::Max(Minions.GetLastBatchFrame(), 0);
	Context.Position = FVector(Store.Position[Slot]);
	Context.YawDegrees = Store.YawDegrees[Slot];
	Context.Tier = static_cast<EGSTN_MinionLODTier>(Store.LODTier[Slot]);
	if (const UGSTN_MinionDefinition* Definition = Minions.GetDefinition(Store.TypeIndex[Slot]))
	{
		Context.WalkSpeed = Definition->Stats.WalkSpeed;
		if (!Definition->Moves.IsEmpty())
		{
			Context.DefaultCommand = Definition->Moves[0].Command;
		}
	}
	ResolveTarget(Slot, Context);
	if (Context.Tier <= EGSTN_MinionLODTier::Near)
	{
		GatherNeighbours(Minions, Slot, Context.Position2D());
		Context.Neighbours = NeighbourScratch;
	}
	UpdateNavigation(Slot, Context);

	bool bRanTree = false;
	if (IsSlotUsingStateTree(Slot))
	{
		UStateTree* Tree = Trees[Slot];
		ActiveContext = &Context;
		FStateTreeExecutionContext Exec(*this, *Tree, Instances[Slot], FOnCollectStateTreeExternalData::CreateUObject(this, &UGSTN_MinionBrainSubsystem::CollectExternalData));
		if (Exec.IsValid())
		{
			bRanTree = true;
			const EStateTreeRunStatus Status = TreeStarted[Slot] ? Exec.Tick(DeltaSeconds) : Exec.Start();
			// A tree that ends (or could not select a state) is started again on the next tick
			TreeStarted[Slot] = Status == EStateTreeRunStatus::Running;
		}
		ActiveContext = nullptr;
	}
	if (!bRanTree)
	{
		Native[Slot].Tick(Context);
	}
	// One commit per ticked slot; the subsystem stamps it with the server frame
	Minions.SetIntent(Context.Handle, Context.PendingIntent);
}

void UGSTN_MinionBrainSubsystem::TickBrains(float DeltaSeconds)
{
	EnsureBound();
	UGSTN_MinionCombatSubsystem* Minions = BoundMinions.Get();
	if (!Minions)
	{
		return;
	}
	Clock += DeltaSeconds;
	++TickCounter;
	Budget.NearInterval = CVarBrainIntervalNear.GetValueOnGameThread();
	Budget.FarInterval = CVarBrainIntervalFar.GetValueOnGameThread();
	bTargetsBuilt = false;
	bNavChecked = false;
	Stats.Brained = BrainedCount;
	Stats.StateTreeSlots = 0;

	const FGSTN_MinionStore& Store = Minions->GetStore();
	DropScratch.Reset();
	for (TConstSetBitIterator<> It(Brained); It; ++It)
	{
		const int32 Slot = It.GetIndex();
		// The minion died or its slot was recycled: the brain goes with it
		if (!Store.IsSlotAlive(Slot) || Store.Generation[Slot] != BrainGen[Slot])
		{
			DropScratch.Add(Slot);
			continue;
		}
		if (!Minions->IsSlotActive(Slot))
		{
			continue;
		}
		Stats.StateTreeSlots += IsSlotUsingStateTree(Slot) ? 1 : 0;
		const EGSTN_MinionLODTier Tier = static_cast<EGSTN_MinionLODTier>(Store.LODTier[Slot]);
		const bool bWoken = Woken[Slot];
		if (!Budget.ShouldTick(Tier, Slot, TickCounter, bWoken))
		{
			continue;
		}
		Woken[Slot] = false;

		const double Start = FPlatformTime::Seconds();
		RunSlot(*Minions, Slot, static_cast<float>(Clock - LastTickClock[Slot]));
		const double Elapsed = FPlatformTime::Seconds() - Start;
		LastTickClock[Slot] = Clock;
		++SlotTicks[Slot];
		SlotLastTick[Slot] = TickCounter;
		++Stats.TotalTicks;
		++WindowTicks;
		Stats.TotalSeconds += Elapsed;
		Stats.MaxTickSeconds = FMath::Max(Stats.MaxTickSeconds, Elapsed);
	}
	for (const int32 Slot : DropScratch)
	{
		ClearSlot(Slot);
	}

	const double Now = FPlatformTime::Seconds();
	if (Now - WindowStartSeconds >= 1.0)
	{
		Stats.TicksLastSec = WindowTicks;
		WindowTicks = 0;
		WindowStartSeconds = Now;
	}
}

FString UGSTN_MinionBrainSubsystem::GetSlotStateName(int32 Slot)
{
	if (!Brained.IsValidIndex(Slot) || !Brained[Slot])
	{
		return TEXT("None");
	}
	if (IsSlotUsingStateTree(Slot) && TreeStarted[Slot])
	{
		ActiveContext = &ContextScratch;
		FStateTreeExecutionContext Exec(*this, *Trees[Slot], Instances[Slot], FOnCollectStateTreeExternalData::CreateUObject(this, &UGSTN_MinionBrainSubsystem::CollectExternalData));
		const FString Full = Exec.IsValid() ? Exec.GetActiveStateName() : FString(TEXT("Invalid"));
		ActiveContext = nullptr;
		// GetActiveStateName is one indented line per active state (root first): keep the leaf so a dump stays one parsable line
		TArray<FString> Lines;
		Full.ParseIntoArrayLines(Lines);
		FString Leaf = Lines.IsEmpty() ? FString(TEXT("None")) : Lines.Last().TrimStartAndEnd();
		while (Leaf.StartsWith(TEXT("-")))
		{
			Leaf.RightChopInline(1);
		}
		Leaf.TrimStartInline();
		return Leaf.Replace(TEXT(" "), TEXT("_"));
	}
	return Native[Slot].bStarted && !IsSlotUsingStateTree(Slot) ? FString(FGSTN_MinionNativeBrain::PhaseName(Native[Slot].Phase)) : FString(TEXT("NotStarted"));
}

void UGSTN_MinionBrainSubsystem::Report() const
{
	const bool bTree = Stats.StateTreeSlots > 0 || (Stats.Brained == 0 && CVarBrainNative.GetValueOnGameThread() == 0);
	const double UsPerTick = Stats.TotalTicks > 0 ? Stats.TotalSeconds * 1.0e6 / static_cast<double>(Stats.TotalTicks) : 0.0;
	UE_LOG(LogGSTNMinion, Log, TEXT("MinionBrain mode=%s brained=%d ticksLastSec=%d usPerTick=%.2f maxUsPerTick=%.2f wakes=%d"),
		bTree ? TEXT("StateTree") : TEXT("Native"), Stats.Brained, Stats.TicksLastSec, UsPerTick, Stats.MaxTickSeconds * 1.0e6, Stats.Wakes);
}

void UGSTN_MinionBrainSubsystem::Dump()
{
	const UGSTN_MinionCombatSubsystem* Minions = BoundMinions.Get();
	if (!Minions)
	{
		UE_LOG(LogGSTNMinion, Log, TEXT("MinionBrain inactive"));
		return;
	}
	const FGSTN_MinionStore& Store = Minions->GetStore();
	for (TConstSetBitIterator<> It(Brained); It; ++It)
	{
		const int32 Slot = It.GetIndex();
		if (!Store.IsSlotAlive(Slot))
		{
			continue;
		}
		// Diagnostics after now=: the action state, the move playing, the intent (stamp, speed, command) and the nearest pawn on the
		// ground plane, i.e. what the LOD stage reads to keep a minion in Combat
		double Nearest = -1.0;
		if (const UWorld* World = GetWorld())
		{
			for (FConstPlayerControllerIterator PC = World->GetPlayerControllerIterator(); PC; ++PC)
			{
				const APawn* Pawn = PC->Get() ? PC->Get()->GetPawn() : nullptr;
				if (Pawn)
				{
					const double D = FVector2D::Distance(FVector2D(Pawn->GetActorLocation()), FVector2D(Store.Position[Slot].X, Store.Position[Slot].Y));
					Nearest = Nearest < 0.0 ? D : FMath::Min(Nearest, D);
				}
			}
		}
		const FGSTN_MinionIntent& Intent = Store.Intent[Slot];
		UE_LOG(LogGSTNMinion, Log, TEXT("MinionBrain slot=%d id=%d tier=%s state=%s target=%d ticks=%d lastTick=%d now=%d action=%d move=%d stamp=%d speed=%.0f cmd=%s nearestPawn=%.0f"),
			Slot, GSTNEntityKinds::MakeMinionEntityId(static_cast<uint16>(Slot)), TierName(static_cast<EGSTN_MinionLODTier>(Store.LODTier[Slot])),
			*GetSlotStateName(Slot), TargetIds[Slot], SlotTicks[Slot], SlotLastTick[Slot], TickCounter,
			Store.ActionState[Slot], Store.MoveId[Slot], Intent.StampFrame, Intent.DesiredSpeed, *Intent.Command.ToString(), Nearest);
	}
}
