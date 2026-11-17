// Combat simulation (GAS-NPP fork): the combat state and events a Blueprint reads (bp-exposure P1).

#include "Presentation/GSTN_CombatStateTypes.h"

#include "Events/GSTN_SimEvent.h"
#include "GSTN_CombatNPTags.h"
#include "GSTN_CombatTags.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatVictim.h"

FGSTN_CombatStateView FGSTN_CombatStateView::FromCore(const FGSTN_CombatSyncCore& Core)
{
	FGSTN_CombatStateView View;
	const int32 Frame = Core.Input.History.GetLatestFrame();
	if (Frame < 0)
	{
		return View;
	}
	View.bValid = true;
	View.Frame = Frame;

	switch (Core.ActionState)
	{
	case EGSTN_ActionState::InTimeline: View.Status = EGSTN_CombatStatus::InMove; break;
	case EGSTN_ActionState::Hitstun: View.Status = EGSTN_CombatStatus::Hitstun; break;
	case EGSTN_ActionState::Tumble: View.Status = EGSTN_CombatStatus::Tumble; break;
	case EGSTN_ActionState::Splat: View.Status = EGSTN_CombatStatus::Splat; break;
	case EGSTN_ActionState::KO: View.Status = EGSTN_CombatStatus::KnockedOut; break;
	default: View.Status = EGSTN_CombatStatus::Actionable; break;
	}

	// The end-frame stamps of the state: a window is live while its end is in the future of the state's own frame
	View.HitstunFramesLeft = FMath::Max(Core.Hit.HitstunEndFrame - Frame, 0);
	View.bInHitstop = Core.IsInHitstop(Frame);
	View.HitstopFramesLeft = View.bInHitstop ? FMath::Max(Core.HitstopEndFrame - Frame, 0) : 0;
	View.bInTumble = Core.ActionState == EGSTN_ActionState::Tumble;
	View.bTumbleActionable = View.bInTumble && Core.bTumbleActionable;
	View.bInHitstun = Core.ActionState == EGSTN_ActionState::Hitstun || Core.ActionState == EGSTN_ActionState::Splat
		|| (View.bInTumble && !Core.bTumbleActionable && View.HitstunFramesLeft > 0);
	View.bKnockedOut = Core.ActionState == EGSTN_ActionState::KO;
	View.GroundBounceCount = Core.Hit.GroundBounceCount;
	View.WallBounceCount = Core.Hit.WallBounceCount;

	View.bInMove = Core.ActionState == EGSTN_ActionState::InTimeline && Core.ActiveTimeline != INDEX_NONE;
	if (View.bInMove)
	{
		View.MoveEntry = Core.ActiveTimeline;
		View.MoveStartFrame = Core.TimelineStartFrame;
		// The timeline frame of the combat model (combat-m3 7.1): it stands still in hitstop
		View.MoveFrame = Core.TimelineStartFrame >= 0 ? FMath::Max(Frame - Core.TimelineStartFrame - Core.TimelinePausedFrames, 0) : INDEX_NONE;
		View.bInRecoil = Core.RecoilStartFrame != INDEX_NONE;
	}

	const FGSTN_JumpState& Jump = Core.Jump;
	switch (Jump.Phase)
	{
	case EGSTN_JumpPhase::Rising: View.AirState = EGSTN_CombatAirState::Rising; break;
	case EGSTN_JumpPhase::Falling: View.AirState = EGSTN_CombatAirState::Falling; break;
	case EGSTN_JumpPhase::Launched: View.AirState = EGSTN_CombatAirState::Launched; break;
	default: View.AirState = EGSTN_CombatAirState::Grounded; break;
	}
	View.bAirborne = View.AirState != EGSTN_CombatAirState::Grounded;
	View.CoyoteFramesLeft = Jump.CoyoteFramesLeft;
	View.AirJumpsUsed = Jump.AirJumpsUsed;
	View.bFastFalling = Jump.bFastFalling;
	View.bGliding = Jump.bGliding;
	return View;
}

FGSTN_CombatHitEventData FGSTN_CombatHitEventData::FromVictimHit(const FGSTN_VictimHit& Hit, int32 Frame, AActor* Attacker, AActor* Victim, const FGameplayTagContainer& EventTags)
{
	FGSTN_CombatHitEventData Data;
	Data.Attacker = Attacker;
	Data.Victim = Victim;
	Data.Frame = Frame;
	Data.Damage = Hit.Entry.Hit.Damage;
	Data.DamagePercentAfter = Hit.DamagePercentAfter;
	Data.HealthFractionAfter = Hit.HealthFractionAfter;
	Data.bStaggered = Hit.bStaggered;
	Data.Reaction = Hit.Reaction;
	Data.bTumble = Hit.bTumble;
	Data.bRicochet = Hit.bRicochet;
	Data.HitstunFrames = Hit.HitstunFrames;
	Data.HitstopFrames = Hit.HitstopFrames;
	if (Hit.bStaggered && Hit.Outcome.bHasKnockback)
	{
		Data.LaunchVelocity = Hit.Outcome.Knockback.LaunchVelocity;
	}
	if (!Data.LaunchVelocity.IsNearlyZero())
	{
		// Measured against world up: the angle a fighting game's knockback angle names (the gravity direction of a planetoid is not used here)
		const double Horizontal = FVector(Data.LaunchVelocity.X, Data.LaunchVelocity.Y, 0.0).Size();
		Data.LaunchAngle = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(Data.LaunchVelocity.Z, Horizontal)));
	}
	Data.Tags = EventTags;
	return Data;
}

bool FGSTN_CombatKOEventData::FromSimEvent(const FGSTN_SimEvent& Event, FGSTN_CombatKOEventData& Out)
{
	if (Event.EventType != GSTNCombatTags::Event_KO || Event.Tags.HasTagExact(GSTNCombatNPTags::EventHit_CreditPartner))
	{
		return false;
	}
	Out = FGSTN_CombatKOEventData();
	Out.Victim = Event.Target.Get();
	Out.Frame = Event.Frame;
	Out.Tags = Event.Tags;
	if (Event.Tags.HasTagExact(GSTNCombatTags::KOReason_Zone))
	{
		Out.Reason = GSTNCombatTags::KOReason_Zone;
	}
	else
	{
		Out.Reason = GSTNCombatTags::KOReason_Health;
		// A health KO names its attacker as the instigator; the victim itself when the attacker is unknown
		AActor* Instigator = Event.Instigator.Get();
		Out.KnockedOutBy = Instigator != Out.Victim ? Instigator : nullptr;
	}
	return true;
}

void FGSTN_CombatStateWatcher::Update(const FGSTN_CombatStateView& Now, TArray<EGSTN_CombatStateChange>& OutChanges)
{
	if (!Now.bValid)
	{
		return;
	}
	if (!bPrimed)
	{
		bPrimed = true;
		Previous = Now;
		LandedFrame = Now.bAirborne ? INDEX_NONE : Now.Frame;
		return;
	}
	const FGSTN_CombatStateView& Was = Previous;

	// One run of a move is its registry entry and its start frame: a move replaced by itself (jab 1 into jab 1) is an end and a start
	const bool bSameRun = Was.bInMove && Now.bInMove && Was.MoveEntry == Now.MoveEntry && Was.MoveStartFrame == Now.MoveStartFrame;
	if (Was.bInMove && !bSameRun)
	{
		OutChanges.Add(EGSTN_CombatStateChange::MoveEnded);
	}
	if (Was.bInHitstun && !Now.bInHitstun)
	{
		OutChanges.Add(EGSTN_CombatStateChange::HitstunEnded);
	}
	if (Was.bInTumble && !Now.bInTumble)
	{
		OutChanges.Add(EGSTN_CombatStateChange::TumbleEnded);
	}
	if (Was.bAirborne && !Now.bAirborne)
	{
		OutChanges.Add(EGSTN_CombatStateChange::Landed);
		LandedFrame = Now.Frame;
	}
	if (!Was.bAirborne && Now.bAirborne)
	{
		OutChanges.Add(EGSTN_CombatStateChange::LeftGround);
		LandedFrame = INDEX_NONE;
	}
	if (!Was.bInTumble && Now.bInTumble)
	{
		OutChanges.Add(EGSTN_CombatStateChange::TumbleStarted);
	}
	if (!Was.bInHitstun && Now.bInHitstun)
	{
		OutChanges.Add(EGSTN_CombatStateChange::HitstunStarted);
	}
	if (Now.bInMove && !bSameRun)
	{
		OutChanges.Add(EGSTN_CombatStateChange::MoveStarted);
	}
	if (Was.bHasRunStages && Now.bHasRunStages && Was.RunStage != Now.RunStage)
	{
		OutChanges.Add(EGSTN_CombatStateChange::RunStageChanged);
	}
	Previous = Now;
}

int32 FGSTN_CombatStateWatcher::GetFramesSinceLanded(int32 Frame) const
{
	return LandedFrame == INDEX_NONE || Frame < LandedFrame ? INDEX_NONE : Frame - LandedFrame;
}

void FGSTN_CombatStateWatcher::Reset()
{
	Previous = FGSTN_CombatStateView();
	bPrimed = false;
	LandedFrame = INDEX_NONE;
}
