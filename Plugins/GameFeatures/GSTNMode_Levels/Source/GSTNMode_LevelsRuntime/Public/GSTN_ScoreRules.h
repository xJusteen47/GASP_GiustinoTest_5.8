// Levels mode: data-driven score.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "GSTN_ScoreRules.generated.h"

/**
 * One counter of the score (game-modes 5 "Scoring"): the authoritative events that match EventQuery are counted, and each one is worth
 * PointsPerEvent. Kills, collectibles, deaths: all are rows, none is code.
 */
USTRUCT(BlueprintType)
struct GSTNMODE_LEVELSRUNTIME_API FGSTN_ScoreCounterRule
{
	GENERATED_BODY()

	/** Key of the counter (Collectibles, Kills, Deaths...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	FName CounterId;

	/** Shown on the results screen; empty = the id */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	FText Label;

	/** Matched against the event's type and tags (the stream's rule). An empty query counts nothing here (unlike the stream's listeners) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	FGameplayTagQuery EventQuery;

	/** Points per counted unit; negative is allowed (a death) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	int32 PointsPerEvent = 0;

	/** Each event counts its Magnitude (rounded, at least 1) instead of 1: a collectible worth 5 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	bool bUseMagnitude = false;

	/** The count starts again when the team restarts the level (collectibles come back). Off for deaths: a restart does not erase them */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	bool bResetOnLevelRestart = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	bool bShowInResults = true;
};

/** A rank reached from MinScore up (S, A, B...) */
USTRUCT(BlueprintType)
struct GSTNMODE_LEVELSRUNTIME_API FGSTN_ScoreRank
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	FName RankId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	int32 MinScore = 0;
};

/** One line of the results screen */
USTRUCT(BlueprintType)
struct GSTNMODE_LEVELSRUNTIME_API FGSTN_ResultLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 Count = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 Points = 0;
};

/** What the results screen shows: computed once on the server when the level completes, replicated as a whole (a few dozen bytes) */
USTRUCT(BlueprintType)
struct GSTNMODE_LEVELSRUNTIME_API FGSTN_LevelResults
{
	GENERATED_BODY()

	/** Index in the level set, INDEX_NONE for a map the set does not list */
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 LevelIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	FName LevelId;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	float ElapsedSeconds = 0.f;

	/** Collectibles the team took in this level (items, not their value) */
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 TeamCollected = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	TArray<FGSTN_ResultLine> Lines;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 CounterPoints = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 TimeBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 CompletionBonus = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	int32 TotalScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	FName RankId;

	UPROPERTY(BlueprintReadOnly, Category = "Score")
	FText RankLabel;

	/** No next level to travel to */
	UPROPERTY(BlueprintReadOnly, Category = "Score")
	bool bFinalLevel = false;
};

/**
 * How a level is scored (owner GM.5, 2026-11-22: simple data-driven score). One asset per level or per set; the level's rules piece names it.
 * Score = sum of counter points + time bonus + completion bonus, never below zero. Server only; nothing here is read by the simulation.
 */
UCLASS(BlueprintType)
class GSTNMODE_LEVELSRUNTIME_API UGSTN_ScoreRules : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	TArray<FGSTN_ScoreCounterRule> Counters;

	/** Finishing faster than this earns PointsPerSecondUnderPar for each second saved. 0 = no time bonus */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score|Time", meta = (ClampMin = "0", ForceUnits = "s"))
	float ParTimeSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score|Time", meta = (ClampMin = "0"))
	int32 PointsPerSecondUnderPar = 0;

	/** Added when the level completes (a failed or abandoned level scores no bonus) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	int32 CompletionBonus = 0;

	/** In any order; the rank is the one with the highest MinScore the total reaches. None reached = no rank */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Score")
	TArray<FGSTN_ScoreRank> Ranks;

	/** The rank for Score, null when none is reached */
	const FGSTN_ScoreRank* FindRank(int32 Score) const;

	/** Collectibles, kills (a KO that carries a Minion.Type tag) and deaths, plus a time bonus, a completion bonus and four ranks. TEST VALUES */
	static UGSTN_ScoreRules* MakeDefault(UObject* Outer);

	virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(TEXT("GSTN_ScoreRules"), GetFName()); }
};
