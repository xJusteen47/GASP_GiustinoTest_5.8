// Levels mode, server side: goal zones, collectibles, the replicated level state and the mode subsystem.

#pragma once

#include "CoreMinimal.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GameFramework/Info.h"
#include "GSTN_LevelsLogic.h"
#include "GSTN_LevelsRules.h"
#include "GSTN_LevelsTags.h"
#include "GSTN_MissionSubsystem.h"
#include "GSTN_ScoreRules.h"
#include "Subsystems/WorldSubsystem.h"
#include "GSTN_LevelsSubsystem.generated.h"

class SWidget;
class UGSTN_LevelSetDefinition;

UENUM(BlueprintType)
enum class EGSTN_LevelPhase : uint8
{
	/** The subsystem is off or the level has not started */
	Inactive,
	Playing,
	/** The goal finished: results on screen, travel to the next level pending */
	Completed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSTN_OnLevelStateChanged);

/**
 * The level state every machine sees: phase, grace countdown, collectibles, results. Always relevant; written by the server's subsystem only.
 * Also owns the placeholder results screen (Slate) on machines that have a screen.
 */
UCLASS(NotPlaceable, Transient)
class GSTNMODE_LEVELSRUNTIME_API AGSTN_LevelStateInfo : public AInfo
{
	GENERATED_BODY()

public:
	AGSTN_LevelStateInfo();

	/** The world's state actor, null before the server spawned it (or when Levels mode is off) */
	UFUNCTION(BlueprintPure, Category = "Levels", meta = (WorldContext = "WorldContextObject"))
	static AGSTN_LevelStateInfo* GetLevelState(const UObject* WorldContextObject);

	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Levels")
	EGSTN_LevelPhase Phase = EGSTN_LevelPhase::Inactive;

	/** Index in the level set, INDEX_NONE when the open map is not listed */
	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Levels")
	int32 LevelIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Levels")
	int32 NumLevels = 0;

	/** Server world time the goal's grace timer ends, negative = no timer running. Use the game state's server time to count down */
	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Levels")
	float GraceEndTime = -1.f;

	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Levels")
	int32 TeamCollected = 0;

	/** Valid when Phase is Completed */
	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Levels")
	FGSTN_LevelResults Results;

	/** Any value above changed (also on the server when it writes them) */
	UPROPERTY(BlueprintAssignable, Category = "Levels")
	FGSTN_OnLevelStateChanged OnStateChanged;

	/** Any machine: seconds left on the goal's grace timer (from the game state's server time), 0 when none runs */
	UFUNCTION(BlueprintPure, Category = "Levels")
	float GetGraceSecondsLeft() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Server: called after writing the fields; runs what OnRep_State runs on clients */
	void NotifyChanged();

	UFUNCTION()
	void OnRep_State();

private:
	void RefreshResultsScreen();
	void HideResultsScreen();

	TSharedPtr<SWidget> ResultsWidget;
};

/**
 * The goal of a level (game-modes 5, owner GM.5, 2026-11-22). It is an objective zone, so a Reach objective with the same ZoneTag still works;
 * what it adds is the grace rule: ONE alive player inside starts a timer, and when it runs out the level finishes even if the others have not
 * arrived (or at once when every alive player is inside). Server-side positions polled by UGSTN_LevelsSubsystem; no collision, nothing simulated.
 */
UCLASS(Blueprintable, meta = (DisplayName = "GSTN Levels Goal Zone"))
class GSTNMODE_LEVELSRUNTIME_API AGSTN_LevelGoalZone : public AGSTN_ObjectiveZone
{
	GENERATED_BODY()

public:
	AGSTN_LevelGoalZone();

	/** Seconds of grace for this zone; negative = the level's rules (GoalGraceSeconds) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Goal", meta = (ForceUnits = "s"))
	float GraceSecondsOverride = -1.f;

	UFUNCTION(BlueprintPure, Category = "Goal")
	EGSTN_GoalState GetGoalState() const { return Tracker.GetState(); }

	/** Server: one poll of this zone. Called by the Levels subsystem */
	FGSTN_GoalGraceResult EvaluateGoal(double Now, int32 AliveInside, int32 AliveTotal, const FGSTN_GoalGraceParams& RulesParams);

	/** Server: the grace timer starts over (the team restarts) */
	void ResetGoal() { Tracker.Reset(); }

	double GetGraceEnd() const { return Tracker.GetGraceEnd(); }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	FGSTN_GoalGraceTracker Tracker;
};

/**
 * A Levels collectible (Mario 3D World stars, stamps): the mission framework's collectible with Levels.Collectible as its default identity.
 * Taken once on the server by the first player in reach, it publishes Message.Mission.Collected; the Levels subsystem counts it for the team and
 * the level (v1: no save, per owner GM.3/GM.5). Needs the missions piece enabled, because the mission subsystem polls the take.
 */
UCLASS(Blueprintable, meta = (DisplayName = "GSTN Levels Collectible"))
class GSTNMODE_LEVELSRUNTIME_API AGSTN_LevelCollectible : public AGSTN_Collectible
{
	GENERATED_BODY()

public:
	AGSTN_LevelCollectible();

	virtual void BeginPlay() override;
};

/**
 * Levels mode (M12 G6): runs the level when the Levels piece of the game rules is enabled. Server only and outside the simulation (game-modes 2.5):
 * it listens to the authoritative event stream (so a re-simulated event is never counted), counts collectibles and score counters, polls the goal
 * zones, completes the level, computes the results and travels to the next map of the level set. Lives and checkpoints are the respawn framework's
 * and objectives the mission framework's; this subsystem only reads their events and never calls them (game-modes 2.1: siblings meet on the stream).
 */
UCLASS()
class GSTNMODE_LEVELSRUNTIME_API UGSTN_LevelsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Levels", meta = (WorldContext = "WorldContextObject"))
	static UGSTN_LevelsSubsystem* GetLevelsSubsystem(const UObject* WorldContextObject);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Levels")
	bool IsRunning() const { return bRunning; }

	UFUNCTION(BlueprintPure, Category = "Levels")
	EGSTN_LevelPhase GetPhase() const { return Phase; }

	/** Friendly fire as it applies now: gstn.Levels.FriendlyFire (-1 = the rules), else the Levels piece (ON by default, GM.13) */
	UFUNCTION(BlueprintPure, Category = "Levels")
	bool GetEffectiveFriendlyFire() const;

	/** Difficulty for the players connected now (GM.2); identity when Levels mode is off */
	UFUNCTION(BlueprintPure, Category = "Levels")
	FGSTN_PlayerCountScaling GetPlayerScaling() const;

	/** Server: complete the level now as if a goal finished (debug, scripted flows). Returns false when it is not Playing */
	UFUNCTION(BlueprintCallable, Category = "Levels")
	bool CompleteLevel();

	/** Server: travel to the next level (or the return map) now, without waiting for the results timer */
	UFUNCTION(BlueprintCallable, Category = "Levels")
	bool TravelToNextLevel();

	const FGSTN_LevelTally& GetTally() const { return Tally; }
	const FGSTN_LevelResults& GetResults() const { return Results; }

	/** One line of state for the console */
	FString Describe() const;

	void RegisterGoalZone(AGSTN_LevelGoalZone* Zone) { GoalZones.AddUnique(Zone); }
	void UnregisterGoalZone(AGSTN_LevelGoalZone* Zone) { GoalZones.Remove(Zone); }

private:
	void Start();
	void Stop();
	void Poll();
	void HandleEvent(const FGSTN_SimEvent& Event);
	void PublishEvent(const FGameplayTag& Type, AActor* Instigator, float Magnitude) const;
	void SyncState();
	bool HasAuthority() const;
	static int32 PlayerKeyOf(const AActor* Actor);

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_LevelSetDefinition> LevelSet;

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_ScoreRules> Score;

	TWeakObjectPtr<AGSTN_LevelStateInfo> StateInfo;
	TArray<TWeakObjectPtr<AGSTN_LevelGoalZone>> GoalZones;

	/** Players knocked out and not back yet (DownEvents / UpEvents): not alive for the goal */
	TSet<TWeakObjectPtr<AActor>> Downed;

	FGSTN_LevelTally Tally;
	FGSTN_LevelResults Results;
	EGSTN_LevelPhase Phase = EGSTN_LevelPhase::Inactive;
	int32 LevelIndex = INDEX_NONE;
	int32 PlayerCount = 0;
	double StartTime = 0.0;
	double GraceEnd = -1.0;
	bool bRunning = false;
	FTimerHandle PollTimer;
	FTimerHandle TravelTimer;
	FGSTN_SimEventListenerHandle Listener;
};
