// Levels mode: the rules as plain C++.

#include "GSTN_LevelsLogic.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GameRules/GSTN_GameRules.h"
#include "GSTN_LevelsRules.h"
#include "GSTN_LevelSetDefinition.h"
#include "GSTN_MissionTypes.h"
#include "GSTN_RespawnRules.h"
#include "Rules/GSTN_CombatRules.h"

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Goal zone

FGSTN_GoalGraceResult FGSTN_GoalGraceTracker::Update(double Now, int32 AliveInside, int32 AliveTotal, const FGSTN_GoalGraceParams& Params)
{
	FGSTN_GoalGraceResult Result;
	if (State == EGSTN_GoalState::Completed)
	{
		return Result;
	}

	if (State == EGSTN_GoalState::Idle)
	{
		if (AliveInside <= 0)
		{
			return Result;
		}
		State = EGSTN_GoalState::Grace;
		GraceEnd = Now + FMath::Max(0.f, Params.GraceSeconds);
		Result.bGraceStarted = true;
	}
	else if (AliveInside <= 0 && Params.bCancelWhenNobodyInside)
	{
		Reset();
		return Result;
	}

	const bool bEveryoneIn = Params.bFinishEarlyWhenAllInside && AliveTotal > 0 && AliveInside >= AliveTotal;
	if (bEveryoneIn || Now >= GraceEnd)
	{
		State = EGSTN_GoalState::Completed;
		Result.bCompleted = true;
	}
	return Result;
}

void FGSTN_GoalGraceTracker::Reset()
{
	State = EGSTN_GoalState::Idle;
	GraceEnd = -1.0;
}

float FGSTN_GoalGraceTracker::GetSecondsLeft(double Now) const
{
	return State == EGSTN_GoalState::Grace ? static_cast<float>(FMath::Max(0.0, GraceEnd - Now)) : 0.f;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Tally and score

void FGSTN_LevelTally::Reset()
{
	Counts.Reset();
	TeamCollected = 0;
	TeamCollectedValue = 0;
	CollectedByPlayer.Reset();
}

void FGSTN_LevelTally::ResetForRestart(const UGSTN_ScoreRules* Rules)
{
	TeamCollected = 0;
	TeamCollectedValue = 0;
	CollectedByPlayer.Reset();
	if (!Rules)
	{
		Counts.Reset();
		return;
	}
	// A counter that is not in the rules cannot say it survives a restart: it starts again
	for (auto It = Counts.CreateIterator(); It; ++It)
	{
		const FGSTN_ScoreCounterRule* Rule = Rules->Counters.FindByPredicate([&It](const FGSTN_ScoreCounterRule& Candidate) { return Candidate.CounterId == It.Key(); });
		if (!Rule || Rule->bResetOnLevelRestart)
		{
			It.RemoveCurrent();
		}
	}
}

bool FGSTN_LevelTally::AddEvent(const UGSTN_ScoreRules* Rules, const FGSTN_SimEvent& Event, int32 PlayerKey)
{
	bool bChanged = false;
	const int32 Magnitude = FMath::Max(1, FMath::RoundToInt(Event.Magnitude));

	if (Event.EventType.MatchesTagExact(GSTNMissionTags::Message_Mission_Collected.GetTag()))
	{
		++TeamCollected;
		TeamCollectedValue += Magnitude;
		++CollectedByPlayer.FindOrAdd(PlayerKey);
		bChanged = true;
	}

	if (Rules)
	{
		for (const FGSTN_ScoreCounterRule& Rule : Rules->Counters)
		{
			// An empty query would match every event in the stream's own rule; here it counts nothing
			if (Rule.CounterId.IsNone() || Rule.EventQuery.IsEmpty() || !FGSTN_SimEventDispatcher::Matches(Rule.EventQuery, Event))
			{
				continue;
			}
			Counts.FindOrAdd(Rule.CounterId) += Rule.bUseMagnitude ? Magnitude : 1;
			bChanged = true;
		}
	}
	return bChanged;
}

FGSTN_LevelResults GSTNLevelsLogic::ComputeResults(const UGSTN_ScoreRules& Rules, const FGSTN_LevelTally& Tally, float ElapsedSeconds, bool bCompleted)
{
	FGSTN_LevelResults Results;
	Results.ElapsedSeconds = ElapsedSeconds;
	Results.TeamCollected = Tally.TeamCollected;

	for (const FGSTN_ScoreCounterRule& Rule : Rules.Counters)
	{
		const int32* Count = Tally.Counts.Find(Rule.CounterId);
		const int32 Points = (Count ? *Count : 0) * Rule.PointsPerEvent;
		Results.CounterPoints += Points;
		if (Rule.bShowInResults)
		{
			FGSTN_ResultLine& Line = Results.Lines.AddDefaulted_GetRef();
			Line.Label = Rule.Label.IsEmpty() ? FText::FromName(Rule.CounterId) : Rule.Label;
			Line.Count = Count ? *Count : 0;
			Line.Points = Points;
		}
	}

	if (bCompleted)
	{
		Results.CompletionBonus = Rules.CompletionBonus;
		if (Rules.ParTimeSeconds > 0.f && Rules.PointsPerSecondUnderPar > 0)
		{
			Results.TimeBonus = FMath::FloorToInt(FMath::Max(0.f, Rules.ParTimeSeconds - ElapsedSeconds)) * Rules.PointsPerSecondUnderPar;
		}
	}

	Results.TotalScore = FMath::Max(0, Results.CounterPoints + Results.TimeBonus + Results.CompletionBonus);
	if (const FGSTN_ScoreRank* Rank = Rules.FindRank(Results.TotalScore))
	{
		Results.RankId = Rank->RankId;
		Results.RankLabel = Rank->Label;
	}
	return Results;
}

void GSTNLevelsLogic::ApplyFriendlyFire(const UGSTN_LevelsRules& Piece, UGSTN_CombatRules& CombatRules)
{
	CombatRules.bFriendlyFire = Piece.bFriendlyFire;
}

UGSTN_GameRules* GSTNLevelsLogic::BuildLevelsGameRules(UObject* Outer, UGSTN_LevelSetDefinition* LevelSet, UGSTN_ScoreRules* ScoreRules)
{
	UGSTN_GameRules* Rules = NewObject<UGSTN_GameRules>(Outer ? Outer : GetTransientPackage());

	UGSTN_LevelsRules* Levels = NewObject<UGSTN_LevelsRules>(Rules);
	Levels->bEnabled = true;
	Levels->LevelSet = TSoftObjectPtr<UGSTN_LevelSetDefinition>(LevelSet);
	Levels->ScoreRules = TSoftObjectPtr<UGSTN_ScoreRules>(ScoreRules);
	Rules->Pieces.Add(Levels);

	// The plan's defaults (m12-modi-livelli 6): the class defaults of the respawn piece already are lives pool 4, refill 20 s, checkpoints on,
	// restart from checkpoint, delay 2 s; only the switch is turned on
	UGSTN_RespawnRules* Respawn = NewObject<UGSTN_RespawnRules>(Rules);
	Respawn->bEnabled = true;
	Rules->Pieces.Add(Respawn);

	// No mission yet: the designer names the level's goal mission (a Reach objective) in the missions piece
	UGSTN_MissionRules* Missions = NewObject<UGSTN_MissionRules>(Rules);
	Missions->bEnabled = true;
	Rules->Pieces.Add(Missions);
	return Rules;
}
