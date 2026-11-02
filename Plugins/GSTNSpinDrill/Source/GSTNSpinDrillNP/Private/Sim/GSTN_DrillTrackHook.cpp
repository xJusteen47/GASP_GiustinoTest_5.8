// Spin and drill, Network Prediction half.

#include "Sim/GSTN_DrillTrackHook.h"

#include "Data/GSTN_SpinDrillTracks.h"
#include "Input/GSTN_ButtonMap.h"
#include "Sim/GSTN_DrillFragment.h"
#include "Sim/GSTN_DrillTickExtension.h"
#include "Simulation/GSTN_InputSimTypes.h"

bool FGSTN_BoreTrackHook::ShouldStall(const FGSTN_DrillFragment* Fragment, int32 TimelineFrame, int32 SeatFrame, int32 BoreFrame)
{
	if (!Fragment || !Fragment->IsLatched())
	{
		return false;
	}
	if (TimelineFrame == SeatFrame && Fragment->Phase == EGSTN_DrillPhase::Seating)
	{
		return true;
	}
	return TimelineFrame == BoreFrame;
}

FGSTN_TrackHookResult FGSTN_BoreTrackHook::Evaluate(const FGSTN_TrackHookContext& Context) const
{
	const FGSTN_BoreTrack& Track = static_cast<const FGSTN_BoreTrack&>(Context.Track);
	FGSTN_TrackHookResult Result;

	const int32 Button = Context.Buttons ? Context.Buttons->GetIndex(Track.HoldButton) : INDEX_NONE;
	const bool bHeld = Button >= 0 && Button < 32 && (Context.Input.History.GetHeldButtons() & (1u << Button)) != 0;
	const FGSTN_DrillFragment* Fragment = Context.Fragments ? FGSTN_SyncFragmentList::Find<FGSTN_DrillFragment>(*Context.Fragments) : nullptr;

	// The timeline stands ON the held frame (TimelineFrame, the one the previous tick evaluated), so the tip's track of that frame stays open
	if (Context.Frame != Context.TimelineStartFrame && ShouldStall(Fragment, Context.TimelineFrame, Track.StartFrame, Track.BoreFrame))
	{
		Result.bStallThisFrame = true;
	}

	Result.bStampWindow = true;
	Result.Window.StartFrame = Context.TimelineStartFrame;
	Result.Window.EndFrame = bHeld ? INDEX_NONE : Context.Frame;
	Result.Window.Profile = GSTNDrillStamps::CrouchMarker;
	return Result;
}
