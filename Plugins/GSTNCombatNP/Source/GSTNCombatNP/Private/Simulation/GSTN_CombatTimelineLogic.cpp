// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_CombatTimelineLogic.h"

#include "Input/GSTN_ButtonMap.h"
#include "Simulation/GSTN_TimelineTrackHook.h"
#include "Timeline/GSTN_ChargeHold.h"
#include "Timeline/GSTN_HoldUntilLanded.h"
#include "Timeline/GSTN_Landing.h"
#include "Timeline/GSTN_Ledge.h"
#include "Timeline/GSTN_StartupSteer.h"
#include "Events/GSTN_SimEvent.h"
#include "GameFramework/Actor.h"

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
		Core.ChargeFrames = 0;
		Core.RecoilStartFrame = INDEX_NONE;
		Core.HardContact = FGSTN_HardContactState();
	Core.StrikeTrack = INDEX_NONE;
		Core.bAirborneOk = false;
		Core.bLedgeStop = false;
		Core.bTumbleActionable = false;
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

	bool ConditionHolds(EGSTN_CancelCondition Condition, const FGSTN_CombatSyncCore& Core, uint32 ExtensionConditions)
	{
		// The confirm flags are the attacker side's (M4 H8): a hit of the running timeline sets one, a new timeline clears them.
		// The conditions the core does not know (OnParry, OnBlock) are answered by the tick extensions of the frame: one bit per
		// condition value, 0 where no plugin holds it (M6 F7)
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
			return (ExtensionConditions & (1u << static_cast<uint32>(Condition))) != 0;
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
	Core.ChargeFrames = 0;
	Core.RecoilStartFrame = INDEX_NONE;
	Core.HardContact = FGSTN_HardContactState();
	Core.StrikeTrack = INDEX_NONE;
	Core.bAirborneOk = false;
	Core.bLedgeStop = false;
	Core.bTumbleActionable = false;
}

bool FGSTN_CombatTimelineLogic::StartRecoil(FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData* Data, const FGSTN_RicochetResponse* Fallback)
{
	if (Core.ActionState != EGSTN_ActionState::InTimeline || Core.RecoilStartFrame != INDEX_NONE || !ResolveRicochet(Data, Core.StrikeTrack, Fallback))
	{
		return false;
	}
	// The tick's clock step of this frame has run already: the recoil is evaluated from the next one
	Core.RecoilStartFrame = Frame + 1;
	return true;
}

const FGSTN_RicochetResponse* FGSTN_CombatTimelineLogic::ResolveRicochet(const FGSTN_CombatTimelineData* Data, int32 StrikeTrack, const FGSTN_RicochetResponse* Fallback)
{
	if (Data)
	{
		if (const FGSTN_RicochetResponse* Own = Data->GetRicochetResponse(StrikeTrack))
		{
			return Own;
		}
	}
	return Fallback && Fallback->IsAuthored() ? Fallback : nullptr;
}

bool FGSTN_CombatTimelineLogic::AdvanceClock(FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData* Data, int32 DefaultOverrideFrames,
	bool bAbilityActive, FGSTN_TimelineOutput& Output, bool bHitstopFrame, const FGSTN_RicochetResponse* RicochetFallback, const FGSTN_TrackHookEnvironment* HookEnvironment)
{
	Output.Reset();
	// The Ledge Check's Stop holds only for the tick that saw the ledge (below)
	Core.bLedgeStop = false;
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
		const FGSTN_RicochetResponse* Response = ResolveRicochet(Data, Core.StrikeTrack, RicochetFallback);
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
		Params.StrikeTrack = Core.StrikeTrack;
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

	// FinishThenRecoil (plan precisione-weakpoint 2): the swing that met a hard face has played its active frames, the recoil replaces the
	// recovery from here. A hitstop frame keeps the timeline where it is, so the check waits for a frame that advances
	if (Core.HardContact.bRecoilPending && !bHitstopFrame && Previous + 1 >= Data->GetStrikeEndFrame(Core.StrikeTrack))
	{
		Core.HardContact.bRecoilPending = false;
		if (ResolveRicochet(Data, Core.StrikeTrack, RicochetFallback))
		{
			Core.RecoilStartFrame = Frame;
			return AdvanceClock(Core, Frame, Data, DefaultOverrideFrames, bAbilityActive, Output, false, RicochetFallback, HookEnvironment);
		}
	}

	// Ground rules (owner 2026-11-16, FGSTN_LandingRules): a Ground move found in the air outside its take-off ends; a landing inside a Landing
	// window autocancels, starts the landing lag (played by the recoil player) or hands over to the continuation move. Before Hold Until Landed,
	// so a Landing window wins over a hold's own land jump. Not on a hitstop frame, only with Mover's ground fact
	if (!bHitstopFrame && Previous >= 0 && HookEnvironment && HookEnvironment->Ground != EGSTN_TrackHookGround::Unknown
		&& (Data->GroundRule != EGSTN_MoveGroundRule::Any || FGSTN_LandingRules::HasLandingTracks(*Data)))
	{
		const FGSTN_GroundClockDecision Ground = FGSTN_LandingRules::Resolve(*Data, Previous + 1, HookEnvironment->Ground == EGSTN_TrackHookGround::OnGround,
			Previous == 0, Core.bAirborneOk);
		Core.bAirborneOk = Ground.bAirborneOk;
		if (Ground.Action == EGSTN_GroundClockAction::CancelAirborne)
		{
			UE_LOG(LogGSTNCombatTimeline, Verbose, TEXT("Frame %d: ground move %d in the air at timeline frame %d: cancelled"), Frame, static_cast<int32>(Core.ActiveTimeline), Previous + 1);
			ClearTimeline(Core);
			return false;
		}
		if (Ground.Action == EGSTN_GroundClockAction::Land)
		{
			const FGSTN_TimelineTrack_Landing& Landing = Data->Tracks[Ground.TrackIndex].Get<FGSTN_TimelineTrack_Landing>();
			UE_LOG(LogGSTNCombatTimeline, Verbose, TEXT("Frame %d: move %d landed at timeline frame %d, action %d"), Frame, static_cast<int32>(Core.ActiveTimeline), Previous + 1,
				static_cast<int32>(Landing.Action));
			if (Landing.Action == EGSTN_LandingAction::LandingLag && Landing.Lag.IsAuthored())
			{
				// The lag replaces the rest of the move from this frame, like a recoil (its response is the track's own: GetRicochetResponse)
				Core.HardContact = FGSTN_HardContactState();
				Core.StrikeTrack = static_cast<int16>(Ground.TrackIndex);
				Core.RecoilStartFrame = Frame;
				return AdvanceClock(Core, Frame, Data, DefaultOverrideFrames, bAbilityActive, Output, false, RicochetFallback, HookEnvironment);
			}
			// AutoCancel, or the continuation (the combat model starts it on this frame; a move it cannot find ends like an autocancel)
			ClearTimeline(Core);
			if (Landing.Action == EGSTN_LandingAction::ContinueWithMove)
			{
				Output.LandingContinueTrack = Ground.TrackIndex;
			}
			return false;
		}
	}

	// Ledge Check (owner 2026-11-16, FGSTN_LedgeRules): on the ground inside a ledge window with no floor ahead, the move stops in place (the ledge
	// guard move reads bLedgeStop in Mover's tick of this frame), ends, recoils into the track's response or hands over to its continuation.
	// Cleared at the top of every tick, so Stop lasts exactly as long as the void is ahead inside the window. Not on a hitstop frame (the freeze
	// holds the pawn)
	if (!bHitstopFrame && Previous >= 0 && HookEnvironment && HookEnvironment->Ground == EGSTN_TrackHookGround::OnGround && HookEnvironment->HasFloorAhead
		&& FGSTN_LedgeRules::HasLedgeTracks(*Data))
	{
		int32 LedgeTrack = INDEX_NONE;
		const FGSTN_TimelineTrack_LedgeCheck* Ledge = FGSTN_LedgeRules::FindWindow(*Data, Previous + 1, &LedgeTrack);
		if (Ledge && !HookEnvironment->HasFloorAhead(Ledge->ProbeAhead, Ledge->ProbeDepth))
		{
			UE_LOG(LogGSTNCombatTimeline, Verbose, TEXT("Frame %d: move %d sees a ledge at timeline frame %d, action %d"), Frame, static_cast<int32>(Core.ActiveTimeline), Previous + 1,
				static_cast<int32>(Ledge->Action));
			if (Ledge->Action == EGSTN_LedgeAction::Stop)
			{
				Core.bLedgeStop = true;
			}
			else if (Ledge->Action == EGSTN_LedgeAction::Recoil && Ledge->Response.IsAuthored())
			{
				// The response replaces the rest of the move from this frame, like a landing lag (GetRicochetResponse of the track's index)
				Core.HardContact = FGSTN_HardContactState();
				Core.StrikeTrack = static_cast<int16>(LedgeTrack);
				Core.RecoilStartFrame = Frame;
				return AdvanceClock(Core, Frame, Data, DefaultOverrideFrames, bAbilityActive, Output, false, RicochetFallback, HookEnvironment);
			}
			else
			{
				// EndMove, an unauthored Recoil, or the continuation (the combat model starts it on this frame; one it cannot find ends like EndMove)
				const bool bContinue = Ledge->Action == EGSTN_LedgeAction::ContinueWithMove;
				ClearTimeline(Core);
				if (bContinue)
				{
					Output.LandingContinueTrack = LedgeTrack;
				}
				return false;
			}
		}
	}

	// Hold Until Landed (FGSTN_TimelineTrack_HoldUntilLanded, the dropkick): in the air on the hold's last frame the timeline stands there; on the
	// ground it jumps to the land frame. Decided before the hooks, so they see the frame the timeline is on after a jump. Not on a hitstop frame (the
	// timeline stands already; the landing is taken on the first frame after it), and only with Mover's ground fact
	int32 ClockPrevious = Previous;
	bool bStall = bHitstopFrame;
	if (!bHitstopFrame && Previous >= 0 && HookEnvironment && HookEnvironment->Ground != EGSTN_TrackHookGround::Unknown)
	{
		const FGSTN_HoldClockDecision Hold = FGSTN_HoldUntilLanded::ResolveClock(*Data, Previous, true, HookEnvironment->Ground == EGSTN_TrackHookGround::OnGround,
			Frame - Core.TimelineStartFrame);
		if (Hold.Action == EGSTN_HoldClockAction::Timeout)
		{
			// Air timeout (owner 2026-11-16): the move ends here like an interrupt, not "by itself" (bFinished stays false, so no follow-up and
			// no montage release): nothing of it is evaluated this frame, so its hitbox is closed, and the character is back in air idle
			UE_LOG(LogGSTNCombatTimeline, Verbose, TEXT("Frame %d: timeline %d still in the air after %d frames: air timeout, the move ends"), Frame,
				static_cast<int32>(Core.ActiveTimeline), Frame - Core.TimelineStartFrame);
			ClearTimeline(Core);
			return false;
		}
		if (Hold.Action == EGSTN_HoldClockAction::Stall)
		{
			bStall = true;
		}
		else if (Hold.Action == EGSTN_HoldClockAction::Land)
		{
			// The frames in between are skipped: the paused count goes down by as many (below zero when the move lands before it ever stood), and
			// the evaluation takes the frame before the land frame as the previous one, so nothing keyed inside the skipped frames fires
			Core.TimelinePausedFrames -= Hold.TargetFrame - (Previous + 1);
			ClockPrevious = Hold.TargetFrame - 1;
		}
	}

	// Charge Hold (owner 2026-11-25, FGSTN_TimelineTrack_ChargeHold): the move's button held without a break since the start stands the timeline on the
	// hold frame, up to the track's cap; ChargeFrames counts it for the damage bonus. The button is read from the input of the end of the previous frame,
	// as the shield's hold reads it. Not on a hitstop frame (the timeline stands already) and never on a frame a Hold Until Landed jumped
	bool bChargeStall = false;
	bool bChargeRelease = false;
	bool bChargeResume = false;
	int32 ChargeTrackIndex = INDEX_NONE;
	const FGSTN_TimelineTrack_ChargeHold* ChargeTrack = FGSTN_ChargeHold::FindTrack(*Data, &ChargeTrackIndex);
	if (ChargeTrack && !bHitstopFrame && ClockPrevious == Previous && Previous >= 0)
	{
		const int32 Button = HookEnvironment && HookEnvironment->Buttons ? HookEnvironment->Buttons->GetIndex(ChargeTrack->ResolveHoldButton()) : INDEX_NONE;
		const bool bHeld = FGSTN_ChargeHold::IsHeldSinceStart(Core.Input.History, Button, Core.TimelineStartFrame);
		bChargeStall = FGSTN_ChargeHold::ShouldStall(*ChargeTrack, Previous, Frame - Core.TimelineStartFrame, bHeld);
		// The tick after the hold frame: the montage the stall paused plays on (a tap passes here too: resuming a montage that is not paused is nothing)
		bChargeRelease = !bChargeStall && Previous == ChargeTrack->StartFrame;
		if (bChargeStall)
		{
			bStall = true;
			Core.ChargeFrames = static_cast<uint8>(FMath::Min<int32>(Core.ChargeFrames + 1, MAX_uint8));
		}
	}
	// Whatever ended the hold, the montage the stall paused must play on: a release (above), and also a landing during the hold, where Hold Until
	// Landed jumps the timeline to its land part (ClockPrevious != Previous) and the land montage would otherwise stay paused for the whole landing
	// (desktop network test 2026-11-27). Only the release puts a loop-section montage back on the hold pose (the ability reads bChargeHoldRelease)
	bChargeResume = ChargeTrack && !bHitstopFrame && !bChargeStall && Previous >= 0 && Previous == ChargeTrack->StartFrame;

	// The track hooks of the plugins (a hold stalls the timeline, a parry track stamps a window): asked before the pause is counted, with the frame the timeline is on.
	// A stall is a pause like the hitstop's, but it does not touch PausedFramesTotal, so the combat clock and the buffers keep running
	FGSTN_StampedWindowList Stamped;
	const FGSTN_TimelineTrackHookRegistry& Hooks = FGSTN_TimelineTrackHookRegistry::Get();
	if (!Hooks.IsEmpty())
	{
		bStall |= Hooks.Run(*Data, Frame, Core.TimelineStartFrame, FMath::Max(ClockPrevious, 0), bHitstopFrame, Core.Input, Stamped, HookEnvironment);
	}
	if (bStall)
	{
		++Core.TimelinePausedFrames;
	}
	EvaluateInto(Core, Frame, ClockPrevious >= 0 ? ClockPrevious : INDEX_NONE, *Data, DefaultOverrideFrames, Output);
	Output.StampedWindows = MoveTemp(Stamped);
	Output.bChargeHoldStall = bChargeStall;
	Output.bChargeHoldRelease = bChargeRelease;
	Output.bChargeHoldResume = bChargeResume;

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
	const FGameplayTag& Command, int32 CommandTier, int32 RunningTier, uint32 ExtensionConditions)
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
		if (Window && ConditionHolds(Window->Condition, Core, ExtensionConditions) && Window->IntoCommands.Matches(CommandTags))
		{
			return true;
		}
	}
	return false;
}

bool FGSTN_CombatTimelineLogic::IsConditionMet(EGSTN_CancelCondition Condition, const FGSTN_CombatSyncCore& Core, uint32 ExtensionConditions)
{
	return ConditionHolds(Condition, Core, ExtensionConditions);
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

bool FGSTN_CombatTimelineLogic::ShouldStickCutTail(const FGSTN_CombatSyncCore& Core, const FGSTN_CombatTimelineData* Data, const FGSTN_TimelineOutput& Output,
	bool bStickOutOfNeutral)
{
	// A recoil or a landing lag is not the move's tail (bTailCancel is false there anyway): only the tail the data names
	return bStickOutOfNeutral && Data && Data->Tail.bHasTail && Data->Tail.bStickCuts && Core.ActionState == EGSTN_ActionState::InTimeline
		&& Core.RecoilStartFrame == INDEX_NONE && Output.bTailCancel;
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

void FGSTN_CombatTimelineLogic::EvaluateStart(FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData& Data, int32 DefaultOverrideFrames, FGSTN_TimelineOutput& Output, const FGSTN_TrackHookEnvironment* HookEnvironment)
{
	EvaluateInto(Core, Frame, INDEX_NONE, Data, DefaultOverrideFrames, Output);
	// Hooks see the first frame too, so a window that opens on it is stamped on it. A stall is not honoured here: the evaluation of frame 0 is done and the
	// next tick's previous-frame arithmetic assumes no pause was counted since (a hold on frame 0 starts with the tick after)
	const FGSTN_TimelineTrackHookRegistry& Hooks = FGSTN_TimelineTrackHookRegistry::Get();
	if (!Hooks.IsEmpty())
	{
		FGSTN_StampedWindowList Stamped;
		Hooks.Run(Data, Frame, Core.TimelineStartFrame, 0, false, Core.Input, Stamped, HookEnvironment);
		Output.StampedWindows = MoveTemp(Stamped);
	}
	if (Output.bCommitThisFrame)
	{
		AddRequest(Core, EGSTN_RequestType::CommitTimeline, Frame, Core.ActiveTimeline);
	}
}

int32 FGSTN_CombatTimelineLogic::AppendOutboxEvents(const FGSTN_TimelineOutput& Output, int32 Frame, AActor* Instigator, FGSTN_SimEventOutbox& Outbox)
{
	int32 Appended = 0;
	for (const FGSTN_TimelineTrack_Event* Key : Output.Events)
	{
		if (!Key || !Key->OutboxEventTag.IsValid())
		{
			continue;
		}
		FGSTN_SimEvent Event;
		Event.EventType = Key->OutboxEventTag;
		Event.Tags = Key->OutboxTags;
		Event.Magnitude = Key->OutboxMagnitude;
		Event.Frame = Frame;
		Event.Instigator = Instigator;
		Outbox.Add(MoveTemp(Event));
		++Appended;
	}
	return Appended;
}
bool FGSTN_CombatTimelineLogic::ResolveSteerHoldYaw(const FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData* Data, const FVector2D& AimStick,
	float CameraYawDeg, bool bPrecisionAim, float& OutYawDeg)
{
	if (!bPrecisionAim || !Data || Core.ActionState != EGSTN_ActionState::InTimeline || Core.RecoilStartFrame != INDEX_NONE || !Core.Hit.MoveOrigin.bSet)
	{
		return false;
	}
	const int32 TimelineFrame = Frame - Core.TimelineStartFrame - Core.TimelinePausedFrames;
	if (TimelineFrame < 0 || TimelineFrame >= FGSTN_StartupSteer::GetSteerEndFrame(*Data))
	{
		return false;
	}
	// The move origin keeps the yaw at 65536 steps per turn in a uint16: the same bits as the request's int16
	const float StartYaw = FGSTN_StartupSteer::DequantizeYaw(static_cast<int16>(Core.Hit.MoveOrigin.RotationQ[1]));
	OutYawDeg = FGSTN_StartupSteer::SteerYaw(StartYaw, AimStick, CameraYawDeg, Data->MaxSteerDegrees, Data->Steer.StickDeadZone);
	return true;
}

EGSTN_SteerPhase FGSTN_CombatTimelineLogic::ResolveSteerFrame(const FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData* Data,
	const FVector2D& MoveStick, const FVector2D& AimStick, bool bPrecisionAim, float CameraYawDeg, float CurrentYawDeg, float StepSeconds, float& OutYawDeg,
	bool& OutStickActive)
{
	OutStickActive = false;
	if (!Data || Core.ActionState != EGSTN_ActionState::InTimeline || Core.RecoilStartFrame != INDEX_NONE || !Core.Hit.MoveOrigin.bSet)
	{
		return EGSTN_SteerPhase::None;
	}
	FGSTN_SteerFrameInput In;
	In.TimelineFrame = Frame - Core.TimelineStartFrame - Core.TimelinePausedFrames;
	// The move origin keeps the yaw at 65536 steps per turn in a uint16: the same bits as the request's int16
	In.StartYawDeg = FGSTN_StartupSteer::DequantizeYaw(static_cast<int16>(Core.Hit.MoveOrigin.RotationQ[1]));
	In.CurrentYawDeg = CurrentYawDeg;
	In.bSourceActive = FGSTN_StartupSteer::PickStick(Data->Steer.Source, MoveStick, AimStick, bPrecisionAim, In.Stick, Data->Steer.StickDeadZone);
	In.CameraYawDeg = CameraYawDeg;
	In.StepSeconds = StepSeconds;
	return FGSTN_StartupSteer::ResolveFrame(*Data, In, OutYawDeg, OutStickActive);
}

bool FGSTN_CombatTimelineLogic::ComputeStartupSteer(const FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData* Data, const FVector2D& AimStick,
	float CameraYawDeg, float CurrentYawDeg, bool bPrecisionAim, int16& OutYawArg, float StepSeconds)
{
	float SteerYawDeg = 0.f;
	if (!Data || !FGSTN_StartupSteer::IsSteering(AimStick, Data->MaxSteerDegrees, Data->Steer.StickDeadZone)
		|| !ResolveSteerHoldYaw(Core, Frame, Data, AimStick, CameraYawDeg, bPrecisionAim, SteerYawDeg))
	{
		return false;
	}
	// Aim handling (M11 L9): the cone is a clamp against the move origin, the rate a step from this frame's pose
	SteerYawDeg = FGSTN_StartupSteer::RateLimitYaw(CurrentYawDeg, SteerYawDeg, Data->SteerRateDegPerSecond, StepSeconds);
	const int16 Target = FGSTN_StartupSteer::QuantizeYaw(SteerYawDeg);
	if (Target == FGSTN_StartupSteer::QuantizeYaw(CurrentYawDeg))
	{
		return false;
	}
	OutYawArg = Target;
	return true;
}
