// Minion runtime (authority batch, M8 G3; spike 3 scenario, course and robustness counters G4).

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Store/GSTN_MinionStore.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_MinionBench.generated.h"

class AActor;
class AGSTN_MinionSpawnerActor;
class UGSTN_MinionCombatSubsystem;
class UGSTN_MinionDefinition;
class UGSTN_MinionWaveDefinition;
class UPrimitiveComponent;

/** What the bench minions do (gstn.Minion.Bench.Spawn <N> <Scenario>) */
enum class EGSTN_MinionBenchScenario : uint8
{
	/** Zero intent: measures the batch floor (schedule, LOD, publish) */
	Idle,
	/**
	 * Every world tick the bench writes an intent toward the nearest player (the bench centre when there is none), standing in for
	 * the AI (G8). Minions stop at half the ring radius, so a crowd does not jam into a "stuck" pile at the target
	 */
	Chase,
	/**
	 * Chase plus random knockback (spike 3): every gstn.Minion.Bench.LaunchEveryFrames fixed frames a seeded random subset of the
	 * minions gets a launch of up to gstn.Minion.Bench.LaunchSpeed in an upward-biased direction, or a push back
	 */
	Massacre,
	/**
	 * G5 gate: minions with a sword-sized swing walk up to a fighter and attack it again and again (gstn.Minion.Bench.Attack), so the
	 * harness sees minion hits arrive in a real fighter's inbox and the attack tokens cap the simultaneous attackers
	 */
	Attack,
	/**
	 * G8: the minions think for themselves. Each gets the test StateTree (the native brain when the editor module is not loaded or
	 * gstn.Minion.Brain.Native is 1) from UGSTN_MinionBrainSubsystem and chases the nearest fighter; the bench writes no intent
	 */
	Brain
};

/**
 * The bench of M8 (spike tooling, combat-m8-minions.md 4 "Debug"): spawns N minions of one type in a ring and drives them with a
 * trivial brain, so `stat GSTNMinion` and gstn.Minion.Bench.Report measure the batch alone. Console commands:
 *   gstn.Minion.Bench.Spawn <N> <Idle|Chase|Massacre|Brain>   gstn.Minion.Bench.Clear   gstn.Minion.Bench.Report
 *   gstn.Minion.Bench.Attack <N> <FighterIndex> [Brain]   gstn.Minion.Bench.Poke <BenchIndex>
 *   gstn.Minion.Bench.Wave <N> <CorpseSeconds=1>   gstn.Minion.Bench.Kill   (G9a: a transient spawner plays a wave of N bench grunts at
 *   the first player; Kill sends a lethal hit to every live bench and wave minion so they play the death timeline and the corpse time)
 *   gstn.Minion.Bench.Course <1|0>   builds (or removes) the hostile test course of spike 3 around the bench centre, in code:
 *   engine cube actors (no /Game asset) for a floor, stairs (30 cm steps), a 40 degree ramp onto a 300 cm ledge, a thin wall
 *   (10 cm) and a 90 degree corner.
 *
 * Robustness counters (spike 3, read from the subsystem's end-of-movement hook, so per fixed frame): fell out of the world
 * (KillZ, plus 10 m below the course floor when a course exists), stuck = trapped (the capsule overlaps blocking geometry after
 * its step, or moves less than 1 cm over 125 frames while airborne or knocked back), blocked (the same while walking with intent:
 * a wall in a straight-line chaser's way, expected without navigation), tunnelled (the centre's segment of one frame crosses a
 * course box, which a correct sweep never allows). Spawns are dropped onto the geometry under the ring, never inside it.
 * Launches are drawn in the same hook, in slot order, from a seeded stream held here: the same run launches the same minions.
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

	/** G5 gate: Count attackers in a ring (150 cm) around the TargetIndex-th registered fighter (ascending id), attacking it. Returns how many were queued */
	int32 SpawnAttackers(int32 Count, int32 TargetIndex, bool bBrain = false);

	/**
	 * G8 test tooling: posts one synthetic 1 damage hit on the BenchIndex-th minion of this bench through PostToEntityInbox, as a fighter's
	 * hit would arrive. It is the only way to hit a Dormant minion (the hit grid holds no Far or Dormant minion). Returns false for a bad index
	 */
	bool Poke(int32 BenchIndex);

	/**
	 * G9a: spawns a transient AGSTN_MinionSpawnerActor at the first player with a transient wave of Count bench grunts (the bench
	 * definition, whose CorpseSeconds is set to the argument) and starts it. The previous bench spawner is destroyed first. Logs
	 * `Bench: wave cleared frame=..` when the wave clears. Returns the number of minions the wave will spawn (0 when it did not start)
	 */
	int32 SpawnWave(int32 Count, float CorpseSeconds);

	/**
	 * G9a: posts a lethal synthetic hit (neutral attacker) on every live bench and wave minion, as Poke does for one, so they die through
	 * the normal damage path. Returns how many were posted
	 */
	int32 Kill();

	/** Despawns every bench minion */
	void Clear();

	/**
	 * Logs `MinionBench alive=.. frames=.. avgBatchUs=.. maxBatchUs=.. moveUsPerMinionFrame=.. fell=.. stuck=.. blocked=.. tunnelled=..`
	 * (LogGSTNMinion)
	 */
	void Report() const;

	/** Builds the test course around the bench centre (true) or removes it (false) */
	void SetCourse(bool bEnable);

	virtual void Deinitialize() override;

private:
	UGSTN_MinionDefinition* ResolveDefinition();
	/** Gives every bench minion without one the test tree (or the native brain when there is none) */
	void StartBenchBrains(UWorld* World);
	/** Nearest player or the origin, once; the course and the ring share it */
	FVector ResolveCenter();
	void AddCourseBox(const FVector& Offset, const FVector& Size, const FRotator& Rotation);
	void BindMovementHook(UGSTN_MinionCombatSubsystem* Minions);
	void HandleMovementStepped(int32 Frame, TConstArrayView<int32> Slots, TConstArrayView<FVector3f> PreviousPositionBySlot);
	void ResetCounters(int32 Capacity);
	void CountTrapped(int32 Slot, const FVector& Feet, const TCHAR* ModeName, const TCHAR* Reason, int32 Frame);

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_MinionDefinition> TransientDefinition;

	/** The attackers of the Attack scenario: one move (a 40 frame swing, hitbox over frames 4 to 9) and no hurtboxes */
	UPROPERTY(Transient)
	TObjectPtr<UGSTN_MinionDefinition> AttackDefinition;
	UPROPERTY(Transient)
	TObjectPtr<UObject> AttackTimeline;
	int32 AttackTargetId = INDEX_NONE;
	/** True when the attackers think with the brain (Attack <N> <Fighter> Brain): the bench writes no intents and pins the brain's target */
	bool bAttackBrain = false;
	/** Highest number of tokens held against the target on any fixed frame since the spawn */
	int32 MaxHoldersSeen = 0;

	/** G9a: the transient spawner of gstn.Minion.Bench.Wave and its wave */
	UPROPERTY(Transient)
	TObjectPtr<AGSTN_MinionSpawnerActor> WaveSpawner;
	UPROPERTY(Transient)
	TObjectPtr<UGSTN_MinionWaveDefinition> WaveDefinition;

	TArray<FGSTN_MinionHandle> Handles;
	EGSTN_MinionBenchScenario Scenario = EGSTN_MinionBenchScenario::Idle;

	FVector Center = FVector::ZeroVector;
	bool bCenterResolved = false;
	double RingRadius = 600.0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> CourseActors;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> CourseBoxes;

	TWeakObjectPtr<UGSTN_MinionCombatSubsystem> HookedMinions;
	FDelegateHandle MovementHookHandle;

	/** Massacre launches (seeded from gstn.Minion.Bench.Seed at each spawn) */
	FRandomStream LaunchStream;

	// Robustness counters and their per-slot state (store capacity entries)
	int32 FellBelowCourse = 0;
	/** Trapped minions (reported as stuck=): in penetration after a step, or not moving for 2 s while airborne or knocked back */
	int32 Stuck = 0;
	/** Walking minions held by a wall for 2 s with intent (reported as blocked=; expected without navigation) */
	int32 Blocked = 0;
	int32 Tunnelled = 0;
	TArray<uint16> SeenGeneration;
	TArray<FVector3f> StuckAnchor;
	TArray<int32> StuckFrames;
	TArray<uint8> SlotFlags;

	/** Seconds since the last bench spawn, for gstn.Minion.Bench.AutoReportSeconds; negative once reported (or before any spawn) */
	float BenchSeconds = -1.f;
};
