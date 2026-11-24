// M12 G6: Levels mode logic, pure (no world, no actors). The runtime actors are thin: they feed these types with counts and events.

#include "Events/GSTN_SimEvent.h"
#include "GameRules/GSTN_GameRules.h"
#include "GSTN_LevelsLogic.h"
#include "GSTN_LevelsRules.h"
#include "GSTN_LevelSetDefinition.h"
#include "GSTN_MissionTypes.h"
#include "GSTN_RespawnRules.h"
#include "GSTN_RespawnTags.h"
#include "GSTN_ScoreRules.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Rules/GSTN_CombatRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGSTN_LevelEntry MakeEntry(const TCHAR* PackagePath)
	{
		FGSTN_LevelEntry Entry;
		if (PackagePath && *PackagePath)
		{
			const FString Name = FPackageName::GetShortName(PackagePath);
			Entry.Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(FString::Printf(TEXT("%s.%s"), PackagePath, *Name)));
		}
		return Entry;
	}

	FGSTN_SimEvent MakeEvent(const FGameplayTag& Type, float Magnitude = 1.f)
	{
		FGSTN_SimEvent Event;
		Event.EventType = Type;
		Event.Magnitude = Magnitude;
		return Event;
	}

	FGSTN_GoalGraceParams GraceParams(float Seconds, bool bEarly = true, bool bCancel = false)
	{
		FGSTN_GoalGraceParams Params;
		Params.GraceSeconds = Seconds;
		Params.bFinishEarlyWhenAllInside = bEarly;
		Params.bCancelWhenNobodyInside = bCancel;
		return Params;
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Level set ordering

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LevelsSetOrderTest, "GSTNMode_Levels.LevelSet.Ordering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_LevelsSetOrderTest::RunTest(const FString& Parameters)
{
	UGSTN_LevelSetDefinition* Set = NewObject<UGSTN_LevelSetDefinition>(GetTransientPackage());
	Set->Levels.Add(MakeEntry(TEXT("/Game/Maps/L_A")));
	Set->Levels.Add(MakeEntry(nullptr)); // half-authored: skipped
	Set->Levels.Add(MakeEntry(TEXT("/Game/Maps/L_C")));
	Set->Levels.Add(MakeEntry(TEXT("/Game/Maps/L_D")));
	Set->Levels[3].ExtraOptions = TEXT("GameRules=/Game/GAS/GR_D");

	TestEqual(TEXT("first valid level follows INDEX_NONE"), Set->GetNextIndex(INDEX_NONE), 0);
	TestEqual(TEXT("an entry without a map is skipped"), Set->GetNextIndex(0), 2);
	TestEqual(TEXT("then the next one"), Set->GetNextIndex(2), 3);
	TestEqual(TEXT("the set ends at the last level"), Set->GetNextIndex(3), static_cast<int32>(INDEX_NONE));

	Set->bLoop = true;
	TestEqual(TEXT("a looping set goes back to the first valid level"), Set->GetNextIndex(3), 0);

	TestEqual(TEXT("find by short name"), Set->FindIndexByMap(TEXT("L_C")), 2);
	TestEqual(TEXT("find by package name"), Set->FindIndexByMap(TEXT("/Game/Maps/L_D")), 3);
	TestEqual(TEXT("case does not matter"), Set->FindIndexByMap(TEXT("l_a")), 0);
	TestEqual(TEXT("a map the set does not list"), Set->FindIndexByMap(TEXT("L_Z")), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("an empty name finds nothing"), Set->FindIndexByMap(FString()), static_cast<int32>(INDEX_NONE));

	TestEqual(TEXT("travel URL"), Set->BuildTravelURL(2, false), FString(TEXT("/Game/Maps/L_C")));
	TestEqual(TEXT("travel URL on a listen server with options"), Set->BuildTravelURL(3, true), FString(TEXT("/Game/Maps/L_D?listen?GameRules=/Game/GAS/GR_D")));
	TestTrue(TEXT("an invalid entry has no URL"), Set->BuildTravelURL(1, false).IsEmpty());

	UGSTN_LevelSetDefinition* Empty = NewObject<UGSTN_LevelSetDefinition>(GetTransientPackage());
	Empty->bLoop = true;
	TestEqual(TEXT("an empty looping set ends instead of looping forever"), Empty->GetNextIndex(INDEX_NONE), static_cast<int32>(INDEX_NONE));
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Goal zone: one alive player + grace timer

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LevelsGoalGraceTest, "GSTNMode_Levels.Goal.OnePlayerStartsGraceTimer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_LevelsGoalGraceTest::RunTest(const FString& Parameters)
{
	FGSTN_GoalGraceTracker Tracker;
	const FGSTN_GoalGraceParams Params = GraceParams(10.f);

	FGSTN_GoalGraceResult R = Tracker.Update(100.0, 0, 3, Params);
	TestFalse(TEXT("nobody inside: nothing starts"), R.bGraceStarted || R.bCompleted);
	TestTrue(TEXT("still idle"), Tracker.GetState() == EGSTN_GoalState::Idle);

	R = Tracker.Update(101.0, 1, 3, Params); // one of three alive players is enough
	TestTrue(TEXT("one alive player starts the timer"), R.bGraceStarted);
	TestFalse(TEXT("and does not finish yet"), R.bCompleted);
	TestTrue(TEXT("grace state"), Tracker.GetState() == EGSTN_GoalState::Grace);
	TestEqual(TEXT("the timer ends 10 s after the entry"), Tracker.GetGraceEnd(), 111.0);
	TestEqual(TEXT("seconds left"), Tracker.GetSecondsLeft(106.0), 5.f);

	R = Tracker.Update(105.0, 1, 3, Params);
	TestFalse(TEXT("no second start event"), R.bGraceStarted);
	TestFalse(TEXT("not finished before the timer ends"), R.bCompleted);

	R = Tracker.Update(111.0, 1, 3, Params);
	TestTrue(TEXT("finished when the timer ends, though two players never arrived"), R.bCompleted);
	TestTrue(TEXT("latched"), Tracker.GetState() == EGSTN_GoalState::Completed);

	R = Tracker.Update(112.0, 3, 3, Params);
	TestFalse(TEXT("completion is reported once"), R.bCompleted || R.bGraceStarted);

	Tracker.Reset();
	TestTrue(TEXT("reset goes back to idle"), Tracker.GetState() == EGSTN_GoalState::Idle);
	TestEqual(TEXT("and no timer"), Tracker.GetGraceEnd(), -1.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LevelsGoalGraceEdgesTest, "GSTNMode_Levels.Goal.GraceEdges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_LevelsGoalGraceEdgesTest::RunTest(const FString& Parameters)
{
	{
		FGSTN_GoalGraceTracker Tracker;
		const FGSTN_GoalGraceResult R = Tracker.Update(0.0, 2, 2, GraceParams(10.f));
		TestTrue(TEXT("every alive player inside ends it before the timer"), R.bGraceStarted && R.bCompleted);
	}
	{
		FGSTN_GoalGraceTracker Tracker;
		Tracker.Update(0.0, 2, 2, GraceParams(10.f, /*bEarly*/ false));
		TestTrue(TEXT("early finish off: the timer still runs"), Tracker.GetState() == EGSTN_GoalState::Grace);
	}
	{
		FGSTN_GoalGraceTracker Tracker;
		const FGSTN_GoalGraceResult R = Tracker.Update(5.0, 1, 4, GraceParams(0.f));
		TestTrue(TEXT("grace 0 ends the level at once"), R.bCompleted);
	}
	{
		// GM.5 default: once started it runs out, even if everybody left
		FGSTN_GoalGraceTracker Tracker;
		Tracker.Update(0.0, 1, 2, GraceParams(10.f));
		Tracker.Update(3.0, 0, 2, GraceParams(10.f));
		TestTrue(TEXT("nobody inside does not stop the timer by default"), Tracker.GetState() == EGSTN_GoalState::Grace);
		TestTrue(TEXT("and it completes"), Tracker.Update(10.0, 0, 2, GraceParams(10.f)).bCompleted);
	}
	{
		FGSTN_GoalGraceTracker Tracker;
		Tracker.Update(0.0, 1, 2, GraceParams(10.f, true, /*bCancel*/ true));
		const FGSTN_GoalGraceResult R = Tracker.Update(3.0, 0, 2, GraceParams(10.f, true, true));
		TestFalse(TEXT("cancel on: nobody inside stops it"), R.bCompleted);
		TestTrue(TEXT("back to idle"), Tracker.GetState() == EGSTN_GoalState::Idle);
	}
	{
		// Nobody alive at all (everyone down): never "everyone is inside"
		FGSTN_GoalGraceTracker Tracker;
		const FGSTN_GoalGraceResult R = Tracker.Update(0.0, 0, 0, GraceParams(10.f));
		TestFalse(TEXT("no alive players: nothing happens"), R.bGraceStarted || R.bCompleted);
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Collectible team counting

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LevelsCollectibleTallyTest, "GSTNMode_Levels.Tally.TeamCollectibles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_LevelsCollectibleTallyTest::RunTest(const FString& Parameters)
{
	UGSTN_ScoreRules* Rules = UGSTN_ScoreRules::MakeDefault(GetTransientPackage());
	FGSTN_LevelTally Tally;

	const FGSTN_SimEvent Collected1 = MakeEvent(GSTNMissionTags::Message_Mission_Collected, 1.f);
	const FGSTN_SimEvent Collected5 = MakeEvent(GSTNMissionTags::Message_Mission_Collected, 5.f);

	TestTrue(TEXT("a collectible changes the tally"), Tally.AddEvent(Rules, Collected1, 1));
	Tally.AddEvent(Rules, Collected5, 2);
	Tally.AddEvent(Rules, Collected1, 1);

	TestEqual(TEXT("the team took 3 collectibles"), Tally.TeamCollected, 3);
	TestEqual(TEXT("their values add up"), Tally.TeamCollectedValue, 7);
	TestEqual(TEXT("player 1 took 2"), Tally.CollectedByPlayer.FindRef(1), 2);
	TestEqual(TEXT("player 2 took 1"), Tally.CollectedByPlayer.FindRef(2), 1);
	TestEqual(TEXT("the Collectibles counter uses the magnitude"), Tally.Counts.FindRef(TEXT("Collectibles")), 7);

	TestFalse(TEXT("an unrelated event changes nothing"), Tally.AddEvent(Rules, MakeEvent(GSTNMissionTags::Message_Mission_MissionCompleted), 1));
	TestEqual(TEXT("still 3"), Tally.TeamCollected, 3);

	TestTrue(TEXT("without rules the collectibles still count for the team"), Tally.AddEvent(nullptr, Collected1, INDEX_NONE));
	TestEqual(TEXT("4 now"), Tally.TeamCollected, 4);
	TestEqual(TEXT("a taker that is not a player has its own key"), Tally.CollectedByPlayer.FindRef(INDEX_NONE), 1);

	// A restart brings the collectibles back; deaths stay (bResetOnLevelRestart is off for them)
	Tally.AddEvent(Rules, MakeEvent(GSTNRespawnTags::Message_Respawn_Down), 1);
	TestEqual(TEXT("a death was counted"), Tally.Counts.FindRef(TEXT("Deaths")), 1);
	Tally.ResetForRestart(Rules);
	TestEqual(TEXT("collectibles are back after a restart"), Tally.TeamCollected, 0);
	TestEqual(TEXT("the collectible counter restarts"), Tally.Counts.FindRef(TEXT("Collectibles")), 0);
	TestEqual(TEXT("deaths survive a restart"), Tally.Counts.FindRef(TEXT("Deaths")), 1);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Score

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LevelsScoreTest, "GSTNMode_Levels.Score.Calculation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_LevelsScoreTest::RunTest(const FString& Parameters)
{
	// Default rules: collectible 100 (by magnitude), death -25, par 180 s at 1 point per second, completion 500, ranks C 0 / B 500 / A 1000 / S 1500
	UGSTN_ScoreRules* Rules = UGSTN_ScoreRules::MakeDefault(GetTransientPackage());
	FGSTN_LevelTally Tally;
	Tally.Counts.Add(TEXT("Collectibles"), 4);
	Tally.Counts.Add(TEXT("Deaths"), 2);
	Tally.TeamCollected = 4;

	FGSTN_LevelResults Results = GSTNLevelsLogic::ComputeResults(*Rules, Tally, 100.f, true);
	TestEqual(TEXT("counter points: 4 x 100 - 2 x 25"), Results.CounterPoints, 350);
	TestEqual(TEXT("time bonus: 80 s under par"), Results.TimeBonus, 80);
	TestEqual(TEXT("completion bonus"), Results.CompletionBonus, 500);
	TestEqual(TEXT("total"), Results.TotalScore, 930);
	TestEqual(TEXT("rank B"), Results.RankId, FName(TEXT("B")));
	TestEqual(TEXT("collectibles carried to the results"), Results.TeamCollected, 4);

	Results = GSTNLevelsLogic::ComputeResults(*Rules, Tally, 200.f, true);
	TestEqual(TEXT("slower than par earns no time bonus"), Results.TimeBonus, 0);
	TestEqual(TEXT("total without it"), Results.TotalScore, 850);

	Results = GSTNLevelsLogic::ComputeResults(*Rules, Tally, 100.f, false);
	TestEqual(TEXT("an abandoned level scores no bonus"), Results.TotalScore, 350);
	TestEqual(TEXT("rank C"), Results.RankId, FName(TEXT("C")));

	FGSTN_LevelTally Falls;
	Falls.Counts.Add(TEXT("Deaths"), 10);
	Results = GSTNLevelsLogic::ComputeResults(*Rules, Falls, 500.f, false);
	TestEqual(TEXT("the score never goes below zero"), Results.TotalScore, 0);

	// Ranks come in any order: the highest MinScore reached wins
	UGSTN_ScoreRules* Ranked = NewObject<UGSTN_ScoreRules>(GetTransientPackage());
	FGSTN_ScoreRank& S = Ranked->Ranks.AddDefaulted_GetRef();
	S.RankId = TEXT("S");
	S.MinScore = 1500;
	FGSTN_ScoreRank& C = Ranked->Ranks.AddDefaulted_GetRef();
	C.RankId = TEXT("C");
	C.MinScore = 0;
	FGSTN_ScoreRank& A = Ranked->Ranks.AddDefaulted_GetRef();
	A.RankId = TEXT("A");
	A.MinScore = 1000;
	TestEqual(TEXT("1200 is an A"), Ranked->FindRank(1200)->RankId, FName(TEXT("A")));
	TestEqual(TEXT("1500 is an S"), Ranked->FindRank(1500)->RankId, FName(TEXT("S")));
	TestTrue(TEXT("below every rank: none"), Ranked->FindRank(-1) == nullptr);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Friendly fire default ON (GM.13) and the rules piece

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_LevelsFriendlyFireTest, "GSTNMode_Levels.Rules.FriendlyFireDefaultOn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_LevelsFriendlyFireTest::RunTest(const FString& Parameters)
{
	const UGSTN_LevelsRules* Defaults = GetDefault<UGSTN_LevelsRules>();
	TestTrue(TEXT("friendly fire is ON by default in Levels (GM.13)"), Defaults->bFriendlyFire);
	TestFalse(TEXT("the mode is off until a rules asset or the cvar turns it on"), Defaults->bEnabled);
	TestEqual(TEXT("goal grace default 10 s (GM.5)"), Defaults->GoalGraceSeconds, 10.f);
	TestEqual(TEXT("up to four players (GM.2)"), Defaults->MaxPlayers, 4);

	// The combat rules default to off; the piece's value is written into a (transient) copy
	UGSTN_CombatRules* Combat = NewObject<UGSTN_CombatRules>(GetTransientPackage());
	Combat->bFriendlyFire = false;
	GSTNLevelsLogic::ApplyFriendlyFire(*Defaults, *Combat);
	TestTrue(TEXT("the combat rules take friendly fire from the piece"), Combat->bFriendlyFire);

	UGSTN_LevelsRules* Piece = NewObject<UGSTN_LevelsRules>(GetTransientPackage());
	Piece->bFriendlyFire = false;
	GSTNLevelsLogic::ApplyFriendlyFire(*Piece, *Combat);
	TestFalse(TEXT("a designer can turn it off in the asset"), Combat->bFriendlyFire);

	// Scaling: highest PlayerCount not above the real count
	TestEqual(TEXT("one player is the identity"), Defaults->GetScaling(1).EnemyCountScale, 1.f);
	TestTrue(TEXT("four players scale up"), Defaults->GetScaling(4).EnemyCountScale > Defaults->GetScaling(2).EnemyCountScale);
	TestEqual(TEXT("more players than the table: its last row"), Defaults->GetScaling(9).PlayerCount, 4);

	// The starting GameRules asset carries the three sibling pieces, all on
	UGSTN_GameRules* Rules = GSTNLevelsLogic::BuildLevelsGameRules(GetTransientPackage());
	TestEqual(TEXT("levels, respawn and missions pieces"), Rules->Pieces.Num(), 3);
	const UGSTN_LevelsRules* LevelsPiece = Cast<UGSTN_LevelsRules>(Rules->FindPiece(UGSTN_LevelsRules::StaticClass()));
	const UGSTN_RespawnRules* RespawnPiece = Cast<UGSTN_RespawnRules>(Rules->FindPiece(UGSTN_RespawnRules::StaticClass()));
	const UGSTN_MissionRules* MissionPiece = Cast<UGSTN_MissionRules>(Rules->FindPiece(UGSTN_MissionRules::StaticClass()));
	TestTrue(TEXT("levels piece enabled"), LevelsPiece && LevelsPiece->bEnabled);
	TestTrue(TEXT("respawn piece enabled"), RespawnPiece && RespawnPiece->bEnabled);
	TestTrue(TEXT("missions piece enabled"), MissionPiece && MissionPiece->bEnabled);
	return true;
}

#endif
