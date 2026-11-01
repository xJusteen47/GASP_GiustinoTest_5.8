// Defence, Network Prediction half.

#include "Sim/GSTN_DefenceTrackHooks.h"

#include "Data/GSTN_DefenceTracks.h"
#include "Input/GSTN_ButtonMap.h"
#include "Sim/GSTN_DefenceFragment.h"
#include "Simulation/GSTN_InputSimTypes.h"

int32 FGSTN_HoldWindowHook::EvaluatedFrame(const FGSTN_TrackHookContext& Context)
{
	if (Context.Frame == Context.TimelineStartFrame)
	{
		return 0;
	}
	return Context.bHitstopFrame ? Context.TimelineFrame : Context.TimelineFrame + 1;
}

FGSTN_TrackHookResult FGSTN_HoldWindowHook::Evaluate(const FGSTN_TrackHookContext& Context) const
{
	const FGSTN_HoldWindowTrack& Track = static_cast<const FGSTN_HoldWindowTrack&>(Context.Track);
	FGSTN_TrackHookResult Result;

	const int32 Button = Context.Buttons ? Context.Buttons->GetIndex(Track.HoldButton) : INDEX_NONE;
	const bool bHeld = Button >= 0 && Button < 32 && (Context.Input.History.GetHeldButtons() & (1u << Button)) != 0;
	const FGSTN_DefenceFragment* Fragment = Context.Fragments ? FGSTN_SyncFragmentList::Find<FGSTN_DefenceFragment>(*Context.Fragments) : nullptr;
	const bool bStunned = Fragment && Fragment->IsShieldStunned(Context.Frame);
	const bool bEmpty = Track.bEndOnZeroShield && Fragment && Fragment->IsEmpty();
	const bool bHolds = bStunned || (bHeld && !bEmpty);

	// The timeline stands ON the window's frame while the hold lasts (TimelineFrame, the frame the previous tick evaluated), so the
	// tracks of that frame (the out-of-shield cancel windows) stay active; a stall decided on the frame about to be evaluated would
	// park it one frame short, outside them. The start tick never stalls (EvaluateStart honours no stall)
	if (Track.Contains(Context.TimelineFrame) && bHolds && Context.Frame != Context.TimelineStartFrame)
	{
		Result.bStallThisFrame = true;
	}

	Result.bStampWindow = true;
	Result.Window.StartFrame = Context.TimelineStartFrame;
	// The hold ended: the button is up (or the shield decayed to nothing) and no stun keeps it. A release inside a stun is not a release
	// (the shield stays up), so the frame reported is the one the hold really ended on
	Result.Window.EndFrame = bHolds ? INDEX_NONE : Context.Frame;
	Result.Window.Profile = GSTNDefenceStamps::HoldMarker;
	return Result;
}

FGSTN_TrackHookResult FGSTN_ParryWindowHook::Evaluate(const FGSTN_TrackHookContext& Context) const
{
	const FGSTN_ParryWindowTrack& Track = static_cast<const FGSTN_ParryWindowTrack&>(Context.Track);
	FGSTN_TrackHookResult Result;
	const bool bStartTick = Context.Frame == Context.TimelineStartFrame;
	if (Context.bHitstopFrame && !bStartTick)
	{
		return Result;
	}
	if (FGSTN_HoldWindowHook::EvaluatedFrame(Context) != Track.StartFrame || Context.TrackIndex < 0 || Context.TrackIndex > GSTNDefenceStamps::MaxTrackIndex)
	{
		return Result;
	}
	Result.bStampWindow = true;
	Result.Window.StartFrame = Context.Frame;
	Result.Window.EndFrame = Context.Frame + FMath::Max(Track.EndFrame - Track.StartFrame, 0);
	Result.Window.Profile = static_cast<uint8>(Context.TrackIndex);
	return Result;
}
