// Levels mode: the mode's piece of the game rules.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameRules/GSTN_GameRules.h"
#include "GSTN_LevelsRules.generated.h"

class UGSTN_LevelSetDefinition;
class UGSTN_ScoreRules;

/** How difficulty follows the number of players (owner GM.2, 2026-11-22: one to four, also alone, scaling is data to tune) */
USTRUCT(BlueprintType)
struct GSTNMODE_LEVELSRUNTIME_API FGSTN_PlayerCountScaling
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scaling", meta = (ClampMin = "1", ClampMax = "16"))
	int32 PlayerCount = 1;

	/** Multiplies the number of enemies a spawn pattern asks for (the director reads it) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scaling", meta = (ClampMin = "0.1"))
	float EnemyCountScale = 1.f;

	/** Multiplies enemy health attributes (applied by the spawner's data, never written into a running simulation) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scaling", meta = (ClampMin = "0.1"))
	float EnemyHealthScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scaling", meta = (ClampMin = "0.1"))
	float EnemyDamageScale = 1.f;
};

/**
 * The Levels piece of the GameRules asset of a level or set (GR-1). Its siblings in the same asset are the respawn piece (shared lives and
 * checkpoints) and the missions piece, so one asset says everything about a level. The class defaults are the owner's answers of 2026-11-22
 * with bEnabled off, so a map without the piece plays as before. Server only: the simulation reads none of it except friendly fire, which is
 * applied to the combat rules before pawns exist (GSTNLevelsLogic::ApplyFriendlyFire).
 */
UCLASS(config = GSTNMode_Levels, meta = (DisplayName = "Levels mode"))
class GSTNMODE_LEVELSRUNTIME_API UGSTN_LevelsRules : public UGSTN_GameRulesPiece
{
	GENERATED_BODY()

public:
	UGSTN_LevelsRules();

	static const UGSTN_LevelsRules& Get() { return FGSTN_ActiveGameRules::GetPiece<UGSTN_LevelsRules>(); }

	/** The Levels subsystem runs (goal, score, results, travel). Off = nothing happens (gstn.Levels.Enabled can force it) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Levels")
	bool bEnabled = false;

	/** The levels in play order; the map that is open is looked up in it. Empty = a single stand-alone level */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Levels")
	TSoftObjectPtr<UGSTN_LevelSetDefinition> LevelSet;

	/** Empty = UGSTN_ScoreRules::MakeDefault */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Levels")
	TSoftObjectPtr<UGSTN_ScoreRules> ScoreRules;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Players", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MinPlayers = 1;

	/** GM.2: up to four. The game mode's player limit is the real gate; this is the number the scaling table and the warnings use */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Players", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxPlayers = 4;

	/** Difficulty by player count; the entry with the highest PlayerCount not above the real count applies (one entry = a flat rule). TEST VALUES */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Players")
	TArray<FGSTN_PlayerCountScaling> Scaling;

	/**
	 * GM.13: friendly fire ON in Levels. Applied to the combat rules by GSTNLevelsLogic::ApplyFriendlyFire before the first pawn simulates (it is a
	 * simulation toggle, game-modes 2.3), so it never changes inside a level
	 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Combat")
	bool bFriendlyFire = true;

	/**
	 * GM.5: seconds between the first alive player entering a goal zone and the level ending, whether or not the others arrived. 0 = the level ends
	 * at once. A zone can override it. TEST VALUE 10
	 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Goal", meta = (ClampMin = "0", ForceUnits = "s"))
	float GoalGraceSeconds = 10.f;

	/** Every alive player inside ends the level before the timer does (no reason to wait for a team that is all there) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Goal")
	bool bGoalFinishEarlyWhenAllInside = true;

	/** The timer stops and starts again if nobody is left inside. Off (GM.5): once started it runs out */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Goal")
	bool bGoalCancelWhenNobodyInside = false;

	/** The results screen stays this long, then the team travels to the next level. TEST VALUE 10 */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Results", meta = (ClampMin = "0", ForceUnits = "s"))
	float ResultsSeconds = 10.f;

	/** Travel with seamless travel (the project needs a transition map); off = a hard server travel */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Results")
	bool bSeamlessTravel = true;

	/** Events whose Target is a player that is down (not alive for the goal), and events that bring the Target back. Tags only */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Events")
	FGameplayTagContainer DownEvents;

	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Events")
	FGameplayTagContainer UpEvents;

	/** Events that restart the level's tally (the respawn framework's RestartLevel: collectibles come back) */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Events")
	FGameplayTagContainer RestartEvents;

	/** Server poll of the goal zones, outside the simulation */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Server", meta = (ClampMin = "1", ClampMax = "30", ForceUnits = "Hz"))
	float PollHz = 4.f;

	/** The scaling for PlayerCount players; identity when the table is empty */
	UFUNCTION(BlueprintPure, Category = "Levels")
	FGSTN_PlayerCountScaling GetScaling(int32 PlayerCount) const;
};
