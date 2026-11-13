// Spin and drill, Network Prediction half.

#include "Sim/GSTN_DrillTickExtension.h"

#include "Abilities/GSTN_GA_BaseCombat.h"
#include "AbilitySystemComponent.h"
#include "Data/GSTN_DrillTargetResponse.h"
#include "Data/GSTN_SpinDrillTracks.h"
#include "Effects/GSTN_SpinDrillEffects.h"
#include "Events/GSTN_SimEvent.h"
#include "GameFramework/Actor.h"
#include "GSTN_LinkRegistry.h"
#include "GSTN_LinkWatchdog.h"
#include "GSTN_SpinDrillTags.h"
#include "Math/GSTN_DrillMath.h"
#include "Sim/GSTN_DrillFragment.h"
#include "Sim/GSTN_LinkFragment.h"
#include "Sim/GSTN_LinkTickExtension.h"
#include "Sim/GSTN_SpinDrillAttach.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "Sync/GSTN_SyncFragment.h"
#include "Timeline/GSTN_TimelinePlayer.h"
#include "World/GSTN_StepSnapshot.h"

namespace GSTNDrillTick
{
	/** Back to no phase: the spin's sense and stamp are cleared so an idle fragment is the default again */
	void EnterNone(FGSTN_DrillFragment& Fragment)
	{
		Fragment.Phase = EGSTN_DrillPhase::None;
		Fragment.Flags = 0;
		Fragment.PhaseStartFrame = INDEX_NONE;
	}

	void ClearLatch(FGSTN_DrillFragment& Fragment)
	{
		Fragment.LatchFrame = INDEX_NONE;
		Fragment.SeatFrames = 0;
		Fragment.MaxBoreFrames = 0;
		Fragment.HitsLanded = 0;
	}

	void Enter(FGSTN_DrillFragment& Fragment, EGSTN_DrillPhase Phase, int32 Frame)
	{
		Fragment.Phase = Phase;
		Fragment.PhaseStartFrame = Frame;
	}

	/**
	 * The low gravity belonged to the air spin's timeline (14 SD.3): a spin that ends early (cancelled, replaced, jump cancelled) ends it
	 * too, keeping only the extra frames of the tuning. A spin that runs to its end finds its window ending there anyway
	 */
	void EndLowGravity(FGSTN_DrillFragment& Fragment, int32 Frame, const FGSTN_DrillSimTuning& Tuning)
	{
		if (Fragment.LowGravityUntilFrame != INDEX_NONE)
		{
			const int32 Until = FMath::Min(Fragment.LowGravityUntilFrame, Frame + FMath::Max(0, Tuning.LowGravityExtraFrames));
			Fragment.LowGravityUntilFrame = Until > Frame ? Until : INDEX_NONE;
		}
	}

	uint16 ToFrames16(int32 Frames)
	{
		return static_cast<uint16>(FMath::Clamp(Frames, 0, static_cast<int32>(MAX_uint16)));
	}

	/** The event the phase change of an AfterExecute raises, empty for none (outbox: published on the authority only, once per frame) */
	FGameplayTag EventForChange(EGSTN_DrillPhase Before, bool bWasLatched, const FGSTN_DrillFragment& After)
	{
		if (!bWasLatched && After.IsLatched())
		{
			return GSTNSpinDrillTags::Event_Drill_Latched;
		}
		if (bWasLatched && !After.IsLatched())
		{
			return After.Phase == EGSTN_DrillPhase::Launching ? GSTNSpinDrillTags::Event_Drill_Launched.GetTag() : GSTNSpinDrillTags::Event_Drill_Detached.GetTag();
		}
		if (Before == EGSTN_DrillPhase::Diving && After.Phase == EGSTN_DrillPhase::Lag)
		{
			return GSTNSpinDrillTags::Event_Drill_GroundLag;
		}
		return FGameplayTag();
	}
}

FGSTN_DrillSimTuning FGSTN_DrillSimTuning::MakeDefault()
{
	return FromTuning(FGSTN_AirSpinTuning(), FGSTN_SpinCancelTuning(), FGSTN_DiveTuning(), FGSTN_BoreTuning(), FGSTN_DrillLaunchTuning());
}

FGSTN_DrillSimTuning FGSTN_DrillSimTuning::FromTuning(const FGSTN_AirSpinTuning& AirSpin, const FGSTN_SpinCancelTuning& Cancel, const FGSTN_DiveTuning& Dive,
	const FGSTN_BoreTuning& Bore, const FGSTN_DrillLaunchTuning& Launch)
{
	FGSTN_DrillSimTuning Tuning;
	Tuning.CancelFromFrame = Cancel.CancelFromFrame;
	Tuning.MinDiveFramesBeforeLatch = Dive.MinDiveFramesBeforeLatch;
	Tuning.UsesPerAirtime = AirSpin.UsesPerAirtime;
	Tuning.RefreshOn = AirSpin.RefreshOn;
	Tuning.LowGravityExtraFrames = AirSpin.LowGravityExtraFrames;
	Tuning.bContinueThroughDestroyed = Bore.bContinueThroughDestroyed;
	Tuning.SeatFrames = Bore.SeatFrames;
	Tuning.MaxBoreFrames = Bore.MaxBoreFrames;
	Tuning.bRefreshAirSpinOnLaunch = Launch.bRefreshAirSpin;
	Tuning.HopSpeed = AirSpin.HopSpeed;
	Tuning.bHopCancelsFall = AirSpin.bHopCancelsFall;
	Tuning.SpinGravityScale = AirSpin.SpinGravityScale;
	Tuning.SpinMaxFallSpeedScale = AirSpin.SpinMaxFallSpeedScale;
	Tuning.PlungeSpeed = Dive.PlungeSpeed;
	Tuning.GroundLagFrames = Dive.GroundLagFrames;
	Tuning.BounceSpeed = Dive.BounceSpeed;
	Tuning.LaunchSpeed = Launch.LaunchSpeed;
	Tuning.LaunchDirection = Launch.LaunchDirection;
	Tuning.DetachPopSpeed = Bore.DetachPopSpeed;
	return Tuning;
}

uint8 FGSTN_DrillTickExtension::GetTypeId() const
{
	return GSTNFeatureIds::SpinDrill;
}

void FGSTN_DrillTickExtension::OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context)
{
	switch (Point)
	{
	case EGSTN_CombatHookPoint::AfterClocks:
		if (FGSTN_DrillFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DrillFragment>(Context.Out.Fragments))
		{
			StepAfterClocks(*Fragment, Context.Frame, GatherInputs(Point, Context), GetTuning(Context));
			// D9 part 2: the seat starts with the kit's bore timeline, which holds the latch while crouch is held. A kit without one keeps the
			// dive's timeline (the D9 part 1 behaviour: its release ends the latch)
			if (Context.Services && WantsBoreAdoption(*Fragment, Context.Frame, Context.Out.Core))
			{
				const UGSTN_CombatSimComponent* Combat = Context.Owner ? Context.Owner->FindComponentByClass<UGSTN_CombatSimComponent>() : nullptr;
				if (const UClass* Bore = FindBoreAbility(Combat ? Combat->FindTableSet(Context.Out.Core.TableRevision) : nullptr))
				{
					Context.Services->StartForcedTimeline(Bore);
				}
			}
		}
		break;
	case EGSTN_CombatHookPoint::AfterExecute:
	{
		const FGSTN_DrillFrameInputs Inputs = GatherInputs(Point, Context);
		FGSTN_DrillFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DrillFragment>(Context.Out.Fragments);
		// An entity that never spun gets the fragment only when a request starts something
		if (!Fragment && Inputs.Request != EGSTN_DrillRequest::None)
		{
			Fragment = &FGSTN_SyncFragmentList::FindOrAdd<FGSTN_DrillFragment>(Context.Out.Fragments);
		}
		if (Fragment)
		{
			const bool bWasLatched = Fragment->IsLatched();
			const EGSTN_DrillPhase Before = Fragment->Phase;
			StepAfterExecute(*Fragment, Context.Frame, Inputs, GetTuning(Context));
			FGSTN_LinkFragment* LinkFragment = (bWasLatched || Fragment->IsLatched()) ? FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Out.Fragments) : nullptr;
			if (bWasLatched && !Fragment->IsLatched())
			{
				// The drill ended its own latch (pop, launch): the link it still holds is released through GSTNLink, which places the exit
				if (ReleaseLinkOnUnlatch(bWasLatched, *Fragment, ReadLinkView(LinkFragment, GetDrillTypeIndex()), LinkFragment))
				{
					// ...and its exit, frames later, must leave with the pop's or the launch's velocity (SD-12)
					Fragment->Flags |= (Fragment->Flags & GSTNDrillFlags::Popped) != 0 ? GSTNDrillFlags::PopExit
						: Fragment->Phase == EGSTN_DrillPhase::Launching ? GSTNDrillFlags::LaunchExit : 0;
				}
			}
			if ((Fragment->Flags & GSTNDrillFlags::ExitImpulse) != 0)
			{
				const FGSTN_SnapshotEntry* Self = Context.Snapshot && Context.EntityId != INDEX_NONE ? Context.Snapshot->Find(Context.EntityId) : nullptr;
				StampExitImpulse(*Fragment, Context.Frame, Self ? &Self->Transform : nullptr, Context.Up, GetTuning(Context),
					FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Out.Fragments));
			}
			else if (Fragment->IsLatched() && LinkFragment && !LinkFragment->Links.Body.HasFlag(GSTNLinkFlags::AnchorOverride))
			{
				// The per-latch anchor: where the driller met the target, from the driller's view of it (on the authority, for a remote player,
				// the target as that player saw it), so the parked driller stays where the tip hit instead of the type's fixed offset
				const FGSTN_StepSnapshot* View = Context.Services ? Context.Services->GetViewSnapshot() : nullptr;
				View = View ? View : Context.Snapshot;
				const FGSTN_LinkState& Body = LinkFragment->Links.Body;
				const FGSTN_SnapshotEntry* Self = Context.Snapshot && Context.EntityId != INDEX_NONE ? Context.Snapshot->Find(Context.EntityId) : nullptr;
				const FGSTN_SnapshotEntry* Partner = View && !Body.PartnerId.IsNull() ? View->Find(Body.PartnerId.Id) : nullptr;
				if (Self && Partner)
				{
					WriteLatchAnchor(LinkFragment->Links.Body, GetDrillTypeIndex(), Self->Transform, Partner->Transform);
				}
			}
			const FGameplayTag Event = GSTNDrillTick::EventForChange(Before, bWasLatched, *Fragment);
			if (Event.IsValid() && Context.Outbox)
			{
				FGSTN_SimEvent SimEvent;
				SimEvent.EventType = Event;
				SimEvent.Frame = Context.Frame;
				SimEvent.Instigator = Context.Owner;
				SimEvent.Magnitude = static_cast<float>(Fragment->HitsLanded);
				Context.Outbox->Add(MoveTemp(SimEvent));
			}
		}
		break;
	}
	case EGSTN_CombatHookPoint::BeforeTagProjection:
		ProjectTags(Context, GetTuning(Context));
		break;
	case EGSTN_CombatHookPoint::BeforeCandidates:
	default:
		break;
	}
}

void FGSTN_DrillTickExtension::StepAfterClocks(FGSTN_DrillFragment& Fragment, int32 Frame, const FGSTN_DrillFrameInputs& Inputs, const FGSTN_DrillSimTuning& Tuning)
{
	// The bounce and pop flags live for their frame only (the dive and bore abilities read them in that frame)
	Fragment.Flags &= ~(GSTNDrillFlags::Bounced | GSTNDrillFlags::Popped);
	if (Fragment.LowGravityUntilFrame != INDEX_NONE && Frame >= Fragment.LowGravityUntilFrame)
	{
		Fragment.LowGravityUntilFrame = INDEX_NONE;
	}

	switch (Fragment.Phase)
	{
	case EGSTN_DrillPhase::Seating:
		if (Fragment.FramesSinceLatch(Frame) >= Fragment.SeatFrames)
		{
			GSTNDrillTick::Enter(Fragment, EGSTN_DrillPhase::Boring, Frame);
		}
		break;
	case EGSTN_DrillPhase::Spinning:
		if (Inputs.bPhaseTimelineEnded)
		{
			GSTNDrillTick::EnterNone(Fragment);
			GSTNDrillTick::EndLowGravity(Fragment, Frame, Tuning);
		}
		break;
	case EGSTN_DrillPhase::Diving:
	case EGSTN_DrillPhase::Launching:
	case EGSTN_DrillPhase::Lag:
		if (Inputs.bPhaseTimelineEnded)
		{
			GSTNDrillTick::EnterNone(Fragment);
		}
		break;
	default:
		break;
	}
}

void FGSTN_DrillTickExtension::StepAfterExecute(FGSTN_DrillFragment& Fragment, int32 Frame, const FGSTN_DrillFrameInputs& Inputs, const FGSTN_DrillSimTuning& Tuning)
{
	using namespace GSTNDrillTick;

	// The refresh comes first: a landing and an air spin request on one frame cannot happen (the air spin is bound to the air), but a
	// trampoline launch and an air spin can, and the spin then counts against the airtime the launch started
	Fragment.AirSpinsUsed = static_cast<uint8>(UGSTN_DrillMath::AirSpinsUsedAfterEvents(Fragment.AirSpinsUsed, Tuning.RefreshOn, Inputs.RefreshEvents));

	switch (Inputs.Request)
	{
	case EGSTN_DrillRequest::Spin:
	case EGSTN_DrillRequest::AirSpin:
	{
		const bool bAir = Inputs.Request == EGSTN_DrillRequest::AirSpin;
		if (Fragment.IsDrilling() || (bAir && !UGSTN_DrillMath::CanAirSpin(Fragment.AirSpinsUsed, Tuning.UsesPerAirtime)))
		{
			break;
		}
		Enter(Fragment, EGSTN_DrillPhase::Spinning, Frame);
		Fragment.Flags = (Inputs.bClockwise ? 0 : GSTNDrillFlags::CounterClockwise) | (bAir ? GSTNDrillFlags::AirSpin : 0);
		if (bAir)
		{
			Fragment.AirSpinsUsed = static_cast<uint8>(FMath::Min(Fragment.AirSpinsUsed + 1, 255));
			Fragment.LowGravityUntilFrame = Frame + FMath::Max(0, Inputs.SpinFrames) + FMath::Max(0, Tuning.LowGravityExtraFrames);
		}
		break;
	}
	case EGSTN_DrillRequest::Cancel:
		// Only the opposite sense, and only from CancelFromFrame on (7.3, 14 SD.2: the opposite rotation stops the spin)
		if (Fragment.IsSpinning() && Inputs.bClockwise != Fragment.IsClockwise() && Fragment.FramesInPhase(Frame) >= Tuning.CancelFromFrame)
		{
			EnterNone(Fragment);
			EndLowGravity(Fragment, Frame, Tuning);
		}
		break;
	case EGSTN_DrillRequest::Dive:
		if (Fragment.IsSpinning())
		{
			const uint8 Sense = Fragment.Flags & GSTNDrillFlags::CounterClockwise;
			Enter(Fragment, EGSTN_DrillPhase::Diving, Frame);
			Fragment.Flags = Sense;
			// The dive has its own gravity (none): the air spin's low gravity ends here
			EndLowGravity(Fragment, Frame, Tuning);
		}
		break;
	case EGSTN_DrillRequest::Latch:
		if (Fragment.Phase == EGSTN_DrillPhase::Diving && Fragment.FramesInPhase(Frame) >= Tuning.MinDiveFramesBeforeLatch)
		{
			// The seat starts on the next frame (7.4: LatchFrame = F + 1), the time origin of every machine
			Enter(Fragment, EGSTN_DrillPhase::Seating, Frame + 1);
			Fragment.LatchFrame = Frame + 1;
			Fragment.SeatFrames = ToFrames16(Inputs.LatchSeatFrames);
			Fragment.MaxBoreFrames = ToFrames16(FMath::Max(1, Inputs.LatchMaxBoreFrames));
			Fragment.HitsLanded = 0;
			Fragment.Flags &= ~GSTNDrillFlags::AwaitingLatch;
		}
		break;
	case EGSTN_DrillRequest::AwaitLatch:
		if (Fragment.Phase == EGSTN_DrillPhase::Diving)
		{
			Fragment.Flags |= GSTNDrillFlags::AwaitingLatch;
		}
		break;
	case EGSTN_DrillRequest::Bounce:
		if (Fragment.Phase == EGSTN_DrillPhase::Diving)
		{
			// 7.6: a bounce never refreshes the air spin. The flag tells the dive ability to bounce and end, this frame only
			EnterNone(Fragment);
			Fragment.Flags = GSTNDrillFlags::Bounced;
		}
		break;
	case EGSTN_DrillRequest::GroundLand:
		if (Fragment.Phase == EGSTN_DrillPhase::Diving)
		{
			Enter(Fragment, EGSTN_DrillPhase::Lag, Frame);
		}
		break;
	case EGSTN_DrillRequest::None:
	default:
		break;
	}

	if (!Fragment.IsLatched() || Frame < Fragment.LatchFrame)
	{
		return;
	}
	if (Inputs.bHitLanded)
	{
		Fragment.HitsLanded = static_cast<uint8>(FMath::Min(Fragment.HitsLanded + 1, 255));
	}

	FGSTN_DrillEndInputs End;
	End.bDrillerInHitstun = Inputs.bDrillerInHitstun;
	End.bDrillerKnockedOut = Inputs.bDrillerKnockedOut;
	End.bTargetGone = Inputs.bTargetGone;
	End.bTargetNotDrillable = Inputs.bTargetNotDrillable;
	End.bContinueThroughDestroyed = Tuning.bContinueThroughDestroyed;
	End.bCrouchHeld = Inputs.bCrouchHeld;
	End.FramesBored = Fragment.FramesSinceLatch(Frame);
	End.MaxBoreFrames = FMath::Max<int32>(1, Fragment.MaxBoreFrames);
	End.bJumpBoost = Inputs.bJumpBoost;

	const EGSTN_DrillEndResult Result = UGSTN_DrillMath::ResolveEnd(End);
	switch (Result)
	{
	case EGSTN_DrillEndResult::DetachHitstun:
		ClearLatch(Fragment);
		EnterNone(Fragment);
		break;
	case EGSTN_DrillEndResult::DetachPop:
		// The bore ability applies the pop on this frame (the flag), then its timeline plays the recovery
		ClearLatch(Fragment);
		EnterNone(Fragment);
		Fragment.Flags = GSTNDrillFlags::Popped;
		break;
	case EGSTN_DrillEndResult::Launch:
		ClearLatch(Fragment);
		Enter(Fragment, EGSTN_DrillPhase::Launching, Frame);
		Fragment.Flags = 0;
		Fragment.AirSpinsUsed = static_cast<uint8>(UGSTN_DrillMath::AirSpinsUsedAfterEvents(Fragment.AirSpinsUsed, Tuning.RefreshOn,
			UGSTN_DrillMath::RefreshEventsForEnd(Result, Tuning.bRefreshAirSpinOnLaunch)));
		break;
	case EGSTN_DrillEndResult::KeepPlunging:
		ClearLatch(Fragment);
		Enter(Fragment, EGSTN_DrillPhase::Diving, Frame);
		break;
	case EGSTN_DrillEndResult::Continue:
	default:
		break;
	}
}

const FGameplayTagContainer& FGSTN_DrillTickExtension::GetProjectableTags()
{
	static const FGameplayTagContainer Tags = []
	{
		FGameplayTagContainer Result;
		Result.AddTag(GSTNSpinDrillTags::State_Spinning);
		Result.AddTag(GSTNSpinDrillTags::State_Spinning_Clockwise);
		Result.AddTag(GSTNSpinDrillTags::State_Spinning_CounterClockwise);
		Result.AddTag(GSTNSpinDrillTags::State_AirSpinSpent);
		Result.AddTag(GSTNSpinDrillTags::State_Drilling);
		Result.AddTag(GSTNSpinDrillTags::State_DrillLatched);
		return Result;
	}();
	return Tags;
}

void FGSTN_DrillTickExtension::GatherStateTags(const FGSTN_DrillFragment* Fragment, const FGSTN_DrillSimTuning& Tuning, FGameplayTagContainer& OutTags)
{
	if (!Fragment)
	{
		return;
	}
	if (Fragment->IsSpinning())
	{
		OutTags.AddTag(GSTNSpinDrillTags::State_Spinning);
		OutTags.AddTag(Fragment->IsClockwise() ? GSTNSpinDrillTags::State_Spinning_Clockwise : GSTNSpinDrillTags::State_Spinning_CounterClockwise);
	}
	if (!UGSTN_DrillMath::CanAirSpin(Fragment->AirSpinsUsed, Tuning.UsesPerAirtime))
	{
		OutTags.AddTag(GSTNSpinDrillTags::State_AirSpinSpent);
	}
	if (Fragment->IsDrilling())
	{
		OutTags.AddTag(GSTNSpinDrillTags::State_Drilling);
	}
	if (Fragment->IsLatched())
	{
		OutTags.AddTag(GSTNSpinDrillTags::State_DrillLatched);
	}
}

bool FGSTN_DrillTickExtension::HasPhaseTimelineEnded(const FGSTN_DrillFragment& Fragment, const FGSTN_CombatSyncCore& Core)
{
	const bool bRunning = Core.ActionState == EGSTN_ActionState::InTimeline;
	switch (Fragment.Phase)
	{
	case EGSTN_DrillPhase::Spinning:
	case EGSTN_DrillPhase::Diving:
		// These phases start with their timeline: the phase start frame is the timeline's
		return !bRunning || Core.TimelineStartFrame != Fragment.PhaseStartFrame;
	case EGSTN_DrillPhase::Lag:
	case EGSTN_DrillPhase::Launching:
		// The lag starts inside the dive's timeline (on landing) and the launch inside the bore's (its recovery): each lasts as long as its
		// timeline does, ending with it or when another one starts
		return !bRunning || Core.TimelineStartFrame >= Fragment.PhaseStartFrame;
	default:
		return false;
	}
}

bool FGSTN_DrillTickExtension::HasSpinTimelineEnded(const FGSTN_DrillFragment& Fragment, const FGSTN_CombatSyncCore& Core)
{
	return Fragment.IsSpinning() && (Core.ActionState != EGSTN_ActionState::InTimeline || Core.TimelineStartFrame != Fragment.PhaseStartFrame);
}

void FGSTN_DrillTickExtension::ReadSpinStart(const FGSTN_SpinTrack& Track, FGSTN_DrillFrameInputs& OutInputs)
{
	OutInputs.Request = Track.bAirSpin ? EGSTN_DrillRequest::AirSpin : EGSTN_DrillRequest::Spin;
	OutInputs.bClockwise = Track.IsClockwise();
	// The low gravity lasts to the end of the track, counted from the timeline's start (the frame of the request)
	OutInputs.SpinFrames = FMath::Max(0, Track.EndFrame);
}

void FGSTN_DrillTickExtension::ReadCancelStart(const FGSTN_SpinCancelTrack& Track, FGSTN_DrillFrameInputs& OutInputs)
{
	OutInputs.Request = EGSTN_DrillRequest::Cancel;
	OutInputs.bClockwise = Track.IsClockwise();
}

EGSTN_DrillRequest FGSTN_DrillTickExtension::ReadDiveOutcome(const FGSTN_DrillFragment& Fragment, int32 Frame, bool bTipHit, bool bGrounded,
	const FGSTN_DrillLinkView& Link)
{
	if (Fragment.Phase != EGSTN_DrillPhase::Diving)
	{
		return EGSTN_DrillRequest::None;
	}
	// D9: the tip's hit asks for the drill latch. Active -> latch (asked again every frame until MinDiveFramesBeforeLatch lets it seat); Pending ->
	// keep diving; a request that went back to idle was refused (the hit did not land) -> bounce
	if (Link.bActive)
	{
		return EGSTN_DrillRequest::Latch;
	}
	if (Link.bPending)
	{
		return EGSTN_DrillRequest::AwaitLatch;
	}
	if ((Fragment.Flags & GSTNDrillFlags::AwaitingLatch) != 0)
	{
		return EGSTN_DrillRequest::Bounce;
	}
	// D8: a tip hit that asked for no link (a timeline without the drill type) bounces; the ground after the dive's first frame is the lag
	if (bTipHit)
	{
		return EGSTN_DrillRequest::Bounce;
	}
	return bGrounded && Fragment.FramesInPhase(Frame) >= 1 ? EGSTN_DrillRequest::GroundLand : EGSTN_DrillRequest::None;
}

FGSTN_DrillLinkView FGSTN_DrillTickExtension::ReadLinkView(const FGSTN_LinkFragment* LinkFragment, int32 DrillTypeIndex)
{
	FGSTN_DrillLinkView View;
	if (!LinkFragment || DrillTypeIndex <= 0)
	{
		return View;
	}
	const FGSTN_LinkState& Body = LinkFragment->Links.Body;
	if (Body.LinkTypeIndex != DrillTypeIndex)
	{
		return View;
	}
	// The request the tip's hit marked this frame (FGSTN_LinkTickExtension::MarkPredicted) is Pending with no role yet: the roles are written
	// when the link forms. Only the driller asks for a drill link from its own fragment, so a role-less request is the driller's own. Reading
	// it as "no link" made the tip hit bounce on that very frame and the dive ended while the link still formed (SD-10)
	if (Body.Phase == EGSTN_LinkPhase::Pending && Body.Role == 0)
	{
		View.bPending = true;
		return View;
	}
	if (Body.Role != 1)
	{
		return View;
	}
	const bool bBreaking = Body.Phase == EGSTN_LinkPhase::Exiting || (Body.IsActive() && Body.HasFlag(GSTNLinkFlags::PendingBreak));
	View.bPending = Body.Phase == EGSTN_LinkPhase::Pending;
	View.bActive = Body.IsActive() && !Body.HasFlag(GSTNLinkFlags::PendingBreak);
	View.bBrokenByHit = bBreaking && Body.GetBreakReason() == EGSTN_LinkBreakReason::Hit;
	return View;
}

void FGSTN_DrillTickExtension::ReadLatchEnd(const FGSTN_DrillFragment& Fragment, const FGSTN_CombatSyncCore& Core, const FGSTN_DrillLinkView& Link,
	FGSTN_DrillFrameInputs& InOutInputs)
{
	if (!Fragment.IsLatched())
	{
		return;
	}
	// The dive's timeline (it ends on the release) or the bore adopted at the latch stands for the held crouch; the bore's stamp refines it
	// (ReadCrouchStamp, which the extension applies after this). Any later timeline is not the drill's: released
	InOutInputs.bCrouchHeld = Core.ActionState == EGSTN_ActionState::InTimeline && Core.TimelineStartFrame <= Fragment.LatchFrame;
	// The driller staggered or knocked out by any source, before the link notices the hit: detach, the hitstun wins (7.4)
	InOutInputs.bDrillerInHitstun |= Core.ActionState == EGSTN_ActionState::Hitstun || Core.ActionState == EGSTN_ActionState::Tumble;
	InOutInputs.bDrillerKnockedOut |= Core.ActionState == EGSTN_ActionState::KO;
	if (!Link.bActive)
	{
		InOutInputs.bDrillerInHitstun |= Link.bBrokenByHit;
		InOutInputs.bTargetGone |= !Link.bBrokenByHit;
	}
}

bool FGSTN_DrillTickExtension::ReleaseLinkOnUnlatch(bool bWasLatched, const FGSTN_DrillFragment& Fragment, const FGSTN_DrillLinkView& Link,
	FGSTN_LinkFragment* LinkFragment)
{
	if (!bWasLatched || Fragment.IsLatched() || !Link.bActive || !LinkFragment)
	{
		return false;
	}
	return FGSTN_LinkTickExtension::MarkPendingBreak(*LinkFragment, static_cast<uint8>(EGSTN_LinkBreakReason::Release));
}

bool FGSTN_DrillTickExtension::StampExitImpulse(FGSTN_DrillFragment& Fragment, int32 Frame, const FTransform* SelfRoot, const FVector& Up,
	const FGSTN_DrillSimTuning& Tuning, FGSTN_LinkFragment* LinkFragment)
{
	const uint8 Impulse = Fragment.Flags & GSTNDrillFlags::ExitImpulse;
	if (Impulse == 0)
	{
		return false;
	}
	FGSTN_LinkActions::FExit* Exit = LinkFragment ? &LinkFragment->Actions.Exit : nullptr;
	const bool bDue = Exit && Exit->Frame == Frame && (Exit->Flags & GSTNLinkExitFlags::Teleport) != 0;
	if (!bDue)
	{
		// Still waiting while the link holds (pending break) or its exit is ahead; anything else (no link, an exit already past, a link that
		// ended without one) drops the impulse, so the flag never outlives the release it belongs to
		const bool bWaiting = Exit && (Exit->Frame > Frame || (Exit->Frame == 0 && LinkFragment->Links.Body.Phase != EGSTN_LinkPhase::None));
		if (!bWaiting)
		{
			Fragment.Flags &= ~GSTNDrillFlags::ExitImpulse;
		}
		return false;
	}
	// The exit leaves from where the parked driller is (end of the previous frame) with the same velocity as the pop and launch effects
	// (UGSTN_GE_DrillPop / UGSTN_GE_DrillLaunch). That spot touches the target, so the teleport is refused there and the link exit keeps the
	// driller in place with this velocity (FGSTN_LinkExitEffect)
	if (SelfRoot)
	{
		Exit->Position = SelfRoot->GetLocation();
		Exit->YawDeg = static_cast<float>(SelfRoot->Rotator().Yaw);
	}
	Exit->Velocity = (Impulse & GSTNDrillFlags::LaunchExit) != 0
		? UGSTN_DrillMath::ComputeLaunchVelocity(Tuning.LaunchDirection, Up, -Up, Tuning.LaunchSpeed)
		: UGSTN_DrillMath::ComputeHopVelocity(FVector::ZeroVector, Up, Tuning.DetachPopSpeed, true);
	Fragment.Flags &= ~GSTNDrillFlags::ExitImpulse;
	return true;
}

bool FGSTN_DrillTickExtension::ReadCrouchStamp(const FGSTN_TimelineOutput* Timeline, bool& bOutHeld, bool* bOutJumpBoost)
{
	if (!Timeline)
	{
		return false;
	}
	for (const FGSTN_StampedWindow& Stamp : Timeline->StampedWindows)
	{
		if (Stamp.Profile == GSTNDrillStamps::CrouchMarker || Stamp.Profile == GSTNDrillStamps::CrouchMarkerJump)
		{
			bOutHeld = Stamp.EndFrame == INDEX_NONE;
			if (bOutJumpBoost)
			{
				*bOutJumpBoost = Stamp.Profile == GSTNDrillStamps::CrouchMarkerJump;
			}
			return true;
		}
	}
	return false;
}

bool FGSTN_DrillTickExtension::WantsBoreAdoption(const FGSTN_DrillFragment& Fragment, int32 Frame, const FGSTN_CombatSyncCore& Core)
{
	// Asked on the seat's first frame only: a kit without a bore is not scanned again on every latched frame
	return Fragment.IsLatched() && Frame == Fragment.LatchFrame
		&& (Core.ActionState != EGSTN_ActionState::InTimeline || Core.TimelineStartFrame < Fragment.LatchFrame);
}

const UClass* FGSTN_DrillTickExtension::FindBoreAbility(const UGSTN_CombatTableSet* Set)
{
	if (!Set)
	{
		return nullptr;
	}
	for (const TSubclassOf<UGSTN_GA_BaseCombat>& Class : Set->TimelineRegistry)
	{
		const UGSTN_GA_BaseCombat* Default = Class ? Class->GetDefaultObject<UGSTN_GA_BaseCombat>() : nullptr;
		if (Default && FGSTN_BoreTrack::Find(Default->GetTimelineData()))
		{
			return Class.Get();
		}
	}
	return nullptr;
}

bool FGSTN_DrillTickExtension::WriteLatchAnchor(FGSTN_LinkState& Body, int32 DrillTypeIndex, const FTransform& SelfRoot, const FTransform& PartnerRoot)
{
	if (DrillTypeIndex <= 0 || Body.LinkTypeIndex != DrillTypeIndex || Body.Role != 1 || !Body.IsActive() || Body.HasFlag(GSTNLinkFlags::AnchorOverride))
	{
		return false;
	}
	Body.SetAnchorOverride(PartnerRoot.InverseTransformPosition(SelfRoot.GetLocation()));
	return true;
}

int32 FGSTN_DrillTickExtension::GetDrillTypeIndex()
{
	const UGSTN_LinkRegistry* Registry = UGSTN_LinkRegistry::Get();
	return Registry ? Registry->GetTypeIndex(GSTNSpinDrillTags::Link_Type_Drill) : 0;
}

bool FGSTN_DrillTickExtension::WantsDiveGravity(const FGSTN_DrillFragment* Fragment)
{
	return Fragment && Fragment->Phase == EGSTN_DrillPhase::Diving;
}

bool FGSTN_DrillTickExtension::WantsLowGravity(const FGSTN_DrillFragment* Fragment, int32 Frame)
{
	return Fragment && Fragment->IsLowGravity(Frame);
}

FGSTN_DrillFrameInputs FGSTN_DrillTickExtension::GatherInputs(EGSTN_CombatHookPoint Point, const FGSTN_CombatTickContext& Context) const
{
	FGSTN_DrillFrameInputs Inputs;
	switch (Point)
	{
	case EGSTN_CombatHookPoint::AfterClocks:
		if (const FGSTN_DrillFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DrillFragment>(Context.Out.Fragments))
		{
			// A timeline that started on this very frame (an adoption, step 3) is left to AfterExecute, which reads what it is (a dive, a
			// cancel...); a plain one ends the phase on the next frame
			const bool bStartedNow = Context.Out.Core.ActionState == EGSTN_ActionState::InTimeline && Context.Out.Core.TimelineStartFrame == Context.Frame;
			Inputs.bPhaseTimelineEnded = !bStartedNow && HasPhaseTimelineEnded(*Fragment, Context.Out.Core);
		}
		break;
	case EGSTN_CombatHookPoint::AfterExecute:
		// The jump step of this frame already ran: In holds the end of the previous frame, Out this one
		Inputs.RefreshEvents = UGSTN_DrillMath::RefreshEventsFromJumpPhase(static_cast<uint8>(Context.In.Core.Jump.Phase), static_cast<uint8>(Context.Out.Core.Jump.Phase));
		if (const FGSTN_CombatTimelineData* Started = ResolveStartedTimeline(Context))
		{
			if (const FGSTN_SpinTrack* Spin = FGSTN_SpinTrack::Find(*Started))
			{
				ReadSpinStart(*Spin, Inputs);
			}
			else if (const FGSTN_SpinCancelTrack* Cancel = FGSTN_SpinCancelTrack::Find(*Started))
			{
				ReadCancelStart(*Cancel, Inputs);
			}
			else if (FGSTN_DiveTrack::Find(*Started))
			{
				Inputs.Request = EGSTN_DrillRequest::Dive;
			}
		}
		if (const FGSTN_DrillFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DrillFragment>(Context.Out.Fragments))
		{
			// The drill latch as the link tick (priority 120) left it this frame; nothing to read for an entity that never linked
			const FGSTN_LinkFragment* LinkFragment = FGSTN_SyncFragmentList::Find<FGSTN_LinkFragment>(Context.Out.Fragments);
			const FGSTN_DrillLinkView Link = LinkFragment ? ReadLinkView(LinkFragment, GetDrillTypeIndex()) : FGSTN_DrillLinkView();
			if (Inputs.Request == EGSTN_DrillRequest::None)
			{
				Inputs.Request = ReadDiveOutcome(*Fragment, Context.Frame, Context.AttackerHits.Num() > 0, Context.Out.Core.Jump.Phase == EGSTN_JumpPhase::None, Link);
			}
			if (Inputs.Request == EGSTN_DrillRequest::Latch)
			{
				// The seat and the longest bore of this latch. Nothing filled them before SD-10: the seat was 0 frames and the bore 1, so every
				// latch ended on its first frame. The target's response (FGSTN_DrillTargetResponse) is not carried to the driller yet: its
				// defaults (scale 1, no override) apply
				const FGSTN_DrillSimTuning Tuning = GetTuning(Context);
				const FGSTN_DrillTargetResponse Response;
				Inputs.LatchSeatFrames = UGSTN_DrillMath::ResolveSeatFrames(Tuning.SeatFrames, Response.SeatFramesScale);
				Inputs.LatchMaxBoreFrames = UGSTN_DrillMath::ResolveMaxBoreFrames(Tuning.MaxBoreFrames, Response.MaxBoreFramesOverride);
			}
			ReadLatchEnd(*Fragment, Context.Out.Core, Link, Inputs);
			if (Fragment->IsLatched())
			{
				ReadCrouchStamp(Context.Timeline, Inputs.bCrouchHeld, &Inputs.bJumpBoost);
				// Every hit of the bore's tip lands on the latched target (the tip sits in it); a ricochet is not a hit
				for (const FGSTN_AttackerHit& Hit : Context.AttackerHits)
				{
					Inputs.bHitLanded |= !Hit.bRicochet;
				}
			}
		}
		break;
	default:
		break;
	}
	return Inputs;
}

FGSTN_DrillSimTuning FGSTN_DrillTickExtension::GetTuning(const FGSTN_CombatTickContext& Context) const
{
	return FGSTN_SpinDrillAttach::FindOrDefault(Context.Owner);
}

const FGSTN_CombatTimelineData* FGSTN_DrillTickExtension::ResolveStartedTimeline(const FGSTN_CombatTickContext& Context) const
{
	const FGSTN_CombatSyncCore& Core = Context.Out.Core;
	// Almost every frame stops here: only the frame a timeline starts on looks further
	if (Core.ActionState != EGSTN_ActionState::InTimeline || Core.TimelineStartFrame != Context.Frame || !Context.Owner)
	{
		return nullptr;
	}
	const UGSTN_CombatSimComponent* Combat = Context.Owner->FindComponentByClass<UGSTN_CombatSimComponent>();
	const UGSTN_CombatTableSet* Set = Combat ? Combat->FindTableSet(Core.TableRevision) : nullptr;
	if (!Set || !Set->TimelineRegistry.IsValidIndex(Core.ActiveTimeline) || !Set->TimelineRegistry[Core.ActiveTimeline])
	{
		return nullptr;
	}
	const UGSTN_GA_BaseCombat* Default = Set->TimelineRegistry[Core.ActiveTimeline]->GetDefaultObject<UGSTN_GA_BaseCombat>();
	return Default ? &Default->GetTimelineData() : nullptr;
}

void FGSTN_DrillTickExtension::ProjectTags(FGSTN_CombatTickContext& Context, const FGSTN_DrillSimTuning& Tuning) const
{
	UAbilitySystemComponent* ASC = Context.AbilitySystem;
	if (!ASC)
	{
		return;
	}
	const FGameplayTagContainer& Projectable = GetProjectableTags();
	const FGSTN_DrillFragment* Fragment = FGSTN_SyncFragmentList::Find<FGSTN_DrillFragment>(Context.Out.Fragments);
	// The low gravity follows the window on every frame, before the early out: a rollback to before the fragment must remove it too
	UGSTN_GE_AirSpinLowGravity::Sync(*ASC, WantsLowGravity(Fragment, Context.Frame), Tuning);
	UGSTN_GE_DrillDiveGravity::Sync(*ASC, WantsDiveGravity(Fragment));
	// No fragment (never spun, or a rollback to before the first spin): clear only what a predicted future may have left
	if (!Fragment && !ASC->HasAnyMatchingGameplayTags(Projectable))
	{
		return;
	}
	FGameplayTagContainer Held;
	GatherStateTags(Fragment, Tuning, Held);
	// Set, never added: nothing else writes these, and a re-simulated frame must not inherit what a predicted future left
	for (const FGameplayTag& Tag : Projectable)
	{
		ASC->SetLooseGameplayTagCount(Tag, Held.HasTagExact(Tag) ? 1 : 0);
	}
}
