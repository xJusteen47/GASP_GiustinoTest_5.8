// Minion runtime (authority batch, M8 G3).

#pragma once

#include "CoreMinimal.h"
#include "Store/GSTN_MinionStore.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_MinionBench.generated.h"

class UGSTN_MinionDefinition;

/** What the bench minions do (gstn.Minion.Bench.Spawn <N> <Scenario>) */
enum class EGSTN_MinionBenchScenario : uint8
{
	/** Zero intent: measures the batch floor (schedule, LOD, publish) */
	Idle,
	/** Every world tick the bench writes an intent toward the nearest player, standing in for the AI (G8) */
	Chase
};

/**
 * The bench of M8 (spike tooling, combat-m8-minions.md 4 "Debug"): spawns N minions of one type in a ring and drives them with a
 * trivial brain, so `stat GSTNMinion` and gstn.Minion.Bench.Report measure the batch alone. Console commands:
 *   gstn.Minion.Bench.Spawn <N> <Idle|Chase>   gstn.Minion.Bench.Clear   gstn.Minion.Bench.Report
 * The type is the asset at gstn.Minion.Bench.Definition, or a transient definition built in code (no content needed).
 *
 * The bench writes intents from the world tick, outside the fixed tick, exactly like the brain will: latest wins, and the batch
 * reads whatever is there when it runs. It does nothing on a client (the minion subsystem is inactive there).
 */
UCLASS()
class GSTNMINIONRUNTIME_API UGSTN_MinionBenchSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UGSTN_MinionBenchSubsystem* Get(const UWorld* World);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Spawns Count minions (queued: they activate over the next frames) and sets the scenario. Returns how many were queued */
	int32 SpawnBench(int32 Count, EGSTN_MinionBenchScenario InScenario);

	/** Despawns every bench minion */
	void Clear();

	/** Logs `MinionBench alive=.. frames=.. avgBatchUs=.. maxBatchUs=..` (LogGSTNMinion) */
	void Report() const;

private:
	UGSTN_MinionDefinition* ResolveDefinition();

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_MinionDefinition> TransientDefinition;

	TArray<FGSTN_MinionHandle> Handles;
	EGSTN_MinionBenchScenario Scenario = EGSTN_MinionBenchScenario::Idle;

	/** Seconds since the last bench spawn, for gstn.Minion.Bench.AutoReportSeconds; negative once reported (or before any spawn) */
	float BenchSeconds = -1.f;
};
