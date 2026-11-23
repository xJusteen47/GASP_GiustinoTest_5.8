// Respawn, pure core (no GAS, no Network Prediction, no world).

#include "GSTN_RespawnLogic.h"
#include "GSTN_RespawnRules.h"
#include "GSTN_RespawnTags.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, GSTNRespawnCore);

namespace GSTNRespawnTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Respawn_Down, "Message.Respawn.Down", "A player was knocked out and spent lives (Target = pawn, Magnitude = lives left). Server event stream only");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Respawn_Respawned, "Message.Respawn.Respawned", "A player came back (Target = the new pawn). Server event stream only");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Respawn_PoolChanged, "Message.Respawn.PoolChanged", "The shared lives pool changed (Magnitude = lives). Server event stream only");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Respawn_CheckpointActivated, "Message.Respawn.CheckpointActivated", "A checkpoint became the team's active one. Server event stream only");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Respawn_RestartFromCheckpoint, "Message.Respawn.RestartFromCheckpoint", "Lives ran out: the team restarts from the last checkpoint");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Respawn_RestartLevel, "Message.Respawn.RestartLevel", "Lives ran out: the team restarts the level (missions reset)");
}

FGSTN_KOReasonRespawn UGSTN_RespawnRules::GetReason(const FGameplayTagContainer& EventTags) const
{
	for (const TPair<FGameplayTag, FGSTN_KOReasonRespawn>& Pair : PerReason)
	{
		if (Pair.Key.IsValid() && EventTags.HasTag(Pair.Key))
		{
			return Pair.Value;
		}
	}
	return FGSTN_KOReasonRespawn();
}

void FGSTN_LivesPool::Reset(const UGSTN_RespawnRules& Rules)
{
	PoolSize = FMath::Max(0, Rules.LivesPoolSize);
	MaxLives = FMath::Max(PoolSize, Rules.MaxLivesWithExtras);
	Lives = PoolSize;
	RegenSeconds = FMath::Max(0.f, Rules.LifeRegenSeconds);
	RegenProgress = 0.f;
	bRestartAtZero = Rules.bRestartWhenPoolReachesZero;
}

EGSTN_LifeSpendResult FGSTN_LivesPool::Spend(int32 Cost)
{
	if (Cost <= 0)
	{
		return EGSTN_LifeSpendResult::Respawn;
	}
	const bool bWasEmpty = Lives <= 0;
	Lives = FMath::Max(0, Lives - Cost);
	if (bRestartAtZero)
	{
		return Lives <= 0 ? EGSTN_LifeSpendResult::TeamRestart : EGSTN_LifeSpendResult::Respawn;
	}
	return bWasEmpty ? EGSTN_LifeSpendResult::TeamRestart : EGSTN_LifeSpendResult::Respawn;
}

int32 FGSTN_LivesPool::Advance(float DeltaSeconds)
{
	if (RegenSeconds <= 0.f || Lives >= PoolSize || DeltaSeconds <= 0.f)
	{
		RegenProgress = 0.f;
		return 0;
	}
	RegenProgress += DeltaSeconds;
	int32 Gained = 0;
	while (RegenProgress >= RegenSeconds && Lives < PoolSize)
	{
		RegenProgress -= RegenSeconds;
		++Lives;
		++Gained;
	}
	if (Lives >= PoolSize)
	{
		RegenProgress = 0.f;
	}
	return Gained;
}

int32 FGSTN_LivesPool::Add(int32 Count)
{
	const int32 Before = Lives;
	Lives = FMath::Clamp(Lives + FMath::Max(0, Count), 0, FMath::Max(MaxLives, PoolSize));
	return Lives - Before;
}

float FGSTN_LivesPool::SecondsToNextLife() const
{
	return RegenSeconds > 0.f && Lives < PoolSize ? FMath::Max(0.f, RegenSeconds - RegenProgress) : -1.f;
}

bool FGSTN_CheckpointProgress::TryActivate(int32 Order, FName Id, const UGSTN_RespawnRules& Rules)
{
	if (!Rules.bUseCheckpoints || Id == ActiveId)
	{
		return false;
	}
	if (Rules.bCheckpointsOnlyForward && HasActive() && Order <= ActiveOrder)
	{
		return false;
	}
	ActiveOrder = FMath::Max(0, Order);
	ActiveId = Id;
	return true;
}

EGSTN_RespawnPlacement GSTNRespawnLogic::ChoosePlacement(const UGSTN_RespawnRules& Rules, bool bHasActiveCheckpoint, bool bHasSafeTeammate, bool bTeamRestart)
{
	if (Rules.bUseCheckpoints && bHasActiveCheckpoint)
	{
		return EGSTN_RespawnPlacement::Checkpoint;
	}
	if (!bTeamRestart && bHasSafeTeammate && Rules.FallbackPlacement == EGSTN_RespawnFallbackPlacement::NearLivingTeammate)
	{
		return EGSTN_RespawnPlacement::NearTeammate;
	}
	return EGSTN_RespawnPlacement::LevelStart;
}

int32 GSTNRespawnLogic::SpawnPointFor(int32 Index, int32 NumPoints, int32& OutRing)
{
	if (NumPoints <= 0 || Index < 0)
	{
		OutRing = FMath::Max(0, Index);
		return INDEX_NONE;
	}
	OutRing = Index / NumPoints;
	return Index % NumPoints;
}
