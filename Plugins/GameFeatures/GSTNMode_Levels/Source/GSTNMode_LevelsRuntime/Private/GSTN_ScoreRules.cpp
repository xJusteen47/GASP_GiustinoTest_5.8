// Levels mode: data-driven score.

#include "GSTN_ScoreRules.h"
#include "GSTN_MissionTypes.h"
#include "GSTN_RespawnTags.h"

const FGSTN_ScoreRank* UGSTN_ScoreRules::FindRank(int32 Score) const
{
	const FGSTN_ScoreRank* Best = nullptr;
	for (const FGSTN_ScoreRank& Rank : Ranks)
	{
		if (Score >= Rank.MinScore && (Best == nullptr || Rank.MinScore > Best->MinScore))
		{
			Best = &Rank;
		}
	}
	return Best;
}

UGSTN_ScoreRules* UGSTN_ScoreRules::MakeDefault(UObject* Outer)
{
	UGSTN_ScoreRules* Rules = NewObject<UGSTN_ScoreRules>(Outer ? Outer : GetTransientPackage());

	FGSTN_ScoreCounterRule& Collectibles = Rules->Counters.AddDefaulted_GetRef();
	Collectibles.CounterId = TEXT("Collectibles");
	Collectibles.Label = NSLOCTEXT("GSTNLevels", "ScoreCollectibles", "Collectibles");
	Collectibles.EventQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNMissionTags::Message_Mission_Collected.GetTag()));
	Collectibles.PointsPerEvent = 100;
	Collectibles.bUseMagnitude = true;

	// A kill is a KO of something with a minion type; the KO of a player is a death, not a kill. Both tags are looked up by name so this plugin
	// needs neither combat nor minion; a project without them simply gets no kill row
	const FGameplayTag KOTag = FGameplayTag::RequestGameplayTag(TEXT("Combat.Event.KO"), false);
	const FGameplayTag MinionTypeTag = FGameplayTag::RequestGameplayTag(TEXT("Minion.Type"), false);
	if (KOTag.IsValid() && MinionTypeTag.IsValid())
	{
		FGameplayTagQueryExpression KOExpr;
		KOExpr.AnyTagsMatch().AddTag(KOTag);
		FGameplayTagQueryExpression MinionExpr;
		MinionExpr.AnyTagsMatch().AddTag(MinionTypeTag);
		FGameplayTagQueryExpression Root;
		Root.AllExprMatch().AddExpr(KOExpr).AddExpr(MinionExpr);

		FGSTN_ScoreCounterRule& Kills = Rules->Counters.AddDefaulted_GetRef();
		Kills.CounterId = TEXT("Kills");
		Kills.Label = NSLOCTEXT("GSTNLevels", "ScoreKills", "Enemies defeated");
		Kills.EventQuery = FGameplayTagQuery::BuildQuery(Root);
		Kills.PointsPerEvent = 10;
	}

	FGSTN_ScoreCounterRule& Deaths = Rules->Counters.AddDefaulted_GetRef();
	Deaths.CounterId = TEXT("Deaths");
	Deaths.Label = NSLOCTEXT("GSTNLevels", "ScoreDeaths", "Falls");
	Deaths.EventQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNRespawnTags::Message_Respawn_Down.GetTag()));
	Deaths.PointsPerEvent = -25;
	Deaths.bResetOnLevelRestart = false;

	Rules->ParTimeSeconds = 180.f;
	Rules->PointsPerSecondUnderPar = 1;
	Rules->CompletionBonus = 500;

	const struct { const TCHAR* Id; int32 Min; } RankRows[] = { { TEXT("C"), 0 }, { TEXT("B"), 500 }, { TEXT("A"), 1000 }, { TEXT("S"), 1500 } };
	for (const auto& Row : RankRows)
	{
		FGSTN_ScoreRank& Rank = Rules->Ranks.AddDefaulted_GetRef();
		Rank.RankId = Row.Id;
		Rank.Label = FText::FromString(Row.Id);
		Rank.MinScore = Row.Min;
	}
	return Rules;
}
