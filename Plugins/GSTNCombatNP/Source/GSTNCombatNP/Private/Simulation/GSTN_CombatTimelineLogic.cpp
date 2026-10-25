// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_CombatTimelineLogic.h"

DEFINE_LOG_CATEGORY_STATIC(LogGSTNCombatTimeline, Log, All);

namespace
{
	/** The request array is small on purpose; a full one is a bug in the tick (more requests in a frame than the design has), not a state to live with */
	void AddRequest(FGSTN_CombatSyncCore& Core, EGSTN_RequestType Type, int32 Frame, int16 Arg)
	{
		if (!Core.Requests.Add(Type, Frame, Arg))
		{
			UE_LOG(LogGSTNCombatTimeline, Error, TEXT("Frame %d: the request array is full, dropped request type %d arg %d"), Frame, static_cast<int32>(Type), static_cast<int32>(Arg));
		}
	}

	/** The timeline that was running is replaced or cancelled: its commit of this very frame (the end of its override window) never happens, and its slot is free */
	void DropOldCommit(FGSTN_CombatSyncCore& Core, int32 Frame)
	{
		if (Core.ActiveTimeline != INDEX_NONE)
		{
			Core.Requests.Remove(EGSTN_RequestType::CommitTimeline, Frame, Core.ActiveTimeline);
		}
	}

	void BeginState(FGSTN_CombatSyncCore& Core, int32 Frame, int32 TimelineIndex, EGSTN_ActivationSource Source)
	{
		Core.ActionState = EGSTN_ActionState::InTimeline;
		Core.ActiveTimeline = static_cast<int16>(TimelineIndex);
		Core.ActiveSource = Source;
		Core.TimelineStartFrame = Frame;
		Core.TimelinePausedFrames = 0;
		Core.RecoilStartFrame = INDEX_NONE;
		// The hits of the previous run do not count for this one; the caller writes the move origin
		Core.Hit.ResetRun();
	}

	void EvaluateInto(const FGSTN_CombatSyncCore& Core, int32 Frame, int32 PreviousFrame, const FGSTN_CombatTimelineData& Data, int32 DefaultOverrideFrames, FGSTN_TimelineOutput& Output)
	{
		FGSTN_TimelineEvalParams Params;
		Params.Frame = Frame - Core.TimelineStartFrame;
		Params.PausedFrames = Core.TimelinePausedFrames;
		Params.PreviousFrame = PreviousFrame;
		Params.DefaultOverrideFrames = DefaultOverrideFrames;
		FGSTN_TimelinePlayer::Evaluate(Data, Params, Output);
	}

	bool ConditionHolds(EGSTN_CancelCondition Condition, const FGSTN_CombatSyncCore& Core)
	{
		// The confirm flags are the attacker side's (M4 H8): a hit of the running timeline sets one, a new timeline clears them.
		// OnBlock stays false until shields exist (M6)
		const bool bHit = Core.Hit.HitConfirmFlags != 0;
		switch (Condition)
		{
		case EGSTN_CancelCondition::Always:
			return true;
		case EGSTN_CancelCondition::OnWhiff:
			return !bHit;
		case EGSTN_CancelCondition::OnHit:
			return bHit;
		default:
			return false;
		}
	}
}

void FGSTN_CombatTimelineLogic::ClearTimeline(FGSTN_CombatSyncCore& Core)
{
	Core.ActionState = EGSTN_ActionState::Actionable;
	Core.ActiveTimeline = INDEX_NONE;
	Core.ActiveSource = EGSTN_ActivationSource::Command;
	Core.TimelineStartFrame = INDEX_NONE;
	Core.TimelinePausedFrames = 0;
	Core.RecoilStartFrame = INDEX_NONE;
}

bool FGSTN_CombatTimelineLogic::StartRecoil(FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData* Data, const FGSTN_RicochetResponse* Fallback)
{
	if (Core.ActionState != EGSTN_ActionState::InTimeline || Core.RecoilStartFrame != INDEX_NONE || !ResolveRicochet(Data, Fallback))
	{
		return false;
	}
	// The tick's clock step of this frame has run already: the recoil is evaluated from the next one
	Core.RecoilStartFrame = Frame + 1;
	return true;
}

const FGSTN_RicochetResponse* FGSTN_CombatTimelineLogic::ResolveRicochet(const FGSTN_CombatTimelineData* Data, const FGSTN_RicochetResponse* Fallback)
{
	if (Data)
	{
		if (const FGSTN_RicochetResponse* Own = Data->GetRicochetResponse())
		{
			return Own;
		}
	}
	return Fallback && Fallback->IsAuthored() ? Fallback : nullptr;
}

bool FGSTN_CombatTimelineLogic::AdvanceClock(FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData* Data, int32 DefaultOverrideFrames,
	bool bAbilityActive, FGSTN_TimelineOutput& Output, bool bHitstopFrame, const FGSTN_RicochetResponse* RicochetFallback)
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

	if (Core.RecoilStartFrame != INDEX_NONE)
	{
		// The recoil replaces the timeline (the response is resolved by the attacker side before it writes the start; a response that is gone
		// ends the recoil as if it were over). A hitstop frame moves the start a frame later, which keeps the recoil frame where it was
		const FGSTN_RicochetResponse* Response = ResolveRicochet(Data, RicochetFallback);
		const int32 PreviousRecoil = Frame - 1 - Core.RecoilStartFrame;
		if (bHitstopFrame)
		{
			++Core.RecoilStartFrame;
		}
		const int32 RecoilFrame = Frame - Core.RecoilStartFrame;
		if (!Response)
		{
			ClearTimeline(Core);
			return false;
		}
		if (RecoilFrame < 0)
		{
			// Hitstop on the very frame the recoil was to begin: it begins when the hitstop is out
			Output.TotalFrames = Response->RecoilFrames;
			Output.bRecoil = true;
			Output.bBusy = true;
			return true;
		}
		FGSTN_TimelineEvalParams Params;
		Params.bRicochet = true;
		Params.RecoilFrame = RecoilFrame;
		Params.PreviousRecoilFrame = PreviousRecoil >= 0 ? PreviousRecoil : INDEX_NONE;
		Params.RicochetFallback = RicochetFallback;
		FGSTN_TimelinePlayer::Evaluate(*Data, Params, Output);
		if (Output.bFinished || (Frame > Core.TimelineStartFrame && !bAbilityActive))
		{
			ClearTimeline(Core);
			return false;
		}
		return true;
	}

	// The previous evaluation was the frame before this one (the start frame was evaluated by EvaluateStart)
	// (the pause count as of that frame, so before this frame's own pause is added)
	const int32 Previous = Frame - 1 - Core.TimelineStartFrame - Core.TimelinePausedFrames;
	if (bHitstopFrame)
	{
		++Core.TimelinePausedFrames;
	}
	EvaluateInto(Core, Frame, Previous >= 0 ? Previous : INDEX_NONE, *Data, DefaultOverrideFrames, Output);

	if (Output.bFinished || (Frame > Core.TimelineStartFrame && !bAbilityActive))
	{
		ClearTimeline(Core);
		return false;
	}

	if (Output.bCommitThisFrame)
	{
		AddRequest(Core, EGSTN_RequestType::CommitTimeline, Frame, Core.ActiveTimeline);
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
		if (Window && ConditionHolds(Window->Condition, Core) && Window->IntoCommands.Matches(CommandTags))
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
		DropOldCommit(Core, Frame);
		AddRequest(Core, EGSTN_RequestType::CancelTimeline, Frame, Core.ActiveTimeline);
	}
	BeginState(Core, Frame, TimelineIndex, Source);
}

void FGSTN_CombatTimelineLogic::CancelForOtherCommand(FGSTN_CombatSyncCore& Core, int32 Frame)
{
	if (Core.ActionState != EGSTN_ActionState::InTimeline)
	{
		return;
	}
	if (Core.ActiveTimeline != INDEX_NONE)
	{
		DropOldCommit(Core, Frame);
		AddRequest(Core, EGSTN_RequestType::CancelTimeline, Frame, Core.ActiveTimeline);
	}
	ClearTimeline(Core);
}

int32 FGSTN_CombatTimelineLogic::Adopt(FGSTN_CombatSyncCore& Core, int32 Frame, TConstArrayView<int32> Awaiting)
{
	if (Awaiting.IsEmpty())
	{
		return INDEX_NONE;
	}
	// The winner is the running timeline's own class (a retrigger): its ability was restarted by the activation that is being
	// adopted, so cancelling "the old entry" would cancel the newcomer. Only the state is rewritten with the new start frame
	if (Core.ActionState == EGSTN_ActionState::InTimeline && Core.ActiveTimeline == Awaiting[0])
	{
		DropOldCommit(Core, Frame);
		BeginState(Core, Frame, Awaiting[0], EGSTN_ActivationSource::Adopted);
	}
	else
	{
		// Start first: its CancelTimeline matters more than a loser's cancel if the small request array runs out (the losers then
		// end by themselves when they have waited too long)
		Start(Core, Frame, Awaiting[0], EGSTN_ActivationSource::Adopted);
	}
	for (int32 Index = 1; Index < Awaiting.Num(); ++Index)
	{
		AddRequest(Core, EGSTN_RequestType::CancelAbility, Frame, static_cast<int16>(Awaiting[Index]));
	}
	return Awaiting[0];
}

void FGSTN_CombatTimelineLogic::EvaluateStart(FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData& Data, int32 DefaultOverrideFrames, FGSTN_TimelineOutput& Output)
{
	EvaluateInto(Core, Frame, INDEX_NONE, Data, DefaultOverrideFrames, Output);
	if (Output.bCommitThisFrame)
	{
		AddRequest(Core, EGSTN_RequestType::CommitTimeline, Frame, Core.ActiveTimeline);
	}
}
