// Minion runtime (authority batch, M8 G3; spike 3 scenario, course and robustness counters G4).

#include "Debug/GSTN_MinionBench.h"

#include "Brain/GSTN_MinionBrainSubsystem.h"
#include "Brain/GSTN_MinionStateTree.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Data/GSTN_MinionDefinition.h"
#include "Data/GSTN_MinionWaveDefinition.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GSTNMinionCoreModule.h"
#include "HAL/IConsoleManager.h"
#include "NetworkPredictionWorldManager.h"
#include "Net/GSTN_MinionNetRates.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Simulation/GSTN_HitInbox.h"
#include "Simulation/GSTN_LagCompensation.h"
#include "GSTN_MinionTags.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "World/GSTN_CombatWorldSubsystem.h"
#include "Spawn/GSTN_MinionSpawner.h"
#include "Spawn/GSTN_MinionSpawnerActor.h"

namespace
{
	TAutoConsoleVariable<FString> CVarBenchDefinition(
		TEXT("gstn.Minion.Bench.Definition"), TEXT(""),
		TEXT("Object path of the UGSTN_MinionDefinition the bench spawns (empty = a transient definition built in code)."));

	// G7a: the transient grunts had no hurtbox set, so no player swing could ever hit them (player -> minion scenarios). Read when a
	// transient definition is first built, so set it before the first Spawn/Attack of the world
	TAutoConsoleVariable<FString> CVarBenchHurtboxSet(
		TEXT("gstn.Minion.Bench.HurtboxSet"), TEXT(""),
		TEXT("Object path of a UGSTN_HurtboxSetDefinition the transient bench grunts get (empty = none: they cannot be hit)."));

	void ApplyBenchHurtboxSet(UGSTN_MinionDefinition* Definition)
	{
		const FString Path = CVarBenchHurtboxSet.GetValueOnGameThread();
		if (Definition && !Path.IsEmpty())
		{
			Definition->HurtboxSet = TSoftObjectPtr<UGSTN_HurtboxSetDefinition>(FSoftObjectPath(Path));
		}
	}

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
		TEXT("gstn.Minion.Bench.Spawn <N> <Idle|Chase|Massacre|Brain>: spawns N bench minions in a ring around the first player (or the origin)."),
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
			else if (Args.Num() > 1 && Args[1].Equals(TEXT("Brain"), ESearchCase::IgnoreCase))
			{
				Scenario = EGSTN_MinionBenchScenario::Brain;
			}
			Bench->SpawnBench(Count, Scenario);
		}));

	FAutoConsoleCommandWithWorldAndArgs BenchPopulationCommand(
		TEXT("gstn.Minion.Bench.Population"),
		TEXT("gstn.Minion.Bench.Population <N=150> <NearCount=60> <FarRadius=12000> [Idle|Chase|Brain]: N minions of which NearCount stand in a ring around the first player and the rest on a far ring (they go Dormant). G11 population case: read the awake count in gstn.Minion.Bench.Report."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGSTN_MinionBenchSubsystem* Bench = UGSTN_MinionBenchSubsystem::Get(World);
			if (!Bench)
			{
				UE_LOG(LogGSTNMinion, Warning, TEXT("gstn.Minion.Bench.Population: no bench in this world"));
				return;
			}
			EGSTN_MinionBenchScenario Scenario = EGSTN_MinionBenchScenario::Idle;
			if (Args.Num() > 3 && Args[3].Equals(TEXT("Chase"), ESearchCase::IgnoreCase))
			{
				Scenario = EGSTN_MinionBenchScenario::Chase;
			}
			else if (Args.Num() > 3 && Args[3].Equals(TEXT("Brain"), ESearchCase::IgnoreCase))
			{
				Scenario = EGSTN_MinionBenchScenario::Brain;
			}
			Bench->SpawnBench(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 150, Scenario, Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 60, Args.Num() > 2 ? FCString::Atod(*Args[2]) : 12000.0);
		}));

	FAutoConsoleCommandWithWorldAndArgs BenchAttackCommand(
		TEXT("gstn.Minion.Bench.Attack"),
		TEXT("gstn.Minion.Bench.Attack <N=1> <FighterIndex=0> [Brain]: spawns N attackers around the FighterIndex-th registered fighter (ascending entity id); they walk up and swing at it in a loop. Logs the target actor."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGSTN_MinionBenchSubsystem* Bench = UGSTN_MinionBenchSubsystem::Get(World);
			if (!Bench)
			{
				UE_LOG(LogGSTNMinion, Warning, TEXT("gstn.Minion.Bench.Attack: no bench in this world"));
				return;
			}
			Bench->SpawnAttackers(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1, Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0, Args.Num() > 2 && Args[2].Equals(TEXT("Brain"), ESearchCase::IgnoreCase));
		}));

	FAutoConsoleCommandWithWorldAndArgs BenchPokeCommand(
		TEXT("gstn.Minion.Bench.Poke"),
		TEXT("gstn.Minion.Bench.Poke <BenchIndex=0>: posts one synthetic 1 damage hit on that bench minion through PostToEntityInbox (wakes a Far or Dormant minion at the next batch). Logs: Bench: poke index=.. id=.. tier=.. posted=.. frame=.."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGSTN_MinionBenchSubsystem* Bench = UGSTN_MinionBenchSubsystem::Get(World);
			if (!Bench || !Bench->Poke(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0))
			{
				UE_LOG(LogGSTNMinion, Warning, TEXT("gstn.Minion.Bench.Poke: no bench or no such bench minion"));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs BenchWaveCommand(
		TEXT("gstn.Minion.Bench.Wave"),
		TEXT("gstn.Minion.Bench.Wave <N=8> <CorpseSeconds=1>: a spawner plays a wave of N bench grunts around the first player; the bodies stay CorpseSeconds after they die. Logs: Bench: wave of .. started, Bench: wave cleared frame=.."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGSTN_MinionBenchSubsystem* Bench = UGSTN_MinionBenchSubsystem::Get(World);
			if (!Bench || Bench->SpawnWave(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 8, Args.Num() > 1 ? FCString::Atof(*Args[1]) : 1.f) == 0)
			{
				UE_LOG(LogGSTNMinion, Warning, TEXT("gstn.Minion.Bench.Wave: no bench or the wave did not start (client world?)"));
			}
		}));

	FAutoConsoleCommandWithWorld BenchKillCommand(
		TEXT("gstn.Minion.Bench.Kill"),
		TEXT("Posts a lethal hit on every live bench and wave minion: they play the death timeline, then wait CorpseSeconds as bodies. Logs: Bench: kill posted=.. frame=.."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UGSTN_MinionBenchSubsystem* Bench = UGSTN_MinionBenchSubsystem::Get(World))
			{
				Bench->Kill();
			}
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

	// Spike 5 measurement hooks (combat-m8-minions.md 9.2: the 12-minion cleave and the rewound query should cost tens of microseconds).
	// They only feed the counters of gstn.Minion.Bench.Report; nothing in the simulation depends on them

	FAutoConsoleCommandWithWorldAndArgs BenchCleaveCommand(
		TEXT("gstn.Minion.Bench.Cleave"),
		TEXT("gstn.Minion.Bench.Cleave <N=12> <Damage=1>: posts one synthetic hit (neutral attacker) on each of the N lowest live minions; the next batch drains them (drainUsPerHit in the report)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(World);
			if (!Minions)
			{
				UE_LOG(LogGSTNMinion, Warning, TEXT("gstn.Minion.Bench.Cleave: no active minion subsystem"));
				return;
			}
			const int32 Count = Args.Num() > 0 ? FMath::Max(FCString::Atoi(*Args[0]), 1) : 12;
			const float Damage = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 1.f;
			const FGSTN_MinionStore& Store = Minions->GetStore();
			const int32 Frame = FMath::Max(Minions->GetLastBatchFrame(), 0);
			int32 Posted = 0;
			for (int32 Slot = Store.NextAliveSlot(0); Slot != INDEX_NONE && Posted < Count; Slot = Store.NextAliveSlot(Slot + 1))
			{
				FGSTN_HitInboxEntry Hit;
				Hit.Frame = Frame;
				// An id no fighter uses: the pipeline treats it as a neutral attacker, nothing is looked up for it
				Hit.AttackerId = 0x3FFFFFF0;
				Hit.AttackerTeam = 255;
				// 255: no relationship index, the TeamFilter step resolves it from the teams (no team = Neutral, hittable by default)
				Hit.Relationship = 255;
				Hit.Hit.Damage = Damage;
				Hit.HitLocation = FVector(Store.Position[Slot]);
				const FGSTN_MinionHandle Handle = Store.HandleOfSlot(Slot);
				Posted += Minions->PostToEntityInbox(GSTNEntityKinds::MakeMinionEntityId(static_cast<uint16>(Slot)), Handle.Generation, Hit.ToInboxEntry()) ? 1 : 0;
			}
			UE_LOG(LogGSTNMinion, Log, TEXT("Bench: cleave of %d hits posted at frame %d"), Posted, Frame);
		}));

	FAutoConsoleCommandWithWorldAndArgs BenchLagCompCommand(
		TEXT("gstn.Minion.Bench.LagComp"),
		TEXT("gstn.Minion.Bench.LagComp <Rounds=10> <RewindMs=250>: runs a rewound candidate query around every live minion, Rounds times (lagCompUsPerQuery in the report)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(World);
			const UNetworkPredictionWorldManager* Manager = World ? World->GetSubsystem<UNetworkPredictionWorldManager>() : nullptr;
			if (!Minions)
			{
				UE_LOG(LogGSTNMinion, Warning, TEXT("gstn.Minion.Bench.LagComp: no active minion subsystem"));
				return;
			}
			const int32 Rounds = Args.Num() > 0 ? FMath::Max(FCString::Atoi(*Args[0]), 1) : 10;
			const float RewindAgeMs = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 250.f;
			const int32 StepMs = Manager && Manager->GetFixedTickState().FixedStepMS > 0 ? Manager->GetFixedTickState().FixedStepMS : 16;
			const float NowMs = static_cast<float>(FGSTN_LagCompTime::TimeOfFrame(FMath::Max(Minions->GetLastBatchFrame(), 0), StepMs));
			const FGSTN_MinionStore& Store = Minions->GetStore();
			TArray<FGSTN_MinionRewoundPose> Poses;
			int32 Found = 0;
			for (int32 Round = 0; Round < Rounds; ++Round)
			{
				for (int32 Slot = Store.NextAliveSlot(0); Slot != INDEX_NONE; Slot = Store.NextAliveSlot(Slot + 1))
				{
					// A sword-sized hitbox bound in front of where the minion is now
					const FVector Centre(Store.Position[Slot]);
					Minions->GatherRewoundCandidates(FBox(Centre - FVector(60.0), Centre + FVector(60.0)), NowMs - RewindAgeMs, StepMs, Poses);
					Found += Poses.Num();
				}
			}
			UE_LOG(LogGSTNMinion, Log, TEXT("Bench: lag comp rounds=%d rewindMs=%.0f poses=%d"), Rounds, RewindAgeMs, Found);
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
		ApplyBenchHurtboxSet(TransientDefinition);
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

int32 UGSTN_MinionBenchSubsystem::SpawnBench(int32 Count, EGSTN_MinionBenchScenario InScenario, int32 NearCount, double FarRadius)
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
	const int32 NumNear = NearCount == INDEX_NONE ? Count : FMath::Clamp(NearCount, 0, Count);
	const int32 NumFar = Count - NumNear;
	// Ring: about 150 cm between neighbours, at least 600 cm out
	RingRadius = FMath::Max(600.0, NumNear * 150.0 / UE_TWO_PI);

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
		const bool bFar = Index >= NumNear;
		const double Angle = bFar ? UE_TWO_PI * (Index - NumNear) / FMath::Max(NumFar, 1) : UE_TWO_PI * Index / FMath::Max(NumNear, 1);
		const double Radius = bFar ? FMath::Max(FarRadius, RingRadius) : RingRadius;
		FVector Location = RingCenter + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0);
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
	if (Scenario == EGSTN_MinionBenchScenario::Brain)
	{
		// The test tree when the editor module built one; otherwise (or with gstn.Minion.Brain.Native 1) the subsystem runs the native brain
		StartBenchBrains(World);
	}
	Minions->ResetBenchTotals();
	ResetCounters(Minions->GetStore().Capacity());
	LaunchStream.Initialize(CVarBenchSeed.GetValueOnGameThread());
	BindMovementHook(Minions);

	const TCHAR* ScenarioName = Scenario == EGSTN_MinionBenchScenario::Massacre ? TEXT("Massacre")
		: (Scenario == EGSTN_MinionBenchScenario::Chase ? TEXT("Chase") : (Scenario == EGSTN_MinionBenchScenario::Brain ? TEXT("Brain") : TEXT("Idle")));
	UE_LOG(LogGSTNMinion, Log, TEXT("Bench: %d minions queued (%s), ring radius %.0f%s, %d ring points skipped (inside geometry)"), Queued,
		ScenarioName, RingRadius, bOnCourse ? TEXT(", on the course") : TEXT(""), Skipped);
	return Queued;
}

int32 UGSTN_MinionBenchSubsystem::SpawnAttackers(int32 Count, int32 TargetIndex, bool bBrain)
{
	UWorld* World = GetWorld();
	UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(World);
	UGSTN_CombatWorldSubsystem* Combat = World ? World->GetSubsystem<UGSTN_CombatWorldSubsystem>() : nullptr;
	if (!Minions || !Combat)
	{
		UE_LOG(LogGSTNMinion, Warning, TEXT("Bench: Attack needs the minion and the combat world subsystems (client world?)"));
		return 0;
	}
	TArray<int32> Fighters;
	Combat->GetRegisteredEntityIds(Fighters);
	Fighters.Sort();
	if (!Fighters.IsValidIndex(TargetIndex))
	{
		UE_LOG(LogGSTNMinion, Warning, TEXT("Bench: Attack: fighter index %d, %d registered fighters"), TargetIndex, Fighters.Num());
		return 0;
	}
	const AActor* TargetActor = Combat->FindOwnerActor(Fighters[TargetIndex]);
	if (!TargetActor)
	{
		UE_LOG(LogGSTNMinion, Warning, TEXT("Bench: Attack: fighter %d has no owner actor"), Fighters[TargetIndex]);
		return 0;
	}
	if (!AttackDefinition)
	{
		// Swing: 40 frames, one sphere over frames 4 to 9, 100 cm ahead at chest height, in the root space of the minion
		UGSTN_CombatTimeline* Timeline = NewObject<UGSTN_CombatTimeline>(this, TEXT("GSTN_MinionBenchSwing"), RF_Transient);
		Timeline->Data.TotalFrames = 40;
		Timeline->Data.FirstActionableFrame = 40;
		TInstancedStruct<FGSTN_TimelineTrack>& Track = Timeline->Data.Tracks.AddDefaulted_GetRef();
		Track.InitializeAs<FGSTN_TimelineTrack_Hitbox>();
		FGSTN_TimelineTrack_Hitbox& Hitbox = Track.GetMutable<FGSTN_TimelineTrack_Hitbox>();
		Hitbox.StartFrame = 4;
		Hitbox.EndFrame = 9;
		FGSTN_HitboxSpec& Spec = Hitbox.Hitboxes.AddDefaulted_GetRef();
		Spec.Shape.Type = EGSTN_ShapeType::Sphere;
		Spec.Shape.Extent = FVector(70.0);
		Spec.Shape.Local = FTransform(FVector(100.0, 0.0, 60.0));
		Spec.Space = EGSTN_HitboxSpace::Root;
		Spec.Data.Damage = 5.f;
		Spec.Data.FixedKnockback = 100.f;
		AttackTimeline = Timeline;

		AttackDefinition = NewObject<UGSTN_MinionDefinition>(this, TEXT("GSTN_MinionBenchAttackDefinition"), RF_Transient);
		AttackDefinition->TypeTag = FGameplayTag::RequestGameplayTag(TEXT("Minion.Type.Grunt"), false);
		AttackDefinition->Stats.MaxHealth = 40.f;
		ApplyBenchHurtboxSet(AttackDefinition);
		FGSTN_MinionMove& Move = AttackDefinition->Moves.AddDefaulted_GetRef();
		const FGameplayTag Swing = FGameplayTag::RequestGameplayTag(TEXT("Command.Minion.Attack.Swing"), false);
		Move.Command = Swing.IsValid() ? Swing : FGameplayTag(GSTNMinionTags::Type);
		Move.Timeline = Timeline;
		Move.MaxRange = 260.f;
		Move.CooldownFrames = 20;
	}
	Scenario = EGSTN_MinionBenchScenario::Attack;
	BenchSeconds = 0.f;
	AttackTargetId = Fighters[TargetIndex];
	bAttackBrain = bBrain;
	MaxHoldersSeen = 0;
	Center = TargetActor->GetActorLocation();
	bCenterResolved = true;
	RingRadius = 250.0;
	Count = FMath::Max(Count, 0);
	int32 Queued = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const double Angle = UE_TWO_PI * Index / FMath::Max(Count, 1);
		const FVector Location = Center + FVector(FMath::Cos(Angle) * RingRadius, FMath::Sin(Angle) * RingRadius, 0.0);
		const FRotator Facing = (Center - Location).Rotation();
		const FGSTN_MinionHandle Handle = UGSTN_MinionSpawner::Spawn(World, AttackDefinition, FTransform(FRotator(0.0, Facing.Yaw, 0.0), Location), FGSTN_MinionSpawnParams());
		if (!Handle.IsNull())
		{
			Handles.Add(Handle);
			++Queued;
		}
	}
	if (bBrain)
	{
		// The brain, not the scripted swing loop, drives the attackers. Its target is pinned to the fighter being attacked so the token
		// numbers mean the same as in the scripted run (the nearest-fighter rule would pick the other pawn if it stood closer)
		if (UGSTN_MinionBrainSubsystem* Brain = UGSTN_MinionBrainSubsystem::Get(World))
		{
			const int32 PinnedId = AttackTargetId;
			TWeakObjectPtr<const UGSTN_CombatWorldSubsystem> WeakCombat(Combat);
			Brain->SetTargetProvider([PinnedId, WeakCombat](TArray<FGSTN_MinionBrainTarget>& Out)
			{
				if (const AActor* Actor = WeakCombat.IsValid() ? WeakCombat->FindOwnerActor(PinnedId) : nullptr)
				{
					FGSTN_MinionBrainTarget& Target = Out.AddDefaulted_GetRef();
					Target.Id = PinnedId;
					Target.Position = Actor->GetActorLocation();
				}
			});
		}
		StartBenchBrains(World);
	}
	Minions->ResetBenchTotals();
	ResetCounters(Minions->GetStore().Capacity());
	BindMovementHook(Minions);
	UE_LOG(LogGSTNMinion, Log, TEXT("Bench: attack %d minions queued on fighter entity %d actor %s at %s%s"), Queued, AttackTargetId, *TargetActor->GetName(), *Center.ToCompactString(), bBrain ? TEXT(" brain") : TEXT(""));
	return Queued;
}

void UGSTN_MinionBenchSubsystem::StartBenchBrains(UWorld* World)
{
	UGSTN_MinionBrainSubsystem* Brain = UGSTN_MinionBrainSubsystem::Get(World);
	if (!Brain)
	{
		UE_LOG(LogGSTNMinion, Warning, TEXT("Bench: no brain subsystem (client world?); the minions will not think"));
		return;
	}
	// A new run starts from a clean report: the spike numbers are per run
	Brain->ResetStats();
	UObject* Tree = GSTNMinion::GetTestBrainTreeFactory().IsBound() ? GSTNMinion::GetTestBrainTreeFactory().Execute() : nullptr;
	int32 Started = 0;
	for (const FGSTN_MinionHandle& Handle : Handles)
	{
		if (!Brain->HasBrain(Handle))
		{
			Started += Brain->StartBrain(Handle, Tree) ? 1 : 0;
		}
	}
	UE_LOG(LogGSTNMinion, Log, TEXT("Bench: %d brains started (%s)"), Started, Tree ? TEXT("StateTree chase and swing") : TEXT("native, no test tree"));
}

bool UGSTN_MinionBenchSubsystem::Poke(int32 BenchIndex)
{
	UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(GetWorld());
	if (!Minions || !Handles.IsValidIndex(BenchIndex) || !Minions->IsValidHandle(Handles[BenchIndex]))
	{
		return false;
	}
	const FGSTN_MinionStore& Store = Minions->GetStore();
	const FGSTN_MinionHandle Handle = Handles[BenchIndex];
	const int32 Frame = FMath::Max(Minions->GetLastBatchFrame(), 0);
	FGSTN_HitInboxEntry Hit;
	Hit.Frame = Frame;
	// The neutral attacker of the cleave: an id no fighter uses, no team, no relationship index
	Hit.AttackerId = 0x3FFFFFF0;
	Hit.AttackerTeam = 255;
	Hit.Relationship = 255;
	Hit.Hit.Damage = 1.f;
	Hit.HitLocation = FVector(Store.Position[Handle.Index]);
	const int32 EntityId = GSTNEntityKinds::MakeMinionEntityId(Handle.Index);
	const bool bPosted = Minions->PostToEntityInbox(EntityId, Handle.Generation, Hit.ToInboxEntry());
	UE_LOG(LogGSTNMinion, Log, TEXT("Bench: poke index=%d id=%d tier=%d posted=%d frame=%d"), BenchIndex, EntityId, static_cast<int32>(Store.LODTier[Handle.Index]), bPosted ? 1 : 0, Frame);
	return bPosted;
}

int32 UGSTN_MinionBenchSubsystem::SpawnWave(int32 Count, float CorpseSeconds)
{
	UWorld* World = GetWorld();
	UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(World);
	if (!World || !Minions)
	{
		UE_LOG(LogGSTNMinion, Warning, TEXT("Bench: the minion subsystem is not active here (client world?)"));
		return 0;
	}
	Count = FMath::Clamp(Count, 1, GSTNMinion::MaxWaveTotal);
	UGSTN_MinionDefinition* Definition = ResolveDefinition();
	Definition->CorpseSeconds = FMath::Max(CorpseSeconds, 0.f);

	if (WaveSpawner)
	{
		WaveSpawner->Destroy();
		WaveSpawner = nullptr;
	}
	// Entries hold at most 64 minions each; a bigger wave is several entries on the same frame
	WaveDefinition = NewObject<UGSTN_MinionWaveDefinition>(this, NAME_None, RF_Transient);
	const double Ring = FMath::Max(300.0, Count * 150.0 / UE_TWO_PI);
	for (int32 Left = Count; Left > 0; Left -= GSTNMinion::MaxWaveEntryCount)
	{
		FGSTN_MinionWaveEntry& Entry = WaveDefinition->Entries.AddDefaulted_GetRef();
		Entry.Definition = Definition;
		Entry.Count = FMath::Min(Left, GSTNMinion::MaxWaveEntryCount);
		Entry.RingRadius = static_cast<float>(Ring);
	}

	bCenterResolved = false;
	const FVector Where = ResolveCenter();
	// Deferred so the wave and the flags are set before BeginPlay: no trigger, started by hand below
	WaveSpawner = World->SpawnActorDeferred<AGSTN_MinionSpawnerActor>(AGSTN_MinionSpawnerActor::StaticClass(), FTransform(Where), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!WaveSpawner)
	{
		return 0;
	}
	WaveSpawner->Wave = WaveDefinition;
	WaveSpawner->bUseTrigger = false;
	WaveSpawner->bStartOnBeginPlay = false;
	WaveSpawner->FinishSpawning(FTransform(Where));
	WaveSpawner->OnWaveClearedNative.AddLambda([](int32 Frame)
	{
		UE_LOG(LogGSTNMinion, Log, TEXT("Bench: wave cleared frame=%d"), Frame);
	});
	Minions->ResetBenchTotals();
	if (!WaveSpawner->StartWave())
	{
		return 0;
	}
	UE_LOG(LogGSTNMinion, Log, TEXT("Bench: wave of %d started, corpse %.2f s, ring %.0f, frame=%d"), Count, Definition->CorpseSeconds, Ring, Minions->GetLastBatchFrame());
	return Count;
}

int32 UGSTN_MinionBenchSubsystem::Kill()
{
	UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(GetWorld());
	if (!Minions)
	{
		return 0;
	}
	TArray<FGSTN_MinionHandle> Targets;
	for (const FGSTN_MinionHandle& Handle : Handles)
	{
		if (Minions->IsAlive(Handle))
		{
			Targets.Add(Handle);
		}
	}
	if (WaveSpawner)
	{
		WaveSpawner->GetAliveHandles(Targets);
	}

	const FGSTN_MinionStore& Store = Minions->GetStore();
	const int32 Frame = FMath::Max(Minions->GetLastBatchFrame(), 0);
	int32 Posted = 0;
	for (const FGSTN_MinionHandle& Handle : Targets)
	{
		FGSTN_HitInboxEntry Hit;
		Hit.Frame = Frame;
		// The neutral attacker of the cleave and the poke; the damage is far above any health, whatever the rules' damage mode
		Hit.AttackerId = 0x3FFFFFF0;
		Hit.AttackerTeam = 255;
		Hit.Relationship = 255;
		Hit.Hit.Damage = 100000.f;
		Hit.HitLocation = FVector(Store.Position[Handle.Index]);
		Posted += Minions->PostToEntityInbox(GSTNEntityKinds::MakeMinionEntityId(Handle.Index), Handle.Generation, Hit.ToInboxEntry()) ? 1 : 0;
	}
	UE_LOG(LogGSTNMinion, Log, TEXT("Bench: kill posted=%d frame=%d"), Posted, Frame);
	return Posted;
}

void UGSTN_MinionBenchSubsystem::Clear()
{
	if (UGSTN_MinionBrainSubsystem* Brain = UGSTN_MinionBrainSubsystem::Get(GetWorld()))
	{
		for (const FGSTN_MinionHandle& Handle : Handles)
		{
			Brain->StopBrain(Handle);
		}
		// Back to the default target source (the registered fighters)
		Brain->SetTargetProvider(nullptr);
	}
	bAttackBrain = false;
	for (const FGSTN_MinionHandle& Handle : Handles)
	{
		UGSTN_MinionSpawner::Despawn(GetWorld(), Handle);
	}
	if (WaveSpawner)
	{
		// The wave's own minions: free them at once (corpses included) and drop the spawner
		TArray<FGSTN_MinionHandle> WaveHandles;
		WaveSpawner->GetSpawnedHandles(WaveHandles);
		for (const FGSTN_MinionHandle& Handle : WaveHandles)
		{
			UGSTN_MinionSpawner::Despawn(GetWorld(), Handle);
		}
		WaveSpawner->Destroy();
		WaveSpawner = nullptr;
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
	// G11 host budget, one parseable key=value line: T0 after the cap, how many the cap held back, awake = T0..T2, dormant = T3, per tier
	int32 PerTier[4] = {};
	const FGSTN_MinionStore& Store = Minions->GetStore();
	for (int32 Slot = Store.NextAliveSlot(0); Slot != INDEX_NONE; Slot = Store.NextAliveSlot(Slot + 1))
	{
		++PerTier[FMath::Min<int32>(Store.LODTier[Slot], 3)];
	}
	UE_LOG(LogGSTNMinion, Log, TEXT("MinionBench budget activeT0=%d demoted=%d awake=%d dormant=%d t0=%d t1=%d t2=%d t3=%d alive=%d t0RateHz=%d maxBatchUs=%.1f avgBatchUs=%.1f"),
		Minions->GetActiveT0Count(), Minions->GetT0DemotedCount(), PerTier[0] + PerTier[1] + PerTier[2], PerTier[3], PerTier[0], PerTier[1], PerTier[2], PerTier[3],
		Minions->GetLastBatchStats().Alive, GSTNMinionNetRate::GetCombatRateHz(), Totals.MaxBatchSeconds * 1.0e6, AvgUs);
	// Spike 5: the drain cost per hit (a 12-minion cleave costs 12 of them) and the rewound candidate query
	const double DrainUsPerHit = Totals.DrainedHits > 0 ? Totals.DrainSeconds * 1.0e6 / static_cast<double>(Totals.DrainedHits) : 0.0;
	const double LagCompUsPerQuery = Totals.LagCompQueries > 0 ? Totals.LagCompSeconds * 1.0e6 / static_cast<double>(Totals.LagCompQueries) : 0.0;
	UE_LOG(LogGSTNMinion, Log, TEXT("MinionBench attack started=%lld tokensDenied=%lld attackHits=%lld maxHolders=%d holdersNow=%d target=%d"),
		Totals.AttacksStarted, Totals.TokensDenied, Totals.AttackHits, MaxHoldersSeen, AttackTargetId != INDEX_NONE ? Minions->GetAttackTokens().NumHolders(AttackTargetId) : 0, AttackTargetId);
	UE_LOG(LogGSTNMinion, Log, TEXT("MinionBench spike5 drainedHits=%lld drainUsPerHit=%.2f cleave12Us=%.1f lagCompQueries=%lld lagCompUsPerQuery=%.2f candidates=%lld rejected=%lld ignoredEffects=%lld"),
		Totals.DrainedHits, DrainUsPerHit, DrainUsPerHit * 12.0, Totals.LagCompQueries, LagCompUsPerQuery, Totals.LagCompCandidates, Totals.LagCompRejected, Totals.IgnoredEffects);
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
	if (AttackTargetId != INDEX_NONE)
	{
		MaxHoldersSeen = FMath::Max(MaxHoldersSeen, Minions->GetAttackTokens().NumHolders(AttackTargetId));
	}
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
	// The brains write the intents of the Brain scenario (UGSTN_MinionBrainSubsystem); the bench only reports
	if (Scenario == EGSTN_MinionBenchScenario::Brain)
	{
		return;
	}
	UWorld* World = GetWorld();
	UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(World);
	if (!Minions)
	{
		return;
	}
	if (Scenario == EGSTN_MinionBenchScenario::Attack && bAttackBrain)
	{
		return;
	}
	if (Scenario == EGSTN_MinionBenchScenario::Attack)
	{
		// Walk to 150 cm from the fighter, face it and ask for the swing every tick; range, cooldown and tokens decide when it starts
		const UGSTN_CombatWorldSubsystem* Combat = World->GetSubsystem<UGSTN_CombatWorldSubsystem>();
		const AActor* TargetActor = Combat ? Combat->FindOwnerActor(AttackTargetId) : nullptr;
		if (!TargetActor)
		{
			return;
		}
		const FVector TargetLocation = TargetActor->GetActorLocation();
		const FGameplayTag Swing = FGameplayTag::RequestGameplayTag(TEXT("Command.Minion.Attack.Swing"), false);
		const FGSTN_MinionStore& AttackStore = Minions->GetStore();
		Handles.RemoveAll([Minions](const FGSTN_MinionHandle& Handle) { return !Minions->IsValidHandle(Handle); });
		for (const FGSTN_MinionHandle& Handle : Handles)
		{
			const FVector Position(AttackStore.Position[Handle.Index]);
			const FVector2D ToTarget(TargetLocation.X - Position.X, TargetLocation.Y - Position.Y);
			const double Distance = ToTarget.Size();
			FGSTN_MinionIntent Intent;
			if (Distance > 150.0)
			{
				Intent.MoveDir = ToTarget / Distance;
				Intent.DesiredSpeed = 200.f;
			}
			Intent.DesiredYaw = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
			Intent.bFaceRequested = true;
			Intent.Command = Swing.IsValid() ? Swing : FGameplayTag(GSTNMinionTags::Type);
			Intent.TargetEntityId = AttackTargetId;
			Minions->SetIntent(Handle, Intent);
		}
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
