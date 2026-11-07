// Minion runtime (authority batch, M8 G3; spike 3 scenario, course and robustness counters G4).

#include "Debug/GSTN_MinionBench.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/GSTN_MinionDefinition.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GSTNMinionCoreModule.h"
#include "HAL/IConsoleManager.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Spawn/GSTN_MinionSpawner.h"

namespace
{
	TAutoConsoleVariable<FString> CVarBenchDefinition(
		TEXT("gstn.Minion.Bench.Definition"), TEXT(""),
		TEXT("Object path of the UGSTN_MinionDefinition the bench spawns (empty = a transient definition built in code)."));

	// Timed headless runs (gate of G3/G11): ExecCmds cannot wait, so the bench reports, and optionally quits, by itself
	TAutoConsoleVariable<float> CVarBenchAutoReportSeconds(
		TEXT("gstn.Minion.Bench.AutoReportSeconds"), 0.f,
		TEXT("Seconds after a bench spawn at which the bench logs its report once (0 = off)."));
	TAutoConsoleVariable<bool> CVarBenchAutoQuit(
		TEXT("gstn.Minion.Bench.AutoQuit"), false,
		TEXT("Quit the process right after the automatic report (headless bench runs)."));

	// Massacre (spike 3)
	TAutoConsoleVariable<int32> CVarBenchLaunchEveryFrames(
		TEXT("gstn.Minion.Bench.LaunchEveryFrames"), 30,
		TEXT("Massacre: fixed frames between two launch rounds."));
	TAutoConsoleVariable<float> CVarBenchLaunchFraction(
		TEXT("gstn.Minion.Bench.LaunchFraction"), 0.25f,
		TEXT("Massacre: chance of each minion to be launched in a round."));
	TAutoConsoleVariable<float> CVarBenchLaunchSpeed(
		TEXT("gstn.Minion.Bench.LaunchSpeed"), 3000.f,
		TEXT("Massacre: highest launch speed, cm/s (a launch draws 30 to 100 percent of it)."));
	TAutoConsoleVariable<int32> CVarBenchSeed(
		TEXT("gstn.Minion.Bench.Seed"), 1234,
		TEXT("Massacre: seed of the launch stream, reset at every bench spawn."));

	/** Robustness thresholds of spike 3 (combat-m8-minions.md 6) */
	constexpr int32 StuckFrameLimit = 125;
	constexpr double StuckDistance = 1.0;
	constexpr double FellBelowCourseDistance = 1000.0;
	/** Half-angle of the upward cone a launch is drawn in, degrees */
	constexpr float LaunchConeDegrees = 35.f;
	/** Share of the launches that are push backs */
	constexpr float PushBackShare = 0.3f;

	enum ESlotFlag : uint8
	{
		StuckCounted = 1 << 0,
		FellCounted = 1 << 1,
		BlockedCounted = 1 << 2,
		PenetrationCounted = 1 << 3
	};

	/** Trapped events logged at Warning with their details; the rest at Verbose */
	constexpr int32 TrappedEventsLogged = 3;

	/** Nearest player pawn location to From on the ground plane; false when there is no player pawn */
	bool FindNearestPlayer(const UWorld* World, const FVector& From, FVector& OutLocation)
	{
		double Best = TNumericLimits<double>::Max();
		bool bFound = false;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* Controller = It->Get();
			const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
			if (!Pawn)
			{
				continue;
			}
			const FVector Location = Pawn->GetActorLocation();
			const double DistSquared = FVector::DistSquared2D(From, Location);
			if (DistSquared < Best)
			{
				Best = DistSquared;
				OutLocation = Location;
				bFound = true;
			}
		}
		return bFound;
	}

	FAutoConsoleCommandWithWorldAndArgs BenchSpawnCommand(
		TEXT("gstn.Minion.Bench.Spawn"),
		TEXT("gstn.Minion.Bench.Spawn <N> <Idle|Chase|Massacre>: spawns N bench minions in a ring around the first player (or the origin)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGSTN_MinionBenchSubsystem* Bench = UGSTN_MinionBenchSubsystem::Get(World);
			if (!Bench)
			{
				UE_LOG(LogGSTNMinion, Warning, TEXT("gstn.Minion.Bench.Spawn: no bench in this world"));
				return;
			}
			const int32 Count = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 40;
			EGSTN_MinionBenchScenario Scenario = EGSTN_MinionBenchScenario::Idle;
			if (Args.Num() > 1 && Args[1].Equals(TEXT("Chase"), ESearchCase::IgnoreCase))
			{
				Scenario = EGSTN_MinionBenchScenario::Chase;
			}
			else if (Args.Num() > 1 && Args[1].Equals(TEXT("Massacre"), ESearchCase::IgnoreCase))
			{
				Scenario = EGSTN_MinionBenchScenario::Massacre;
			}
			Bench->SpawnBench(Count, Scenario);
		}));

	FAutoConsoleCommandWithWorld BenchClearCommand(
		TEXT("gstn.Minion.Bench.Clear"),
		TEXT("Despawns every bench minion."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UGSTN_MinionBenchSubsystem* Bench = UGSTN_MinionBenchSubsystem::Get(World))
			{
				Bench->Clear();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs BenchCourseCommand(
		TEXT("gstn.Minion.Bench.Course"),
		TEXT("gstn.Minion.Bench.Course <1|0>: builds (1) or removes (0) the spike 3 test course around the bench centre."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UGSTN_MinionBenchSubsystem* Bench = UGSTN_MinionBenchSubsystem::Get(World))
			{
				Bench->SetCourse(Args.Num() == 0 || FCString::Atoi(*Args[0]) != 0);
			}
		}));

	FAutoConsoleCommandWithWorld BenchReportCommand(
		TEXT("gstn.Minion.Bench.Report"),
		TEXT("Logs one line: MinionBench alive=.. frames=.. avgBatchUs=.. maxBatchUs=.. moveUsPerMinionFrame=.. fell=.. stuck=.. blocked=.. tunnelled=.."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const UGSTN_MinionBenchSubsystem* Bench = UGSTN_MinionBenchSubsystem::Get(World))
			{
				Bench->Report();
			}
		}));
}

UGSTN_MinionBenchSubsystem* UGSTN_MinionBenchSubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UGSTN_MinionBenchSubsystem>() : nullptr;
}

bool UGSTN_MinionBenchSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UGSTN_MinionBenchSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTN_MinionBenchSubsystem, STATGROUP_Tickables);
}

void UGSTN_MinionBenchSubsystem::Deinitialize()
{
	if (UGSTN_MinionCombatSubsystem* Minions = HookedMinions.Get())
	{
		Minions->OnMovementStepped.Remove(MovementHookHandle);
	}
	MovementHookHandle.Reset();
	HookedMinions.Reset();
	Super::Deinitialize();
}

UGSTN_MinionDefinition* UGSTN_MinionBenchSubsystem::ResolveDefinition()
{
	const FString Path = CVarBenchDefinition.GetValueOnGameThread();
	if (!Path.IsEmpty())
	{
		if (UGSTN_MinionDefinition* Loaded = LoadObject<UGSTN_MinionDefinition>(nullptr, *Path))
		{
			return Loaded;
		}
		UE_LOG(LogGSTNMinion, Warning, TEXT("Bench: definition %s not found, using the transient one"), *Path);
	}
	if (!TransientDefinition)
	{
		// A grunt in numbers only: no hurtbox set (cannot be hit), no moves, default stats. Enough to measure the batch
		TransientDefinition = NewObject<UGSTN_MinionDefinition>(this, TEXT("GSTN_MinionBenchDefinition"), RF_Transient);
		TransientDefinition->TypeTag = FGameplayTag::RequestGameplayTag(TEXT("Minion.Type.Grunt"), false);
		TransientDefinition->Stats.MaxHealth = 40.f;
	}
	return TransientDefinition;
}

FVector UGSTN_MinionBenchSubsystem::ResolveCenter()
{
	if (!bCenterResolved)
	{
		Center = FVector::ZeroVector;
		FindNearestPlayer(GetWorld(), FVector::ZeroVector, Center);
		bCenterResolved = true;
	}
	return Center;
}

int32 UGSTN_MinionBenchSubsystem::SpawnBench(int32 Count, EGSTN_MinionBenchScenario InScenario)
{
	UWorld* World = GetWorld();
	UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(World);
	if (!Minions)
	{
		UE_LOG(LogGSTNMinion, Warning, TEXT("Bench: the minion subsystem is not active here (client world?)"));
		return 0;
	}
	UGSTN_MinionDefinition* Definition = ResolveDefinition();
	Scenario = InScenario;
	BenchSeconds = 0.f;

	// With a course the ring uses the course's centre (just above its floor); otherwise the nearest player, as in G3
	const bool bOnCourse = !CourseActors.IsEmpty();
	if (!bOnCourse)
	{
		bCenterResolved = false;
	}
	const FVector RingCenter = ResolveCenter() + (bOnCourse ? FVector(0.0, 0.0, 5.0) : FVector::ZeroVector);
	Count = FMath::Max(Count, 0);
	// Ring: about 150 cm between neighbours, at least 600 cm out
	RingRadius = FMath::Max(600.0, Count * 150.0 / UE_TWO_PI);

	// Spawns are placed by a capsule sweep down from 600 cm above the ring point, so none starts inside geometry (the ring crosses
	// the course's stairs); a point whose sweep starts in penetration is skipped
	const FGSTN_MinionStats& Stats = Definition->Stats;
	const double HalfHeight = FMath::Max(Stats.CapsuleHalfHeight, Stats.CapsuleRadius);
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Stats.CapsuleRadius, static_cast<float>(HalfHeight));
	FCollisionObjectQueryParams ObjectQuery;
	for (const TEnumAsByte<ECollisionChannel> Channel : Minions->GetMovementSettings().BlockingObjectTypes)
	{
		ObjectQuery.AddObjectTypesToQuery(Channel);
	}
	const double DropHeight = 600.0;

	int32 Queued = 0;
	int32 Skipped = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const double Angle = UE_TWO_PI * Index / FMath::Max(Count, 1);
		FVector Location = RingCenter + FVector(FMath::Cos(Angle) * RingRadius, FMath::Sin(Angle) * RingRadius, 0.0);
		const FVector SweepStart = Location + FVector(0.0, 0.0, DropHeight + HalfHeight);
		const FVector SweepEnd = Location + FVector(0.0, 0.0, HalfHeight - DropHeight);
		FHitResult Hit;
		if (World->SweepSingleByObjectType(Hit, SweepStart, SweepEnd, FQuat::Identity, ObjectQuery, Capsule, FCollisionQueryParams(SCENE_QUERY_STAT(GSTN_MinionBenchSpawn), false)))
		{
			if (Hit.bStartPenetrating)
			{
				++Skipped;
				continue;
			}
			// Feet 1 cm above the floor found (the backend's skin)
			Location = Hit.Location - FVector(0.0, 0.0, HalfHeight - 1.0);
		}
		const FRotator Facing = (RingCenter - Location).Rotation();
		const FGSTN_MinionHandle Handle = UGSTN_MinionSpawner::Spawn(World, Definition, FTransform(FRotator(0.0, Facing.Yaw, 0.0), Location), FGSTN_MinionSpawnParams());
		if (!Handle.IsNull())
		{
			Handles.Add(Handle);
			++Queued;
		}
	}
	Minions->ResetBenchTotals();
	ResetCounters(Minions->GetStore().Capacity());
	LaunchStream.Initialize(CVarBenchSeed.GetValueOnGameThread());
	BindMovementHook(Minions);

	const TCHAR* ScenarioName = Scenario == EGSTN_MinionBenchScenario::Massacre ? TEXT("Massacre")
		: (Scenario == EGSTN_MinionBenchScenario::Chase ? TEXT("Chase") : TEXT("Idle"));
	UE_LOG(LogGSTNMinion, Log, TEXT("Bench: %d minions queued (%s), ring radius %.0f%s, %d ring points skipped (inside geometry)"), Queued,
		ScenarioName, RingRadius, bOnCourse ? TEXT(", on the course") : TEXT(""), Skipped);
	return Queued;
}

void UGSTN_MinionBenchSubsystem::Clear()
{
	for (const FGSTN_MinionHandle& Handle : Handles)
	{
		UGSTN_MinionSpawner::Despawn(GetWorld(), Handle);
	}
	UE_LOG(LogGSTNMinion, Log, TEXT("Bench: %d minions cleared"), Handles.Num());
	Handles.Reset();
}

void UGSTN_MinionBenchSubsystem::Report() const
{
	const UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(GetWorld());
	if (!Minions)
	{
		UE_LOG(LogGSTNMinion, Log, TEXT("MinionBench inactive"));
		return;
	}
	const FGSTN_MinionBenchTotals& Totals = Minions->GetBenchTotals();
	const double AvgUs = Totals.Batches > 0 ? Totals.TotalBatchSeconds * 1.0e6 / Totals.Batches : 0.0;
	const double MoveUs = Totals.MovementMinionFrames > 0 ? Totals.MovementSeconds * 1.0e6 / static_cast<double>(Totals.MovementMinionFrames) : 0.0;
	UE_LOG(LogGSTNMinion, Log, TEXT("MinionBench alive=%d frames=%d avgBatchUs=%.1f maxBatchUs=%.1f moveUsPerMinionFrame=%.2f fell=%d stuck=%d blocked=%d tunnelled=%d"),
		Minions->GetLastBatchStats().Alive, Totals.Batches, AvgUs, Totals.MaxBatchSeconds * 1.0e6, MoveUs,
		Totals.FellOut + FellBelowCourse, Stuck, Blocked, Tunnelled);
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Robustness counters and Massacre launches (end of stage 4, once per fixed frame)

void UGSTN_MinionBenchSubsystem::ResetCounters(int32 Capacity)
{
	FellBelowCourse = 0;
	Stuck = 0;
	Blocked = 0;
	Tunnelled = 0;
	SeenGeneration.Init(0, Capacity);
	StuckAnchor.Init(FVector3f::ZeroVector, Capacity);
	StuckFrames.Init(0, Capacity);
	SlotFlags.Init(0, Capacity);
}

void UGSTN_MinionBenchSubsystem::CountTrapped(int32 Slot, const FVector& Feet, const TCHAR* ModeName, const TCHAR* Reason, int32 Frame)
{
	++Stuck;
	if (Stuck <= TrappedEventsLogged)
	{
		UE_LOG(LogGSTNMinion, Warning, TEXT("Bench: TRAPPED #%d slot %d at %s mode %s: %s (frame %d)"), Stuck, Slot, *Feet.ToCompactString(), ModeName, Reason, Frame);
	}
	else
	{
		UE_LOG(LogGSTNMinion, Verbose, TEXT("Bench: trapped slot %d at %s mode %s: %s (frame %d)"), Slot, *Feet.ToCompactString(), ModeName, Reason, Frame);
	}
}

void UGSTN_MinionBenchSubsystem::BindMovementHook(UGSTN_MinionCombatSubsystem* Minions)
{
	if (HookedMinions.Get() == Minions && MovementHookHandle.IsValid())
	{
		return;
	}
	if (UGSTN_MinionCombatSubsystem* Previous = HookedMinions.Get())
	{
		Previous->OnMovementStepped.Remove(MovementHookHandle);
	}
	HookedMinions = Minions;
	MovementHookHandle = Minions->OnMovementStepped.AddUObject(this, &UGSTN_MinionBenchSubsystem::HandleMovementStepped);
}

void UGSTN_MinionBenchSubsystem::HandleMovementStepped(int32 Frame, TConstArrayView<int32> Slots, TConstArrayView<FVector3f> PreviousPositionBySlot)
{
	UGSTN_MinionCombatSubsystem* Minions = HookedMinions.Get();
	if (!Minions)
	{
		return;
	}
	const FGSTN_MinionStore& Store = Minions->GetStore();
	const FGSTN_MinionMovementColumns& Columns = Minions->GetMovementColumns();
	if (SeenGeneration.Num() != Store.Capacity())
	{
		ResetCounters(Store.Capacity());
	}
	const bool bHasCourse = !CourseBoxes.IsEmpty();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FCollisionObjectQueryParams ObjectQuery;
	for (const TEnumAsByte<ECollisionChannel> Channel : Minions->GetMovementSettings().BlockingObjectTypes)
	{
		ObjectQuery.AddObjectTypesToQuery(Channel);
	}

	for (const int32 Slot : Slots)
	{
		const FVector Now(Store.Position[Slot]);
		if (SeenGeneration[Slot] != Store.Generation[Slot])
		{
			// A new occupant of the slot: its counters start over
			SeenGeneration[Slot] = Store.Generation[Slot];
			StuckAnchor[Slot] = FVector3f(Now);
			StuckFrames[Slot] = 0;
			SlotFlags[Slot] = 0;
			continue;
		}
		const FVector Previous(PreviousPositionBySlot[Slot]);

		// No displacement (within 1 cm for 125 frames). Airborne or in a knockback it is "trapped" (a movement failure: gravity or
		// the launch should move it); walking into a wall with intent it is only "blocked" (expected for a straight-line chaser
		// without navigation)
		const FGSTN_MinionIntent& Intent = Store.Intent[Slot];
		const uint8 Mode = Columns.KnockbackMode[Slot];
		const bool bGrounded = Columns.bGrounded[Slot] != 0;
		const bool bAirOrKnockback = Mode != static_cast<uint8>(EGSTN_MinionKnockbackMode::None) || !bGrounded;
		const bool bWantsToMove = !Intent.IsStale(Frame) && Intent.DesiredSpeed > 0.f && !Intent.MoveDir.IsNearlyZero();
		const TCHAR* ModeName = Mode == static_cast<uint8>(EGSTN_MinionKnockbackMode::Launched) ? TEXT("Launched")
			: (Mode == static_cast<uint8>(EGSTN_MinionKnockbackMode::PushedBack) ? TEXT("PushedBack") : (bGrounded ? TEXT("Walking") : TEXT("Falling")));
		if (!(bWantsToMove || bAirOrKnockback) || FVector::Dist(Now, FVector(StuckAnchor[Slot])) >= StuckDistance)
		{
			StuckAnchor[Slot] = FVector3f(Now);
			StuckFrames[Slot] = 0;
			SlotFlags[Slot] &= static_cast<uint8>(~(StuckCounted | BlockedCounted));
		}
		else if (++StuckFrames[Slot] >= StuckFrameLimit)
		{
			if (bAirOrKnockback && !(SlotFlags[Slot] & StuckCounted))
			{
				SlotFlags[Slot] |= StuckCounted;
				CountTrapped(Slot, Now, ModeName, TEXT("no displacement for 2 s"), Frame);
			}
			else if (!bAirOrKnockback && !(SlotFlags[Slot] & BlockedCounted))
			{
				SlotFlags[Slot] |= BlockedCounted;
				++Blocked;
				UE_LOG(LogGSTNMinion, Verbose, TEXT("Bench: minion slot %d blocked at %s (frame %d)"), Slot, *Now.ToCompactString(), Frame);
			}
		}

		// Trapped: the capsule (shrunk by 0.5 cm, so resting contacts do not count) overlaps blocking geometry after its step.
		// Bench only (outside the stage 4 timing); assumes gravity Z like the rest of the bench
		const UGSTN_MinionDefinition* SlotDefinition = Minions->GetDefinition(Store.TypeIndex[Slot]);
		if (SlotDefinition)
		{
			const float Radius = SlotDefinition->Stats.CapsuleRadius;
			const float HalfHeight = FMath::Max(SlotDefinition->Stats.CapsuleHalfHeight, Radius);
			const bool bInside = World->OverlapAnyTestByObjectType(Now + FVector(0.0, 0.0, HalfHeight), FQuat::Identity, ObjectQuery,
				FCollisionShape::MakeCapsule(Radius - 0.5f, HalfHeight - 0.5f), FCollisionQueryParams(SCENE_QUERY_STAT(GSTN_MinionBenchTrapped), false));
			if (bInside && !(SlotFlags[Slot] & PenetrationCounted))
			{
				SlotFlags[Slot] |= PenetrationCounted;
				CountTrapped(Slot, Now, ModeName, TEXT("in penetration after its step"), Frame);
			}
			else if (!bInside)
			{
				SlotFlags[Slot] &= static_cast<uint8>(~PenetrationCounted);
			}
		}

		if (!bHasCourse)
		{
			continue;
		}
		if (Now.Z < Center.Z - FellBelowCourseDistance && !(SlotFlags[Slot] & FellCounted))
		{
			SlotFlags[Slot] |= FellCounted;
			++FellBelowCourse;
			UE_LOG(LogGSTNMinion, Warning, TEXT("Bench: minion slot %d fell below the course at %s (frame %d)"), Slot, *Now.ToCompactString(), Frame);
		}
		if (FVector::DistSquared(Previous, Now) < 0.01)
		{
			continue;
		}
		// Tunnelled: the capsule centre's segment of this frame crosses a course box. The sweeps keep the capsule (and so its
		// centre) out of every box, also when a launch carries it over a wall, so any crossing is a collision failure
		const UGSTN_MinionDefinition* Definition = Minions->GetDefinition(Store.TypeIndex[Slot]);
		const FVector Lift(0.0, 0.0, Definition ? Definition->Stats.CapsuleHalfHeight : 88.0);
		for (const TWeakObjectPtr<UPrimitiveComponent>& Box : CourseBoxes)
		{
			UPrimitiveComponent* Component = Box.Get();
			FHitResult Hit;
			if (Component && Component->LineTraceComponent(Hit, Previous + Lift, Now + Lift, FCollisionQueryParams()))
			{
				++Tunnelled;
				UE_LOG(LogGSTNMinion, Warning, TEXT("Bench: minion slot %d tunnelled %s between %s and %s (frame %d)"), Slot,
					*GetNameSafe(Component->GetOwner()), *Previous.ToCompactString(), *Now.ToCompactString(), Frame);
				break;
			}
		}
	}

	// Massacre: a seeded round of launches every N fixed frames, drawn in slot order (applied by the next movement step)
	const int32 Every = CVarBenchLaunchEveryFrames.GetValueOnGameThread();
	if (Scenario == EGSTN_MinionBenchScenario::Massacre && Every > 0 && Frame % Every == 0)
	{
		const float Fraction = CVarBenchLaunchFraction.GetValueOnGameThread();
		const float MaxSpeed = CVarBenchLaunchSpeed.GetValueOnGameThread();
		for (const int32 Slot : Slots)
		{
			if (LaunchStream.FRand() >= Fraction)
			{
				continue;
			}
			const bool bPushBack = LaunchStream.FRand() < PushBackShare;
			const float Speed = LaunchStream.FRandRange(0.3f, 1.f) * MaxSpeed;
			const float YawRadians = FMath::DegreesToRadians(LaunchStream.FRandRange(0.f, 360.f));
			// Push backs are horizontal; launches are drawn in an upward cone
			const float TiltRadians = FMath::DegreesToRadians(bPushBack ? 90.f : LaunchStream.FRandRange(0.f, LaunchConeDegrees));
			const FVector Direction(FMath::Sin(TiltRadians) * FMath::Cos(YawRadians), FMath::Sin(TiltRadians) * FMath::Sin(YawRadians), FMath::Cos(TiltRadians));
			Minions->LaunchMinion(Store.HandleOfSlot(Slot), Direction * Speed, bPushBack);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Course

void UGSTN_MinionBenchSubsystem::AddCourseBox(const FVector& Offset, const FVector& Size, const FRotator& Rotation)
{
	UWorld* World = GetWorld();
	const FTransform Transform(Rotation, Center + Offset, Size / 100.0);
	// The engine's 100 cm cube (an /Engine asset, no project content). Should a cooked build lack it, a bare box component
	// gives the same collision, invisible
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube)
	{
		AStaticMeshActor* Actor = World->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Actor)
		{
			return;
		}
		UStaticMeshComponent* Mesh = Actor->GetStaticMeshComponent();
		// Movable: a static component may not get a mesh after construction
		Mesh->SetMobility(EComponentMobility::Movable);
		Mesh->SetStaticMesh(Cube);
		Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Actor->FinishSpawning(Transform);
		CourseActors.Add(Actor);
		CourseBoxes.Add(Mesh);
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.ObjectFlags |= RF_Transient;
	AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), Transform, Params);
	if (!Actor)
	{
		return;
	}
	UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("CourseBox"));
	Box->SetBoxExtent(FVector(50.0));
	Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Actor->SetRootComponent(Box);
	Box->SetWorldTransform(Transform);
	Box->RegisterComponent();
	CourseActors.Add(Actor);
	CourseBoxes.Add(Box);
}

void UGSTN_MinionBenchSubsystem::SetCourse(bool bEnable)
{
	for (AActor* Actor : CourseActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	CourseActors.Reset();
	CourseBoxes.Reset();
	if (!bEnable || !GetWorld())
	{
		UE_LOG(LogGSTNMinion, Log, TEXT("Bench: course removed"));
		return;
	}

	bCenterResolved = false;
	ResolveCenter();
	// Offsets from the centre; the floor's top is at the centre's height. Minions come from the ring toward the centre, so each
	// obstacle is met on the way in. Walls are oblique to the radial paths: a straight-line chaser slides along them instead of
	// pushing into a perpendicular face, which would count as "stuck" by the brain's fault, not the movement's

	// Floor, 300 m: a 3000 cm/s launch in the 35 degree cone lands within about 90 m
	AddCourseBox(FVector(0.0, 0.0, -50.0), FVector(30000.0, 30000.0, 100.0), FRotator::ZeroRotator);

	// Stairs on +X: five 30 cm steps (60 cm treads) up to a 150 cm platform that ends in a ledge at 500 cm from the centre
	for (int32 Step = 0; Step < 5; ++Step)
	{
		const double Height = 30.0 * (Step + 1);
		const double OuterX = 1100.0 - 60.0 * Step;
		AddCourseBox(FVector((800.0 + OuterX) * 0.5, 0.0, Height * 0.5), FVector(OuterX - 800.0, 400.0, Height), FRotator::ZeroRotator);
	}
	AddCourseBox(FVector(650.0, 0.0, 75.0), FVector(300.0, 400.0, 150.0), FRotator::ZeroRotator);

	// 40 degree ramp on -X, rising toward the centre onto a 300 cm platform whose ledge (300 cm drop) is at 500 cm. The ramp is a
	// thick box pitched up, so there is no open wedge under it
	{
		const double Slope = FMath::DegreesToRadians(40.0);
		const double Rise = 300.0;
		const double Run = Rise / FMath::Tan(Slope);
		const double Length = Rise / FMath::Sin(Slope);
		const double Thickness = 400.0;
		const FVector TopCentre(-1300.0 + Run * 0.5, 0.0, Rise * 0.5);
		const FVector Normal(-FMath::Sin(Slope), 0.0, FMath::Cos(Slope));
		AddCourseBox(TopCentre - Normal * (Thickness * 0.5), FVector(Length, 400.0, Thickness), FRotator(40.0, 0.0, 0.0));
		const double PlatformStart = -1300.0 + Run;
		AddCourseBox(FVector((PlatformStart - 500.0) * 0.5, 0.0, Rise * 0.5), FVector(-500.0 - PlatformStart, 400.0, Rise), FRotator::ZeroRotator);
	}

	// Thin wall on +Y: 10 cm, 300 cm tall, 600 cm long, at 45 degrees to the radial paths
	AddCourseBox(FVector(0.0, 800.0, 150.0), FVector(600.0, 10.0, 300.0), FRotator(0.0, 45.0, 0.0));

	// 90 degree corner on -Y, its outer vertex toward the ring (minions slide around it; its inside opens toward the centre)
	{
		const FVector Vertex(0.0, -900.0, 100.0);
		const double Half = 200.0;
		const double Diagonal = Half * UE_INV_SQRT_2;
		AddCourseBox(Vertex + FVector(Diagonal, Diagonal, 0.0), FVector(2.0 * Half, 20.0, 200.0), FRotator(0.0, 45.0, 0.0));
		AddCourseBox(Vertex + FVector(-Diagonal, Diagonal, 0.0), FVector(2.0 * Half, 20.0, 200.0), FRotator(0.0, -45.0, 0.0));
	}

	UE_LOG(LogGSTNMinion, Log, TEXT("Bench: course built at %s (%d boxes)"), *Center.ToCompactString(), CourseBoxes.Num());
}

// ---------------------------------------------------------------------------------------------------------------------------------
// Brain stand-in

void UGSTN_MinionBenchSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (BenchSeconds >= 0.f)
	{
		BenchSeconds += DeltaTime;
		const float ReportAt = CVarBenchAutoReportSeconds.GetValueOnGameThread();
		if (ReportAt > 0.f && BenchSeconds >= ReportAt)
		{
			BenchSeconds = -1.f;
			Report();
			if (CVarBenchAutoQuit.GetValueOnGameThread() && GEngine)
			{
				GEngine->Exec(GetWorld(), TEXT("quit"));
				return;
			}
		}
	}
	if (Scenario == EGSTN_MinionBenchScenario::Idle || Handles.IsEmpty())
	{
		return;
	}
	UWorld* World = GetWorld();
	UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(World);
	if (!Minions)
	{
		return;
	}
	// Stand-in brain: walk to the nearest player (the bench centre without one) and stop at half the ring radius (150 cm at
	// least). Runs in the world tick and only writes intents (latest wins); a launched minion walks back once it lands
	const double StopDistance = FMath::Max(150.0, RingRadius * 0.5);
	const FGSTN_MinionStore& Store = Minions->GetStore();
	Handles.RemoveAll([Minions](const FGSTN_MinionHandle& Handle) { return !Minions->IsValidHandle(Handle); });
	for (const FGSTN_MinionHandle& Handle : Handles)
	{
		const FVector Position(Store.Position[Handle.Index]);
		FGSTN_MinionIntent Intent;
		FVector Target = Center;
		FindNearestPlayer(World, Position, Target);
		const FVector2D ToTarget(Target.X - Position.X, Target.Y - Position.Y);
		const double Distance = ToTarget.Size();
		if (Distance > StopDistance)
		{
			Intent.MoveDir = ToTarget / Distance;
			const UGSTN_MinionDefinition* Definition = Minions->GetDefinition(Store.TypeIndex[Handle.Index]);
			Intent.DesiredSpeed = Definition ? Definition->Stats.WalkSpeed : 200.f;
		}
		Minions->SetIntent(Handle, Intent);
	}
}
