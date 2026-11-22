// Respawn, server side.

#pragma once

#include "CoreMinimal.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GameFramework/Info.h"
#include "GSTN_RespawnLogic.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/Interface.h"
#include "GSTN_RespawnSubsystem.generated.h"

class AGSTN_Checkpoint;
class AController;
class UGSTN_RespawnRules;

UINTERFACE(MinimalAPI, Blueprintable)
class UGSTN_PawnSpawnService : public UInterface
{
	GENERATED_BODY()
};

/**
 * How a game brings a player back (game-modes 8.1 IGSTN_PawnSpawnService). The game mode implements it when the default (destroy the old pawn,
 * AGameModeBase::RestartPlayerAtTransform) is not enough: a pawn class per kit, a drop pod, a spawn animation. Works the same with a classic game
 * mode and with Experiences (decision D5): whoever owns the game mode implements it.
 */
class GSTNRESPAWNRUNTIME_API IGSTN_PawnSpawnService
{
	GENERATED_BODY()

public:
	/** Server: bring Controller back at Where with a new pawn. Return false to let the default run */
	UFUNCTION(BlueprintNativeEvent, Category = "Respawn")
	bool RespawnPlayer(AController* Controller, const FTransform& Where);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSTN_OnRespawnStateChanged);

/** The respawn state every machine sees (lives, active checkpoint, restarts), for UI. Always relevant; written by the server's subsystem only */
UCLASS(NotPlaceable, Transient)
class GSTNRESPAWNRUNTIME_API AGSTN_RespawnStateInfo : public AInfo
{
	GENERATED_BODY()

public:
	AGSTN_RespawnStateInfo();

	/** The world's state actor, null before the server spawned it (or when respawn is off) */
	UFUNCTION(BlueprintPure, Category = "Respawn", meta = (WorldContext = "WorldContextObject"))
	static AGSTN_RespawnStateInfo* GetRespawnState(const UObject* WorldContextObject);

	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Respawn")
	int32 Lives = 0;

	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Respawn")
	int32 PoolSize = 0;

	/** Seconds to the next refilled life when it was last sent (1 Hz), -1 = none coming */
	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Respawn")
	float SecondsToNextLife = -1.f;

	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Respawn")
	TObjectPtr<AGSTN_Checkpoint> ActiveCheckpoint;

	/** Team restarts so far in this level */
	UPROPERTY(ReplicatedUsing = OnRep_State, BlueprintReadOnly, Category = "Respawn")
	int32 Restarts = 0;

	/** Any value above changed (also on the server when it writes them) */
	UPROPERTY(BlueprintAssignable, Category = "Respawn")
	FGSTN_OnRespawnStateChanged OnStateChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_State();
};

/**
 * The respawn framework of a match (M12 G3; game-modes 5): shared lives pool, team checkpoints, no revive, team restart when lives run out. Server
 * only and outside the simulation (game-modes 2.5): it listens to the authoritative KO event (Combat.Event.KO, published once by the host), so a
 * re-simulated KO is never counted, and it brings players back by spawning a new pawn, never by writing simulated state.
 *
 * Rules: UGSTN_RespawnRules from the level's GameRules asset, overridden by the gstn.Respawn.* variables (debug window). Everything it does is
 * published as Message.Respawn.* on the same event stream, so missions, UI and scoring react by tag without depending on this plugin.
 */
UCLASS()
class GSTNRESPAWNRUNTIME_API UGSTN_RespawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Respawn", meta = (WorldContext = "WorldContextObject"))
	static UGSTN_RespawnSubsystem* GetRespawnSubsystem(const UObject* WorldContextObject);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Whether the framework runs now (rules + gstn.Respawn.Enabled). Server */
	UFUNCTION(BlueprintPure, Category = "Respawn")
	bool IsRunning() const { return bRunning; }

	UFUNCTION(BlueprintPure, Category = "Respawn")
	int32 GetLives() const { return Pool.Lives; }

	/** Server: extra lives (a collectible, a reward), capped by the rules. Returns the lives added */
	UFUNCTION(BlueprintCallable, Category = "Respawn")
	int32 AddLives(int32 Count);

	/** Server: a checkpoint was touched by Player (the subsystem's own poll calls it; a scripted flow may too). Returns whether it became active */
	UFUNCTION(BlueprintCallable, Category = "Respawn")
	bool ActivateCheckpoint(AGSTN_Checkpoint* Checkpoint, AActor* Player);

	/** Server: the team restart now, as if the lives ran out (debug, scripted fail) */
	UFUNCTION(BlueprintCallable, Category = "Respawn")
	void ForceTeamRestart();

	/** Server: the active checkpoint, null = level start */
	UFUNCTION(BlueprintPure, Category = "Respawn")
	AGSTN_Checkpoint* GetActiveCheckpoint() const { return ActiveCheckpoint.Get(); }

	/** One line of state for the console and the on-screen debug */
	FString Describe() const;

	void RegisterCheckpoint(AGSTN_Checkpoint* Checkpoint);
	void UnregisterCheckpoint(AGSTN_Checkpoint* Checkpoint);

	/** The rules in force (the level's piece with the gstn.Respawn.* overrides applied); valid while running */
	const UGSTN_RespawnRules* GetEffectiveRules() const { return Effective; }

private:
	void Start();
	void Stop();
	void Poll();
	void HandleKO(const FGSTN_SimEvent& Event);
	void RespawnController(AController* Controller, bool bTeamRestart, int32 Index);
	FTransform ChooseTransform(AController* Controller, bool bTeamRestart, int32 Index, EGSTN_RespawnPlacement& OutPlacement) const;
	bool FindSafeTeammate(const AController* Except, FTransform& OutWhere) const;
	void DoTeamRestart();
	void PollPlayers();
	void Publish(const FGameplayTag& Type, AActor* Instigator, AActor* Target, float Magnitude) const;
	void SyncState();
	bool IsPending(const AController* Controller) const;
	bool HasAuthority() const;

	struct FPendingRespawn
	{
		TWeakObjectPtr<AController> Controller;
		double Time = 0.0;
	};
	TArray<FPendingRespawn> Pending;

	/** World time of the team restart, negative = none scheduled */
	double RestartAt = -1.0;

	/** Last location on safe ground (walkable floor, outside KO zones) of each living player, server-side positions */
	TMap<TWeakObjectPtr<AController>, FTransform> SafeGround;

	TArray<TWeakObjectPtr<AGSTN_Checkpoint>> Checkpoints;
	TWeakObjectPtr<AGSTN_Checkpoint> ActiveCheckpoint;
	TWeakObjectPtr<AGSTN_RespawnStateInfo> StateInfo;

	UPROPERTY(Transient)
	TObjectPtr<UGSTN_RespawnRules> Effective;

	FGSTN_LivesPool Pool;
	FGSTN_CheckpointProgress Progress;
	int32 Restarts = 0;
	bool bRunning = false;
	double LastPollTime = -1.0;
	double LastStateSyncTime = -1.0;
	FTimerHandle PollTimer;
	FGSTN_SimEventListenerHandle KOListener;
};
