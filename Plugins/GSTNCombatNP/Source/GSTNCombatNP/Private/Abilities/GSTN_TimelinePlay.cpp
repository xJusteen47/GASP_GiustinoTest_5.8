// Combat abilities (GAS-NPP fork).

#include "Abilities/GSTN_TimelinePlay.h"

EGSTN_TimelineStartRefusal FGSTN_TimelinePlayRules::CheckStart(bool bInSimulationTick, bool bRestoringFrame, bool bGateClosed, bool bEntryRegistered)
{
	if (!bInSimulationTick)
	{
		return EGSTN_TimelineStartRefusal::OutsideSimulation;
	}
	if (bRestoringFrame)
	{
		return EGSTN_TimelineStartRefusal::Restoring;
	}
	if (bGateClosed)
	{
		return EGSTN_TimelineStartRefusal::GateClosed;
	}
	return bEntryRegistered ? EGSTN_TimelineStartRefusal::None : EGSTN_TimelineStartRefusal::NotRegistered;
}

EGSTN_TimelinePlayResult FGSTN_TimelinePlayRules::ResolveResult(EGSTN_MoveEndReason Reason, bool bAbilityEnding)
{
	if (Reason == EGSTN_MoveEndReason::Finished)
	{
		return EGSTN_TimelinePlayResult::Completed;
	}
	return bAbilityEnding ? EGSTN_TimelinePlayResult::Cancelled : EGSTN_TimelinePlayResult::Interrupted;
}

bool FGSTN_TimelinePlayRules::ShouldHostEndAfterPlay(EGSTN_TimelinePlayResult Result, bool bEndOnInterrupt)
{
	return Result == EGSTN_TimelinePlayResult::Interrupted && bEndOnInterrupt;
}

void FGSTN_TimelinePlayRules::DiffTags(const FGameplayTagContainer& Before, const FGameplayTagContainer& After, FGameplayTagContainer& OutRemove, FGameplayTagContainer& OutAdd)
{
	OutRemove.Reset();
	OutAdd.Reset();
	for (const FGameplayTag& Tag : Before)
	{
		if (!After.HasTagExact(Tag))
		{
			OutRemove.AddTag(Tag);
		}
	}
	for (const FGameplayTag& Tag : After)
	{
		if (!Before.HasTagExact(Tag))
		{
			OutAdd.AddTag(Tag);
		}
	}
}
