// Levels mode: the rules as plain C++ (no world, no actors), so the runtime and the tests share them.

#pragma once

#include "CoreMinimal.h"
#include "Events/GSTN_SimEvent.h"
#include "GSTN_ScoreRules.h"
#include "GSTN_LevelsLogic.generated.h"

class UGSTN_CombatRules;
class UGSTN_GameRules;
class UGSTN_LevelsRules;
class UGSTN_LevelSetDefinition;

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Goal zone

UENUM(BlueprintType)
enum class EGSTN_GoalState : uint8
{
	/** Nobody alive has entered yet */
	Idle,
	/** The grace timer runs */
	Grace,
	/** Finished; latched until Reset */
	Completed
};

struct FGSTN_GoalGraceParams
{
	/** Seconds from the first alive player's entry to the end of the level. 0 = at once */
	float GraceSeconds = 10.f;

	/** Every alive player inside ends it before the timer does */
	bool bFinishEarlyWhenAllInside = true;

	/** Nobody left inside stops the timer and starts it over. Off (GM.5): once started it runs out */
	bool bCancelWhenNobodyInside = false;
};

struct FGSTN_GoalGraceResult
{
	bool bGraceStarted = false;
	bool bCompleted = false;
};

/**
 * The goal rule of owner decision GM.5 (2026-11-22): ONE alive player in the zone starts a grace timer, and when it runs out the level finishes
 * even if the others did not arrive. Fed by the server's poll with counts (alive players inside this zone, alive players in all), so it knows
 * nothing about actors and a test drives it with numbers. Time is the server's world time, never frames: nothing here is simulated.
 */
class GSTNMODE_LEVELSRUNTIME_API FGSTN_GoalGraceTracker
{
public:
	/** One poll. Each of the two results is true on exactly one call (completion latches) */
	FGSTN_GoalGraceResult Update(double Now, int32 AliveInside, int32 AliveTotal, const FGSTN_GoalGraceParams& Params);

	void Reset();

	EGSTN_GoalState GetState() const { return State; }

	/** World time the timer ends, negative when it does not run */
	double GetGraceEnd() const { return State == EGSTN_GoalState::Grace ? GraceEnd : -1.0; }

	/** Seconds left on the timer (0 when it does not run) */
	float GetSecondsLeft(double Now) const;

private:
	EGSTN_GoalState State = EGSTN_GoalState::Idle;
	double GraceEnd = -1.0;
};

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Tally and score

/** What the team did in this level, counted from the authoritative event stream (each event once: the stream never replays a re-simulation) */
struct GSTNMODE_LEVELSRUNTIME_API FGSTN_LevelTally
{
	/** By counter id of the score rules: events counted, or their magnitude for counters with bUseMagnitude */
	TMap<FName, int32> Counts;

	/** Collectibles the team took in this level (GM.5: per level and per team) */
	int32 TeamCollected = 0;

	/** The sum of the Value of those collectibles */
	int32 TeamCollectedValue = 0;

	/** Collectibles by player key (the player id; INDEX_NONE for a taker that is not a player) */
	TMap<int32, int32> CollectedByPlayer;

	void Reset();

	/** The level restarts: collectibles come back and the counters that say so start again (deaths stay); Rules may be null */
	void ResetForRestart(const UGSTN_ScoreRules* Rules);

	/**
	 * Counts one event: the team's collectibles when it is a Message.Mission.Collected (PlayerKey = who took it), and every counter of Rules whose
	 * query matches. Rules may be null (collectibles only). Returns whether anything changed
	 */
	bool AddEvent(const UGSTN_ScoreRules* Rules, const FGSTN_SimEvent& Event, int32 PlayerKey);
};

namespace GSTNLevelsLogic
{
	/** Counters, time bonus, completion bonus, rank. bCompleted false (an abandoned level) leaves out both bonuses. LevelIndex, LevelId and bFinalLevel stay for the caller */
	GSTNMODE_LEVELSRUNTIME_API FGSTN_LevelResults ComputeResults(const UGSTN_ScoreRules& Rules, const FGSTN_LevelTally& Tally, float ElapsedSeconds, bool bCompleted);

	/** GM.13: writes the piece's friendly-fire value into a combat rules asset (call it on a transient copy, never on the shared asset) */
	GSTNMODE_LEVELSRUNTIME_API void ApplyFriendlyFire(const UGSTN_LevelsRules& Piece, UGSTN_CombatRules& CombatRules);

	/**
	 * A GameRules asset with the Levels piece, the respawn piece (shared lives pool 4 refilling every 20 s, checkpoints on, restart from the last
	 * checkpoint) and the missions piece, all enabled with the plan's defaults (m12-modi-livelli 6). The starting point of a level's rules, and what
	 * the example level of G7 is built on; the designer then adds the goal mission and the level set
	 */
	GSTNMODE_LEVELSRUNTIME_API UGSTN_GameRules* BuildLevelsGameRules(UObject* Outer, UGSTN_LevelSetDefinition* LevelSet = nullptr, UGSTN_ScoreRules* ScoreRules = nullptr);
}
