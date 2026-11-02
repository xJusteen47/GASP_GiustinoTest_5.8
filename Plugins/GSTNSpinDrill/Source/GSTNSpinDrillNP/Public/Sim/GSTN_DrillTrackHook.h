// Spin and drill, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/GSTN_TimelineTrackHook.h"

struct FGSTN_DrillFragment;

/**
 * The drill's hook for FGSTN_BoreTrack (plan D9 part 2, the defence hold window's pattern): it holds the bore timeline while the drill is latched
 * and reports the crouch to the phase machine.
 *
 *  - Stall on the track's StartFrame (the seat frame, no hitbox) while the fragment is Seating, and on BoreFrame (the tip with its re-hit
 *    interval) while the fragment is latched. Once the latch ended (launch, pop, detach) the timeline plays on into its recovery. The start tick
 *    never stalls.
 *  - Stamp, on every tick of the track, a window with Profile GSTNDrillStamps::CrouchMarker whose EndFrame is INDEX_NONE while HoldButton is held
 *    and the frame otherwise; the drill tick extension reads it at AfterExecute (FGSTN_DrillTickExtension::ReadCrouchStamp).
 *
 * The stall reads the fragment the frame is writing: the seat-to-bore change of AfterClocks comes after the clock, so the bore frame is reached
 * one frame after the seat ended, on every machine alike. Simulation code: a pure function of the context, no state of its own.
 */
class GSTNSPINDRILLNP_API FGSTN_BoreTrackHook : public IGSTN_TimelineTrackHook
{
public:
	virtual FGSTN_TrackHookResult Evaluate(const FGSTN_TrackHookContext& Context) const override;

	/** Whether the timeline stands still on TimelineFrame for a fragment in this state (null: never). Pure, for tests */
	static bool ShouldStall(const FGSTN_DrillFragment* Fragment, int32 TimelineFrame, int32 SeatFrame, int32 BoreFrame);
};
