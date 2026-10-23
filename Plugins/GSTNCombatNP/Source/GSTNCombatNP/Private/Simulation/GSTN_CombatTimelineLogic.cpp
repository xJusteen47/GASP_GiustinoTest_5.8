// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_CombatTimelineLogic.h"

namespace
{
	void EvaluateInto(const FGSTN_CombatSyncCore& Core, int32 Frame, int32 PreviousFrame, const FGSTN_CombatTimelineData& Data, int32 DefaultOverrideFrames, FGSTN_TimelineOutput& Output)
	{
		FGSTN_TimelineEvalParams Params;
		Params.Frame = Frame - Core.TimelineStartFrame;
		Params.PausedFrames = Core.TimelinePausedFrames;
		Params.PreviousFrame = PreviousFrame;
		Params.DefaultOverrideFrames = DefaultOverrideFrames;
		FGSTN_TimelinePlayer::Evaluate(Data, Params, Output);
	}

	bool ConditionHolds(EGSTN_CancelCondition Condition)
	{
		// Nothing can hit before M4: a hit or a block never happened, so "not hit yet" holds
		return Condition == EGSTN_CancelCondition::Always || Condition == EGSTN_CancelCondition::OnWhiff;
	}
}

void FGSTN_CombatTimelineLogic::ClearTimeline(FGSTN_CombatSyncCore& Core)
{
	Core.ActionState = EGSTN_ActionState::Actionable;
	Core.ActiveTimeline = INDEX_NONE;
	Core.ActiveSource = EGSTN_ActivationSource::Command;
	Core.TimelineStartFrame = INDEX_NONE;
	Core.TimelinePausedFrames = 0;
}

bool FGSTN_CombatTimelineLogic::AdvanceClock(FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData* Data, int32 DefaultOverrideFrames,
	bool bAbilityActive, FGSTN_TimelineOutput& Output)
{
	Output.Reset();
	if (Core.ActionState != EGSTN_ActionState::InTimeline)
	{
		return false;
	}
	if (!Data || Core.TimelineStartFrame == INDEX_NONE)
	{
		ClearTimeline(Core);
		return false;
	}

	// The previous evaluation was the frame before this one (the start frame was evaluated by EvaluateStart)
	const int32 Previous = Frame - 1 - Core.TimelineStartFrame - Core.TimelinePausedFrames;
	EvaluateInto(Core, Frame, Previous >= 0 ? Previous : INDEX_NONE, *Data, DefaultOverrideFrames, Output);

	if (Output.bFinished || (Frame > Core.TimelineStartFrame && !bAbilityActive))
	{
		ClearTimeline(Core);
		return false;
	}

	if (Output.bCommitThisFrame)
	{
		Core.Requests.Add(EGSTN_RequestType::CommitTimeline, Frame, Core.ActiveTimeline);
	}
	return true;
}

bool FGSTN_CombatTimelineLogic::IsCommandAllowed(const FGSTN_CombatSyncCore& Core, const FGSTN_CombatTimelineData* Data, const FGSTN_TimelineOutput& Output,
	const FGameplayTag& Command, int32 CommandTier, int32 RunningTier)
{
	if (Core.ActionState != EGSTN_ActionState::InTimeline || !Data)
	{
		return true;
	}
	if (Output.bTailCancel || !Output.bBusy)
	{
		return true;
	}

	// The command and its parents, so a query on Command.Special matches Command.Special.Neutral
	const FGameplayTagContainer CommandTags = Command.GetSingleTagContainer();

	if (Output.bInOverrideWindow)
	{
		const bool bAllowed = Data->OverrideInto.IsEmpty() ? CommandTier > RunningTier : Data->OverrideInto.Matches(CommandTags);
		if (bAllowed)
		{
			return true;
		}
	}

	for (const FGSTN_TimelineTrack_CancelWindow* Window : Output.CancelWindows)
	{
		if (Window && ConditionHolds(Window->Condition) && Window->IntoCommands.Matches(CommandTags))
		{
			return true;
		}
	}
	return false;
}

void FGSTN_CombatTimelineLogic::Start(FGSTN_CombatSyncCore& Core, int32 Frame, int32 TimelineIndex, EGSTN_ActivationSource Source)
{
	if (Core.ActionState == EGSTN_ActionState::InTimeline && Core.ActiveTimeline != INDEX_NONE)
	{
		Core.Requests.Add(EGSTN_RequestType::CancelTimeline, Frame, Core.ActiveTimeline);
	}
	Core.ActionState = EGSTN_ActionState::InTimeline;
	Core.ActiveTimeline = static_cast<int16>(TimelineIndex);
	Core.ActiveSource = Source;
	Core.TimelineStartFrame = Frame;
	Core.TimelinePausedFrames = 0;
}

void FGSTN_CombatTimelineLogic::EvaluateStart(FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData& Data, int32 DefaultOverrideFrames, FGSTN_TimelineOutput& Output)
{
	EvaluateInto(Core, Frame, INDEX_NONE, Data, DefaultOverrideFrames, Output);
	if (Output.bCommitThisFrame)
	{
		Core.Requests.Add(EGSTN_RequestType::CommitTimeline, Frame, Core.ActiveTimeline);
	}
}
