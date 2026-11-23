// Respawn, pure core (no GAS, no Network Prediction, no world).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameRules/GSTN_GameRules.h"
#include "GSTN_RespawnRules.generated.h"

/** What happens when the shared lives pool runs out (owner GM.4, 2026-10-23) */
UENUM(BlueprintType)
enum class EGSTN_OnLivesExhausted : uint8
{
	/** Everyone comes back at the last activated checkpoint (the level start when none is active) and the pool refills */
	RestartFromCheckpoint,
	/** The checkpoints are forgotten and everyone comes back at the level start; with bReloadMapOnRestartLevel the map reloads instead */
	RestartLevel
};

/** Where a fallen player comes back when no checkpoint is active, or when the rules do not use checkpoints */
UENUM(BlueprintType)
enum class EGSTN_RespawnFallbackPlacement : uint8
{
	/** Near a living teammate, on the last safe ground the server saw them on; the level start when nobody qualifies (game-modes 5 proposal) */
	NearLivingTeammate,
	/** Always the level start (the game mode's player start) */
	LevelStart
};

/** How the fallen player comes back */
UENUM(BlueprintType)
enum class EGSTN_RespawnMethod : uint8
{
	/**
	 * A new pawn (a fresh Network Prediction entity) through the game mode, or through IGSTN_PawnSpawnService when the game mode implements it.
	 * The spec's method (game-modes 2.5: a respawn is a spawn of a new pawn, a framework never writes simulated state)
	 */
	NewPawn,
	/**
	 * The same pawn: teleported, then made Actionable again with UGSTN_CombatSimComponent::ServerRespawn (what the test harness does). Kept for
	 * test maps whose pawn cannot be restarted by the game mode; a teleport from outside the simulation is corrected by the next server state
	 */
	InPlace
};

/** A KO reason's own numbers (KO by Health or by Zone, game-modes 5: same rule, data by reason) */
USTRUCT(BlueprintType)
struct GSTNRESPAWNCORE_API FGSTN_KOReasonRespawn
{
	GENERATED_BODY()

	/** Lives this KO takes from the pool. 0 = a free KO (e.g. a tutorial pit) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "0", ClampMax = "10"))
	int32 LifeCost = 1;

	/** Seconds before the player comes back; negative = the rules' RespawnDelaySeconds */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Respawn", meta = (ForceUnits = "s"))
	float DelaySeconds = -1.f;
};

/**
 * The respawn piece of the game rules (M12 G3; game-modes 5 "Checkpoints", owner GM.4 and GM.9: shared lives, no revive). One per GameRules asset,
 * chosen by the server per level or mode (UGSTN_GameRulesSubsystem), so a level is balanced in data: e.g. 20 lives and no checkpoints, or 4 lives
 * that refill fast with checkpoints. The class defaults are the plan's defaults (m12-modi-livelli 6) with bEnabled off, so a map without an asset
 * plays as before (the harness and the hub keep their own KO handling). Server only: nothing here is read by the simulation.
 *
 * The kit drop on death is NOT here: it is the held objects' piece (UGSTN_HeldRules::KitSwap.DeathDrop, M11), which already drops the kit on the
 * KO event; one owner per rule. Set it in the same GameRules asset.
 */
UCLASS(config = GSTNRespawn, meta = (DisplayName = "Respawn and lives"))
class GSTNRESPAWNCORE_API UGSTN_RespawnRules : public UGSTN_GameRulesPiece
{
	GENERATED_BODY()

public:
	/** The active rules' piece, else the defaults */
	static const UGSTN_RespawnRules& Get() { return FGSTN_ActiveGameRules::GetPiece<UGSTN_RespawnRules>(); }

	/** The respawn framework runs (KO = a life from the pool, the player comes back). Off = nothing happens on a KO (today's behaviour) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Respawn")
	bool bEnabled = false;

	/** Lives of the team's shared pool at the start and after a restart. TEST VALUE 4 (plan default) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Lives", meta = (ClampMin = "0", ClampMax = "999"))
	int32 LivesPoolSize = 4;

	/** Seconds to get one life back while the pool is below LivesPoolSize; 0 = no refill. TEST VALUE 20 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Lives", meta = (ClampMin = "0", ForceUnits = "s"))
	float LifeRegenSeconds = 20.f;

	/** Most lives the pool may hold with extra lives (collectibles, AddLives); below LivesPoolSize = LivesPoolSize */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Lives", meta = (ClampMin = "0", ClampMax = "999"))
	int32 MaxLivesWithExtras = 9;

	/**
	 * Owner's example read as "the restart happens when a KO takes the pool to zero" (game-modes 5, reading). Off = the KO that empties the pool
	 * still respawns the player, and the restart happens at the next KO with the pool already empty. Both kept as data
	 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Lives")
	bool bRestartWhenPoolReachesZero = true;

	/** Activated checkpoints are where the team comes back. Off = no checkpoint is ever activated (e.g. the 20-lives level) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Checkpoints")
	bool bUseCheckpoints = true;

	/** A checkpoint with a lower order than the active one does not take over (walking back does not move the respawn point backwards) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Checkpoints")
	bool bCheckpointsOnlyForward = true;

	/** What happens when the pool runs out */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Lives")
	EGSTN_OnLivesExhausted OnLivesExhausted = EGSTN_OnLivesExhausted::RestartFromCheckpoint;

	/** RestartLevel reloads the map (server travel) instead of moving everyone to the start. Resets everything in the level */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Lives")
	bool bReloadMapOnRestartLevel = false;

	/** Seconds between the KO that empties the pool and the team restart (the death presentation plays first) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Lives", meta = (ClampMin = "0", ForceUnits = "s"))
	float RestartDelaySeconds = 3.f;

	/** Seconds between a KO and the player's return. TEST VALUE 2 (plan default) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "0", ForceUnits = "s"))
	float RespawnDelaySeconds = 2.f;

	/**
	 * Invulnerable frames after a respawn. TEST VALUE 60 (plan default). DATA ONLY for now: a framework may not write simulated state, and the
	 * combat inbox has no "apply effect" entry yet (game-modes 2.5 unsure); the new pawn's own data will apply it (M12 G3 part 2)
	 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "0", ClampMax = "600"))
	int32 InvulnerableFrames = 60;

	/** Where a player comes back when no checkpoint is active */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Respawn")
	EGSTN_RespawnFallbackPlacement FallbackPlacement = EGSTN_RespawnFallbackPlacement::NearLivingTeammate;

	/** How the player comes back */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Respawn")
	EGSTN_RespawnMethod Method = EGSTN_RespawnMethod::NewPawn;

	/** Per KO reason (Combat.KOReason.Health, Combat.KOReason.Zone): cost and delay. A reason not listed costs 1 life and uses RespawnDelaySeconds */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Respawn", meta = (Categories = "Combat.KOReason"))
	TMap<FGameplayTag, FGSTN_KOReasonRespawn> PerReason;

	/** Server poll rate of checkpoints and safe ground, outside the simulation */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Server", meta = (ClampMin = "1", ClampMax = "30", ForceUnits = "Hz"))
	float PollHz = 10.f;

	/** The reason's numbers (defaults when not listed) */
	FGSTN_KOReasonRespawn GetReason(const FGameplayTagContainer& EventTags) const;
};
