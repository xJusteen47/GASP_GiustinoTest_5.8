// Minion runtime (authority batch, M8 G3).

#include "Debug/GSTN_MinionBench.h"

#include "Data/GSTN_MinionDefinition.h"
#include "Engine/Engine.h"
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
		TEXT("gstn.Minion.Bench.Spawn <N> <Idle|Chase>: spawns N bench minions in a ring around the first player (or the origin)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UGSTN_MinionBenchSubsystem* Bench = UGSTN_MinionBenchSubsystem::Get(World);
			if (!Bench)
			{
				UE_LOG(LogGSTNMinion, Warning, TEXT("gstn.Minion.Bench.Spawn: no bench in this world"));
				return;
			}
			const int32 Count = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 40;
			const EGSTN_MinionBenchScenario Scenario = Args.Num() > 1 && Args[1].Equals(TEXT("Chase"), ESearchCase::IgnoreCase)
				? EGSTN_MinionBenchScenario::Chase : EGSTN_MinionBenchScenario::Idle;
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

	FAutoConsoleCommandWithWorld BenchReportCommand(
		TEXT("gstn.Minion.Bench.Report"),
		TEXT("Logs one line: MinionBench alive=.. frames=.. avgBatchUs=.. maxBatchUs=.."),
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

int32 UGSTN_MinionBenchSubsystem::SpawnBench(int32 Count, EGSTN_MinionBenchScenario InScenario)
{
	UWorld* World = GetWorld();
	if (!UGSTN_MinionCombatSubsystem::GetActive(World))
	{
		UE_LOG(LogGSTNMinion, Warning, TEXT("Bench: the minion subsystem is not active here (client world?)"));
		return 0;
	}
	UGSTN_MinionDefinition* Definition = ResolveDefinition();
	Scenario = InScenario;
	BenchSeconds = 0.f;

	FVector Center = FVector::ZeroVector;
	FindNearestPlayer(World, FVector::ZeroVector, Center);
	Count = FMath::Max(Count, 0);
	// Ring: about 150 cm between neighbours, at least 600 cm out
	const double Radius = FMath::Max(600.0, Count * 150.0 / UE_TWO_PI);

	int32 Queued = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const double Angle = UE_TWO_PI * Index / FMath::Max(Count, 1);
		const FVector Location = Center + FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.0);
		const FRotator Facing = (Center - Location).Rotation();
		const FGSTN_MinionHandle Handle = UGSTN_MinionSpawner::Spawn(World, Definition, FTransform(FRotator(0.0, Facing.Yaw, 0.0), Location), FGSTN_MinionSpawnParams());
		if (!Handle.IsNull())
		{
			Handles.Add(Handle);
			++Queued;
		}
	}
	if (UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(World))
	{
		Minions->ResetBenchTotals();
	}
	UE_LOG(LogGSTNMinion, Log, TEXT("Bench: %d minions queued (%s), ring radius %.0f"), Queued,
		Scenario == EGSTN_MinionBenchScenario::Chase ? TEXT("Chase") : TEXT("Idle"), Radius);
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
	UE_LOG(LogGSTNMinion, Log, TEXT("MinionBench alive=%d frames=%d avgBatchUs=%.1f maxBatchUs=%.1f"),
		Minions->GetLastBatchStats().Alive, Totals.Batches, AvgUs, Totals.MaxBatchSeconds * 1.0e6);
}

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
	if (Scenario != EGSTN_MinionBenchScenario::Chase || Handles.IsEmpty())
	{
		return;
	}
	UWorld* World = GetWorld();
	UGSTN_MinionCombatSubsystem* Minions = UGSTN_MinionCombatSubsystem::GetActive(World);
	if (!Minions)
	{
		return;
	}
	// Stand-in brain: walk to the nearest player, stop 150 cm short. Runs in the world tick and only writes intents (latest wins)
	const FGSTN_MinionStore& Store = Minions->GetStore();
	Handles.RemoveAll([Minions](const FGSTN_MinionHandle& Handle) { return !Minions->IsValidHandle(Handle); });
	for (const FGSTN_MinionHandle& Handle : Handles)
	{
		const FVector Position(Store.Position[Handle.Index]);
		FGSTN_MinionIntent Intent;
		FVector Target;
		if (FindNearestPlayer(World, Position, Target))
		{
			const FVector2D ToTarget(Target.X - Position.X, Target.Y - Position.Y);
			const double Distance = ToTarget.Size();
			if (Distance > 150.0)
			{
				Intent.MoveDir = ToTarget / Distance;
				const UGSTN_MinionDefinition* Definition = Minions->GetDefinition(Store.TypeIndex[Handle.Index]);
				Intent.DesiredSpeed = Definition ? Definition->Stats.WalkSpeed : 200.f;
			}
		}
		Minions->SetIntent(Handle, Intent);
	}
}
