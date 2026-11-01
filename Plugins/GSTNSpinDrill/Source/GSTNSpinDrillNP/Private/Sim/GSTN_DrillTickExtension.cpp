// Spin and drill, Network Prediction half.

#include "Sim/GSTN_DrillTickExtension.h"

#include "AbilitySystemComponent.h"
#include "GSTN_SpinDrillTags.h"
#include "Math/GSTN_DrillMath.h"
#include "Sim/GSTN_DrillFragment.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Sync/GSTN_SyncFragment.h"

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

	uint16 ToFrames16(int32 Frames)
	{
		return static_cast<uint16>(FMath::Clamp(Frames, 0, static_cast<int32>(MAX_uint16)));
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
	Tuning.bRefreshAirSpinOnLaunch = Launch.bRefreshAirSpin;
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
			StepAfterExecute(*Fragment, Context.Frame, Inputs, GetTuning(Context));
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
	Fragment.AirSpinsUsed = static_cast<uint8>(UGSTN_DrillMath::AirSpinsUsedAfterEvents(Fragment.AirSpinsUsed, Tuning.RefreshOn, Inputs.RefreshEvents));
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
		}
		break;
	case EGSTN_DrillRequest::Dive:
		if (Fragment.IsSpinning())
		{
			const uint8 Sense = Fragment.Flags & GSTNDrillFlags::CounterClockwise;
			Enter(Fragment, EGSTN_DrillPhase::Diving, Frame);
			Fragment.Flags = Sense;
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
		}
		break;
	case EGSTN_DrillRequest::Bounce:
		if (Fragment.Phase == EGSTN_DrillPhase::Diving)
		{
			// 7.6: a bounce never refreshes the air spin
			EnterNone(Fragment);
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
	case EGSTN_DrillEndResult::DetachPop:
		ClearLatch(Fragment);
		EnterNone(Fragment);
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

FGSTN_DrillFrameInputs FGSTN_DrillTickExtension::GatherInputs(EGSTN_CombatHookPoint Point, const FGSTN_CombatTickContext& Context) const
{
	return FGSTN_DrillFrameInputs();
}

FGSTN_DrillSimTuning FGSTN_DrillTickExtension::GetTuning(const FGSTN_CombatTickContext& Context) const
{
	return FGSTN_DrillSimTuning::MakeDefault();
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
