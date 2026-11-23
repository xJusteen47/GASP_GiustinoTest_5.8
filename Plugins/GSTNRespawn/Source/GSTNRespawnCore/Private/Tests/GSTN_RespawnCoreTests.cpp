// M12 G3 part 1: lives pool, checkpoints and placement, pure.

#include "GSTN_RespawnLogic.h"
#include "GSTN_RespawnRules.h"
#include "GameRules/GSTN_GameRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	UGSTN_RespawnRules* MakeRules(int32 Pool, float Regen, bool bRestartAtZero = true)
	{
		UGSTN_RespawnRules* Rules = NewObject<UGSTN_RespawnRules>(GetTransientPackage());
		Rules->LivesPoolSize = Pool;
		Rules->LifeRegenSeconds = Regen;
		Rules->MaxLivesWithExtras = Pool + 2;
		Rules->bRestartWhenPoolReachesZero = bRestartAtZero;
		return Rules;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RespawnPoolTest, "GSTNRespawn.Core.LivesPool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_RespawnPoolTest::RunTest(const FString& Parameters)
{
	// Owner example 1: 4 lives that refill (restart when a KO takes the pool to zero)
	FGSTN_LivesPool Pool;
	Pool.Reset(*MakeRules(4, 20.f));
	TestEqual(TEXT("full at start"), Pool.Lives, 4);
	TestTrue(TEXT("KO 1 respawns"), Pool.Spend(1) == EGSTN_LifeSpendResult::Respawn);
	TestEqual(TEXT("3 left"), Pool.Lives, 3);
	TestEqual(TEXT("19 s: nothing yet"), Pool.Advance(19.f), 0);
	TestEqual(TEXT("next life in 1 s"), Pool.SecondsToNextLife(), 1.f);
	TestEqual(TEXT("20 s: one life back"), Pool.Advance(1.f), 1);
	TestEqual(TEXT("full again"), Pool.Lives, 4);
	TestEqual(TEXT("no refill above the pool"), Pool.Advance(100.f), 0);
	TestEqual(TEXT("no next life when full"), Pool.SecondsToNextLife(), -1.f);
	Pool.Spend(1);
	Pool.Spend(1);
	Pool.Spend(1);
	TestTrue(TEXT("4 KOs close together: the 4th restarts the team"), Pool.Spend(1) == EGSTN_LifeSpendResult::TeamRestart);
	TestEqual(TEXT("empty"), Pool.Lives, 0);

	// Owner example 2: 20 lives, no refill
	FGSTN_LivesPool Twenty;
	Twenty.Reset(*MakeRules(20, 0.f));
	int32 Restarts = 0;
	for (int32 KO = 0; KO < 20; ++KO)
	{
		Restarts += Twenty.Spend(1) == EGSTN_LifeSpendResult::TeamRestart ? 1 : 0;
	}
	TestEqual(TEXT("the 20th KO restarts, once"), Restarts, 1);
	TestEqual(TEXT("no refill with 0 s"), Twenty.Advance(1000.f), 0);

	// The other reading: the KO that empties the pool still respawns; the next one restarts
	FGSTN_LivesPool Late;
	Late.Reset(*MakeRules(2, 0.f, false));
	TestTrue(TEXT("late: KO 1 respawns"), Late.Spend(1) == EGSTN_LifeSpendResult::Respawn);
	TestTrue(TEXT("late: KO 2 empties and respawns"), Late.Spend(1) == EGSTN_LifeSpendResult::Respawn);
	TestTrue(TEXT("late: KO 3 restarts"), Late.Spend(1) == EGSTN_LifeSpendResult::TeamRestart);

	// Free KO, extra lives, multi-life refill in one step
	FGSTN_LivesPool Extra;
	Extra.Reset(*MakeRules(3, 5.f));
	TestTrue(TEXT("free KO respawns"), Extra.Spend(0) == EGSTN_LifeSpendResult::Respawn);
	TestEqual(TEXT("free KO costs nothing"), Extra.Lives, 3);
	TestEqual(TEXT("extras capped at MaxLivesWithExtras"), Extra.Add(10), 2);
	TestEqual(TEXT("5 lives"), Extra.Lives, 5);
	Extra.Spend(4);
	TestEqual(TEXT("one long step refills two lives up to the pool"), Extra.Advance(12.f), 2);
	TestEqual(TEXT("back to the pool size, not the extras"), Extra.Lives, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RespawnCheckpointTest, "GSTNRespawn.Core.Checkpoints",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_RespawnCheckpointTest::RunTest(const FString& Parameters)
{
	UGSTN_RespawnRules* Rules = MakeRules(4, 20.f);
	FGSTN_CheckpointProgress Progress;
	TestFalse(TEXT("none at start"), Progress.HasActive());
	TestTrue(TEXT("first touch activates"), Progress.TryActivate(1, TEXT("CP_A"), *Rules));
	TestFalse(TEXT("same one again: no change"), Progress.TryActivate(1, TEXT("CP_A"), *Rules));
	TestTrue(TEXT("a later one takes over"), Progress.TryActivate(2, TEXT("CP_B"), *Rules));
	TestFalse(TEXT("walking back does not regress"), Progress.TryActivate(1, TEXT("CP_A"), *Rules));
	TestEqual(TEXT("B active"), Progress.ActiveId, FName(TEXT("CP_B")));
	Rules->bCheckpointsOnlyForward = false;
	TestTrue(TEXT("without only-forward the last touched wins"), Progress.TryActivate(1, TEXT("CP_A"), *Rules));
	Rules->bUseCheckpoints = false;
	Progress.Clear();
	TestFalse(TEXT("no checkpoints in the rules: never active"), Progress.TryActivate(3, TEXT("CP_C"), *Rules));

	// Placement
	Rules->bUseCheckpoints = true;
	using namespace GSTNRespawnLogic;
	TestTrue(TEXT("checkpoint first"), ChoosePlacement(*Rules, true, true, false) == EGSTN_RespawnPlacement::Checkpoint);
	TestTrue(TEXT("else near a teammate"), ChoosePlacement(*Rules, false, true, false) == EGSTN_RespawnPlacement::NearTeammate);
	TestTrue(TEXT("team restart never near a teammate"), ChoosePlacement(*Rules, false, true, true) == EGSTN_RespawnPlacement::LevelStart);
	TestTrue(TEXT("nobody safe: level start"), ChoosePlacement(*Rules, false, false, false) == EGSTN_RespawnPlacement::LevelStart);
	Rules->FallbackPlacement = EGSTN_RespawnFallbackPlacement::LevelStart;
	TestTrue(TEXT("rule LevelStart"), ChoosePlacement(*Rules, false, true, false) == EGSTN_RespawnPlacement::LevelStart);
	Rules->bUseCheckpoints = false;
	TestTrue(TEXT("checkpoints off ignores an active one"), ChoosePlacement(*Rules, true, false, false) == EGSTN_RespawnPlacement::LevelStart);

	int32 Ring = 0;
	TestEqual(TEXT("player 3 of 4 points"), SpawnPointFor(2, 4, Ring), 2);
	TestEqual(TEXT("player 5 of 4 points wraps"), SpawnPointFor(4, 4, Ring), 0);
	TestEqual(TEXT("second ring"), Ring, 1);
	TestEqual(TEXT("no points"), SpawnPointFor(1, 0, Ring), (int32)INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RespawnGameRulesTest, "GSTNRespawn.Core.GameRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGSTN_RespawnGameRulesTest::RunTest(const FString& Parameters)
{
	const UGSTN_GameRules* Previous = FGSTN_ActiveGameRules::Get();
	FGSTN_ActiveGameRules::Set(nullptr);
	TestFalse(TEXT("off without an asset (today's behaviour)"), UGSTN_RespawnRules::Get().bEnabled);

	UGSTN_GameRules* Rules = NewObject<UGSTN_GameRules>(GetTransientPackage());
	UGSTN_RespawnRules* Piece = NewObject<UGSTN_RespawnRules>(Rules);
	Piece->bEnabled = true;
	Piece->LivesPoolSize = 20;
	FGSTN_KOReasonRespawn Pit;
	Pit.LifeCost = 0;
	Piece->PerReason.Add(FGameplayTag::RequestGameplayTag(TEXT("Combat.KOReason.Zone"), false), Pit);
	Rules->Pieces.Add(Piece);
	FGSTN_ActiveGameRules::Set(Rules);
	TestTrue(TEXT("the level's piece is read"), &UGSTN_RespawnRules::Get() == Piece);
	TestEqual(TEXT("its pool"), UGSTN_RespawnRules::Get().LivesPoolSize, 20);
	const FGameplayTag Zone = FGameplayTag::RequestGameplayTag(TEXT("Combat.KOReason.Zone"), false);
	if (Zone.IsValid())
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(Zone);
		TestEqual(TEXT("a zone KO is free in this level"), UGSTN_RespawnRules::Get().GetReason(Tags).LifeCost, 0);
	}
	TestEqual(TEXT("an unlisted reason costs 1"), UGSTN_RespawnRules::Get().GetReason(FGameplayTagContainer()).LifeCost, 1);

	FGSTN_ActiveGameRules::Set(Previous);
	return true;
}

#endif
