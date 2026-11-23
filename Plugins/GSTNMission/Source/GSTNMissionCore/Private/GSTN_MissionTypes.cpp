// Missions, pure core (no GAS, no Network Prediction, no world).

#include "GSTN_MissionTypes.h"
#include "Events/GSTN_SimEventSubsystem.h"
#include "GameplayTagsManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

/**
 * Registers the plugin's Config/Tags folder (the ini leaves of Mission.Def and Mission.Objective). The engine reads Config/Tags only for the
 * project and for Game Feature plugins; registering it here keeps the plugin usable alone (as GSTNPiggyback does).
 */
class FGSTNMissionCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GSTNMission")))
		{
			TagIniPath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Config"), TEXT("Tags"));
			UGameplayTagsManager::Get().AddTagIniSearchPath(TagIniPath);
		}
	}

	virtual void ShutdownModule() override
	{
		if (!TagIniPath.IsEmpty() && UObjectInitialized())
		{
			UGameplayTagsManager::Get().RemoveTagIniSearchPath(TagIniPath);
		}
	}

private:
	FString TagIniPath;
};

IMPLEMENT_MODULE(FGSTNMissionCoreModule, GSTNMissionCore);

namespace GSTNMissionTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mission_Def, "Mission.Def", "Root of mission identities; leaves in ini (Config/Tags/<Mode>.ini)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Mission_Objective, "Mission.Objective", "Root of objective identities (also objective zones); leaves in ini");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Mission_ObjectiveActivated, "Message.Mission.ObjectiveActivated", "An objective became active. Server event stream only");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Mission_ObjectiveProgressed, "Message.Mission.ObjectiveProgressed", "An objective's progress changed (Magnitude = progress)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Mission_ObjectiveCompleted, "Message.Mission.ObjectiveCompleted", "An objective was completed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Mission_ObjectiveFailed, "Message.Mission.ObjectiveFailed", "An objective failed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Mission_MissionCompleted, "Message.Mission.MissionCompleted", "A mission was completed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Mission_MissionFailed, "Message.Mission.MissionFailed", "A mission failed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Message_Mission_Collected, "Message.Mission.Collected", "A collectible was taken (Instigator = player, Tags = its identity)");
}

bool GSTNMissionLogic::QueryMatches(const FGameplayTagQuery& Query, const FGSTN_SimEvent& Event)
{
	return !Query.IsEmpty() && FGSTN_SimEventDispatcher::Matches(Query, Event);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Rules

int32 UGSTN_ObjectiveRule::GetTargetCount_Implementation() const
{
	return 1;
}

int32 UGSTN_ObjectiveRule::OnEvent_Implementation(const FGSTN_SimEvent& Event, const FGSTN_ObjectiveState& InState) const
{
	return 0;
}

int32 UGSTN_ObjectiveRule::OnPoll_Implementation(const FGSTN_ObjectivePollContext& Context, const FGSTN_ObjectiveState& InState) const
{
	return -1;
}

int32 UGSTN_ObjectiveRule_CountEvents::OnEvent_Implementation(const FGSTN_SimEvent& Event, const FGSTN_ObjectiveState& InState) const
{
	if (!GSTNMissionLogic::QueryMatches(EventQuery, Event))
	{
		return 0;
	}
	return bUseMagnitude ? FMath::Max(1, FMath::RoundToInt(Event.Magnitude)) : 1;
}

int32 UGSTN_ObjectiveRule_Reach::OnPoll_Implementation(const FGSTN_ObjectivePollContext& Context, const FGSTN_ObjectiveState& InState) const
{
	const int32* Inside = Context.PlayersInZone.Find(ZoneTag);
	const int32 Count = Inside ? *Inside : 0;
	const bool bReached = Require == EGSTN_ReachRequire::AnyPlayer ? Count > 0 : (Context.AlivePlayers > 0 && Count >= Context.AlivePlayers);
	return bReached ? 1 : 0;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Tracker

void FGSTN_MissionTracker::Start(const UGSTN_MissionDefinition* InDefinition, float Now, bool bInMergeSameFrame, TArray<FGSTN_MissionChange>& OutChanges)
{
	Definition = InDefinition;
	bMergeSameFrame = bInMergeSameFrame;
	Objectives.Reset();
	RecentEvents.Reset();
	State = InDefinition ? EGSTN_MissionState::Running : EGSTN_MissionState::NotStarted;
	MissionEnd = InDefinition && InDefinition->TimeLimitSeconds > 0.f ? Now + InDefinition->TimeLimitSeconds : -1.f;
	if (!InDefinition)
	{
		return;
	}
	for (const FGSTN_ObjectiveSpec& Spec : InDefinition->Objectives)
	{
		FGSTN_ObjectiveState& Objective = Objectives.AddDefaulted_GetRef();
		Objective.ObjectiveTag = Spec.ObjectiveTag;
		Objective.Target = Spec.Rule ? FMath::Max(1, Spec.Rule->GetTargetCount()) : 1;
	}
	UnlockReady(Now, OutChanges);
	EvaluateMission(OutChanges);
}

void FGSTN_MissionTracker::SetObjectiveState(int32 Index, EGSTN_ObjectiveState NewState, float Now, TArray<FGSTN_MissionChange>& OutChanges)
{
	FGSTN_ObjectiveState& Objective = Objectives[Index];
	if (Objective.State == NewState)
	{
		return;
	}
	Objective.State = NewState;
	FGSTN_MissionChange Change;
	Change.Objective = Index;
	Change.Progress = Objective.Progress;
	switch (NewState)
	{
	case EGSTN_ObjectiveState::Active:
	{
		const UGSTN_ObjectiveRule* Rule = Definition.IsValid() ? Definition->Objectives[Index].Rule.Get() : nullptr;
		Objective.TimerEnd = Rule && Rule->TimerSeconds > 0.f ? Now + Rule->TimerSeconds : -1.f;
		Change.EventType = GSTNMissionTags::Message_Mission_ObjectiveActivated;
		break;
	}
	case EGSTN_ObjectiveState::Completed:
		Objective.TimerEnd = -1.f;
		Change.EventType = GSTNMissionTags::Message_Mission_ObjectiveCompleted;
		break;
	case EGSTN_ObjectiveState::Failed:
		Objective.TimerEnd = -1.f;
		Change.EventType = GSTNMissionTags::Message_Mission_ObjectiveFailed;
		break;
	default:
		return;
	}
	OutChanges.Add(Change);
}

void FGSTN_MissionTracker::SetProgress(int32 Index, int32 NewProgress, float Now, TArray<FGSTN_MissionChange>& OutChanges)
{
	FGSTN_ObjectiveState& Objective = Objectives[Index];
	NewProgress = FMath::Clamp(NewProgress, 0, Objective.Target);
	if (NewProgress == Objective.Progress)
	{
		return;
	}
	Objective.Progress = NewProgress;
	FGSTN_MissionChange Change;
	Change.Objective = Index;
	Change.Progress = NewProgress;
	Change.EventType = GSTNMissionTags::Message_Mission_ObjectiveProgressed;
	OutChanges.Add(Change);
	if (NewProgress >= Objective.Target)
	{
		SetObjectiveState(Index, EGSTN_ObjectiveState::Completed, Now, OutChanges);
	}
}

void FGSTN_MissionTracker::UnlockReady(float Now, TArray<FGSTN_MissionChange>& OutChanges)
{
	const UGSTN_MissionDefinition* Def = Definition.Get();
	if (!Def || State != EGSTN_MissionState::Running)
	{
		return;
	}
	auto IsCompleted = [this](const FGameplayTag& Tag)
	{
		return Objectives.ContainsByPredicate([&Tag](const FGSTN_ObjectiveState& O) { return O.ObjectiveTag == Tag && O.State == EGSTN_ObjectiveState::Completed; });
	};
	// One pass is enough: an objective unlocked now is Active, not Completed, so it cannot unlock another in the same pass
	int32 PreviousRequired = INDEX_NONE;
	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		const FGSTN_ObjectiveSpec& Spec = Def->Objectives[Index];
		if (Objectives[Index].State == EGSTN_ObjectiveState::Locked)
		{
			bool bReady = true;
			for (const FGameplayTag& Tag : Spec.Prerequisites)
			{
				bReady &= IsCompleted(Tag);
			}
			if (Def->Order == EGSTN_MissionOrder::Sequential && PreviousRequired != INDEX_NONE)
			{
				bReady &= Objectives[PreviousRequired].State == EGSTN_ObjectiveState::Completed;
			}
			if (bReady)
			{
				SetObjectiveState(Index, EGSTN_ObjectiveState::Active, Now, OutChanges);
			}
		}
		if (!Spec.bOptional)
		{
			PreviousRequired = Index;
		}
	}
}

void FGSTN_MissionTracker::EvaluateMission(TArray<FGSTN_MissionChange>& OutChanges)
{
	const UGSTN_MissionDefinition* Def = Definition.Get();
	if (!Def || State != EGSTN_MissionState::Running)
	{
		return;
	}
	bool bAllDone = true;
	bool bFailed = false;
	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		if (Def->Objectives[Index].bOptional)
		{
			continue;
		}
		bAllDone &= Objectives[Index].State == EGSTN_ObjectiveState::Completed;
		bFailed |= Objectives[Index].State == EGSTN_ObjectiveState::Failed;
	}
	if (bFailed || bAllDone)
	{
		State = bFailed ? EGSTN_MissionState::Failed : EGSTN_MissionState::Completed;
		FGSTN_MissionChange Change;
		Change.EventType = bFailed ? GSTNMissionTags::Message_Mission_MissionFailed : GSTNMissionTags::Message_Mission_MissionCompleted;
		OutChanges.Add(Change);
	}
}

bool FGSTN_MissionTracker::IsDuplicate(const FGSTN_SimEvent& Event)
{
	if (!bMergeSameFrame || Event.Frame == INDEX_NONE)
	{
		return false;
	}
	const void* Target = Event.Target.Get();
	for (const FEventKey& Key : RecentEvents)
	{
		if (Key.Frame == Event.Frame && Key.Target == Target && Key.Type == Event.EventType)
		{
			return true;
		}
	}
	if (RecentEvents.Num() >= 64)
	{
		RecentEvents.RemoveAt(0);
	}
	RecentEvents.Add({ Event.EventType, Target, Event.Frame });
	return false;
}

void FGSTN_MissionTracker::HandleEvent(const FGSTN_SimEvent& Event, float Now, TArray<FGSTN_MissionChange>& OutChanges)
{
	const UGSTN_MissionDefinition* Def = Definition.Get();
	if (!Def || State != EGSTN_MissionState::Running || IsDuplicate(Event))
	{
		return;
	}
	if (GSTNMissionLogic::QueryMatches(Def->FailQuery, Event))
	{
		State = EGSTN_MissionState::Failed;
		FGSTN_MissionChange Change;
		Change.EventType = GSTNMissionTags::Message_Mission_MissionFailed;
		OutChanges.Add(Change);
		return;
	}
	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		const UGSTN_ObjectiveRule* Rule = Def->Objectives[Index].Rule.Get();
		if (!Rule || Objectives[Index].State != EGSTN_ObjectiveState::Active)
		{
			continue;
		}
		if (GSTNMissionLogic::QueryMatches(Rule->FailQuery, Event))
		{
			SetObjectiveState(Index, EGSTN_ObjectiveState::Failed, Now, OutChanges);
			continue;
		}
		const int32 Delta = Rule->OnEvent(Event, Objectives[Index]);
		if (Delta != 0)
		{
			SetProgress(Index, Objectives[Index].Progress + Delta, Now, OutChanges);
		}
	}
	UnlockReady(Now, OutChanges);
	EvaluateMission(OutChanges);
}

void FGSTN_MissionTracker::Poll(const FGSTN_ObjectivePollContext& Context, TArray<FGSTN_MissionChange>& OutChanges)
{
	const UGSTN_MissionDefinition* Def = Definition.Get();
	if (!Def || State != EGSTN_MissionState::Running)
	{
		return;
	}
	if (MissionEnd >= 0.f && Context.Now >= MissionEnd)
	{
		State = EGSTN_MissionState::Failed;
		FGSTN_MissionChange Change;
		Change.EventType = GSTNMissionTags::Message_Mission_MissionFailed;
		OutChanges.Add(Change);
		return;
	}
	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		const UGSTN_ObjectiveRule* Rule = Def->Objectives[Index].Rule.Get();
		FGSTN_ObjectiveState& Objective = Objectives[Index];
		if (!Rule || Objective.State != EGSTN_ObjectiveState::Active)
		{
			continue;
		}
		if (Objective.TimerEnd >= 0.f && Context.Now >= Objective.TimerEnd)
		{
			if (Rule->bTimerCompletes)
			{
				Objective.Progress = Objective.Target;
			}
			SetObjectiveState(Index, Rule->bTimerCompletes ? EGSTN_ObjectiveState::Completed : EGSTN_ObjectiveState::Failed, Context.Now, OutChanges);
			continue;
		}
		const int32 NewProgress = Rule->OnPoll(Context, Objective);
		if (NewProgress >= 0)
		{
			SetProgress(Index, NewProgress, Context.Now, OutChanges);
		}
	}
	UnlockReady(Context.Now, OutChanges);
	EvaluateMission(OutChanges);
}

void FGSTN_MissionTracker::CompleteActive(float Now, TArray<FGSTN_MissionChange>& OutChanges)
{
	for (int32 Index = 0; Index < Objectives.Num(); ++Index)
	{
		if (Objectives[Index].State == EGSTN_ObjectiveState::Active)
		{
			SetProgress(Index, Objectives[Index].Target, Now, OutChanges);
		}
	}
	UnlockReady(Now, OutChanges);
	EvaluateMission(OutChanges);
}

FString FGSTN_MissionTracker::Describe() const
{
	const UGSTN_MissionDefinition* Def = Definition.Get();
	FString Text = FString::Printf(TEXT("%s: %s"), Def ? *Def->GetName() : TEXT("?"), *UEnum::GetValueAsString(State));
	for (const FGSTN_ObjectiveState& Objective : Objectives)
	{
		Text += FString::Printf(TEXT(" | %s %d/%d %s"), *Objective.ObjectiveTag.ToString(), Objective.Progress, Objective.Target, *UEnum::GetValueAsString(Objective.State));
	}
	return Text;
}
