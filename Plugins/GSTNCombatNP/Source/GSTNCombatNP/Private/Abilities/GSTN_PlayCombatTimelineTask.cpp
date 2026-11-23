// Combat abilities (GAS-NPP fork).

#include "Abilities/GSTN_PlayCombatTimelineTask.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "Abilities/NpAbilitySystemComponent.h"

#define LOCTEXT_NAMESPACE "GSTNPlayCombatTimelineTask"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNPlayTimelineTask, Log, All);

UGSTN_PlayCombatTimelineTask::UGSTN_PlayCombatTimelineTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// No data of its own to sync: the fork syncs the active bit, the owning ability the rest (see the class comment)
	DataType = nullptr;
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UGSTN_PlayCombatTimelineTask, ExecuteTask);
	ShouldTaskTick = false;
}

void UGSTN_PlayCombatTimelineTask::ExecuteTask(UGSTN_CombatTimeline* Timeline)
{
	if (!GetOwningAbility() || IsInRollback())
	{
		return;
	}
	UGSTN_GA_BaseCombat* Ability = Cast<UGSTN_GA_BaseCombat>(GetOwningAbility());
	if (!Ability)
	{
		UE_LOG(LogGSTNPlayTimelineTask, Warning, TEXT("%s: Play Combat Timeline And Wait needs a combat ability (a child of GSTN_GA_BaseCombat)"), *GetNameSafe(GetOwningAbility()));
		OnCancelled.Broadcast();
		return;
	}
	// The ability activates this task once the start is written (after the play this task may still own ended as Interrupted)
	const int32 TaskIndex = Ability->PredictionTasksInstances.IndexOfByKey(this);
	if (Ability->PlayTimeline(Timeline, TaskIndex) != EGSTN_TimelineStartRefusal::None)
	{
		DeactivateTask(true);
		OnCancelled.Broadcast();
	}
}

void UGSTN_PlayCombatTimelineTask::HandleKey(FGameplayTag KeyTag, int32 TimelineFrame)
{
	if (ShouldTriggerCallbacks())
	{
		OnKey.Broadcast(KeyTag, TimelineFrame);
	}
}

void UGSTN_PlayCombatTimelineTask::HandleHitConfirmed(int32 NewHitGroups, int32 AllHitGroups)
{
	if (ShouldTriggerCallbacks())
	{
		OnHitConfirmed.Broadcast(NewHitGroups, AllHitGroups);
	}
}

void UGSTN_PlayCombatTimelineTask::HandleEnded(EGSTN_TimelinePlayResult Result, EGSTN_MoveEndReason Reason)
{
	if (!ShouldTriggerCallbacks())
	{
		return;
	}
	// Inactive before the pin runs: a Play from the pin may activate this same task again for the next timeline
	DeactivateTask(Result == EGSTN_TimelinePlayResult::Cancelled);
	switch (Result)
	{
	case EGSTN_TimelinePlayResult::Completed:
		OnCompleted.Broadcast();
		break;
	case EGSTN_TimelinePlayResult::Interrupted:
		OnInterrupted.Broadcast(Reason);
		break;
	default:
		OnCancelled.Broadcast();
		break;
	}
}

FText UGSTN_PlayCombatTimelineTask::GetNodeTitle() const
{
	return LOCTEXT("PlayCombatTimelineAndWait", "Play Combat Timeline And Wait");
}

FLinearColor UGSTN_PlayCombatTimelineTask::GetNodeTitleColor() const
{
	// The colour of the engine's latent ability task nodes
	return FLinearColor(0.15f, 0.42f, 0.70f, 1.f);
}

#undef LOCTEXT_NAMESPACE
