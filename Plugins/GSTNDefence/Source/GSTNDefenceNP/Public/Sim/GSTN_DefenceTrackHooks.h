// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/GSTN_TimelineTrackHook.h"

/** What the defence hooks write into FGSTN_StampedWindow::Profile, so the tick extension tells the two kinds of stamp apart */
namespace GSTNDefenceStamps
{
	/** A hold window stamp: StartFrame = the timeline's start (the press), EndFrame = the frame the hold ended (INDEX_NONE while it holds) */
	constexpr uint8 HoldMarker = 255;

	/** A parry window stamp carries the TRACK INDEX of the parry window track in the timeline data (at most this), never a profile: the extension resolves the track */
	constexpr uint8 MaxTrackIndex = 254;
}

/**
 * The clock hook of FGSTN_HoldWindowTrack (m6 spec 3, 5.2 step 1): while the frame the timeline stands on (the one the previous tick
 * evaluated) is inside the window and the track's button is held, the timeline stalls ON that frame, so its tracks (the out-of-shield
 * cancel windows) stay active (TimelinePausedFrames grows; the combat clock does not). It also stalls while the
 * entity is shield stunned, whatever the button (the fragment's ShieldStunEndFrame: a blocked hit holds the shield up, as in Smash),
 * and stops stalling when the value reached zero from decay (the fragment's Empty flag, set by the tick extension the frame before:
 * the forced drop of bEndOnZeroShield) or the button is up.
 *
 * Every tick it stamps a hold window: StartFrame = the timeline's start (the press frame), EndFrame = INDEX_NONE while the hold goes
 * on, else the frame the hold ended on (the release the perfect-shield window is anchored on), Profile = GSTNDefenceStamps::HoldMarker.
 * The tick extension turns the first stamp into the raise and the ended one into the release; it never reads the input itself.
 *
 * A pure function of the context: the held mask of the input history at the end of the previous frame (one frame of latency on a
 * release, the same on every machine; the combat tick samples the frame's input after the clock step), the button map and the fragment the environment lends, nothing else. Without a button map the
 * button reads as up; without fragments there is no stun and no empty shield.
 */
class GSTNDEFENCENP_API FGSTN_HoldWindowHook : public IGSTN_TimelineTrackHook
{
public:
	virtual FGSTN_TrackHookResult Evaluate(const FGSTN_TrackHookContext& Context) const override;

	/** The timeline frame this tick evaluates when nothing stalls it: 0 on the start frame, else the frame after the one last evaluated (the same frame on a hitstop tick) */
	static int32 EvaluatedFrame(const FGSTN_TrackHookContext& Context);
};

/**
 * The clock hook of FGSTN_ParryWindowTrack (m6 spec 5.4, source Move): on the tick that evaluates the track's first frame it stamps the
 * window in simulation frames, [Frame, Frame + the track's length), with the track's index as the stamp's Profile; the tick extension
 * resolves the track again (its profile name against the entity's rules, its hurtbox selector against the entity's set) and writes the
 * window into the fragment. Not stamped on a hitstop tick (the frame was evaluated already), so hitstop inside the window consumes it,
 * as the spec proposes. Never stalls.
 */
class GSTNDEFENCENP_API FGSTN_ParryWindowHook : public IGSTN_TimelineTrackHook
{
public:
	virtual FGSTN_TrackHookResult Evaluate(const FGSTN_TrackHookContext& Context) const override;
};
