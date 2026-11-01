// Spin and drill, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_SpinDrillTypes.h"
#include "Simulation/GSTN_CombatTickExtension.h"

struct FGSTN_DrillFragment;

/** What asks the phase machine to change at AfterExecute (the abilities of D4-D10 will raise them; tests raise them by hand) */
enum class EGSTN_DrillRequest : uint8
{
	None,

	/** Start a ground spin in FGSTN_DrillFrameInputs::bClockwise */
	Spin,

	/** Start an air spin (refused when the air spins of the airtime are spent) */
	AirSpin,

	/** The opposite rotation during a spin, from CancelFromFrame on: the spin stops (14 SD.2) */
	Cancel,

	/** Crouch held during a spin: start the dive */
	Dive,

	/** The tip latched onto a drillable target this frame (resolved seat and max frames in the inputs) */
	Latch,

	/** The tip hit a hittable but not drillable target: the dive ends with a bounce (7.6) */
	Bounce,

	/** The dive reached plain ground: the ground lag */
	GroundLand,
};

/**
 * One frame of what the phase machine reads besides its own fragment. In the game these come from the input history, the combat core
 * state, the snapshot and the running timeline (D4 onward); in D3 nothing fills them (FGSTN_DrillTickExtension::GatherInputs returns
 * the defaults), and the tests fill them by hand. Values of the frame, so a re-simulation reads the same ones.
 */
struct FGSTN_DrillFrameInputs
{
	// ---- AfterClocks

	/** EGSTN_AirSpinRefresh events of the frame (landing, wall, trampoline, launch, hit) */
	int32 RefreshEvents = 0;

	/** The timeline that drives the current phase ended this frame (spin, launch, lag) */
	bool bPhaseTimelineEnded = false;

	// ---- AfterExecute

	EGSTN_DrillRequest Request = EGSTN_DrillRequest::None;

	/** Sense of a Spin, AirSpin or Cancel request */
	bool bClockwise = true;

	/** Length of the air spin's timeline, for the low gravity that follows it */
	int32 SpinFrames = 0;

	/** Resolved at the latch: UGSTN_DrillMath::ResolveSeatFrames / ResolveMaxBoreFrames with the target's response */
	int32 LatchSeatFrames = 0;
	int32 LatchMaxBoreFrames = 1;

	/** A drill hit landed on the latched target this frame */
	bool bHitLanded = false;

	// ---- End conditions of a latched driller (UGSTN_DrillMath::ResolveEnd)

	bool bCrouchHeld = true;
	bool bJumpBoost = false;
	bool bDrillerInHitstun = false;
	bool bDrillerKnockedOut = false;
	bool bTargetGone = false;
	bool bTargetNotDrillable = false;
};

/** The tuning values the phase machine needs, copied from the kit's tuning (content, the same on every machine) */
struct GSTNSPINDRILLNP_API FGSTN_DrillSimTuning
{
	int32 CancelFromFrame = 6;
	int32 MinDiveFramesBeforeLatch = 2;
	int32 UsesPerAirtime = 1;
	int32 RefreshOn = 0;
	int32 LowGravityExtraFrames = 0;
	bool bContinueThroughDestroyed = false;
	bool bRefreshAirSpinOnLaunch = true;

	// The air spin's movement (D4): read by the hop creator and the low gravity effect, never by the phase machine
	float HopSpeed = 450.f;
	bool bHopCancelsFall = true;
	float SpinGravityScale = 0.35f;
	float SpinMaxFallSpeedScale = 0.5f;

	/** The defaults of the Core tuning structs */
	static FGSTN_DrillSimTuning MakeDefault();

	static FGSTN_DrillSimTuning FromTuning(const FGSTN_AirSpinTuning& AirSpin, const FGSTN_SpinCancelTuning& Cancel, const FGSTN_DiveTuning& Dive,
		const FGSTN_BoreTuning& Bore, const FGSTN_DrillLaunchTuning& Launch);
};

/**
 * The spin and drill phase machine inside the combat tick (spin-drill.md 7.2), skeleton of plan D3 with the spin of D4. It reads and writes
 * only the drill fragment; the other inputs arrive as FGSTN_DrillFrameInputs. Priority 130: after the parts (100), the defence (110) and the
 * links (120).
 *
 *  - AfterClocks: the low gravity stamp is cleared once passed; a seat that reached its length becomes the bore; the end of the timeline of
 *    a spin, launch or lag returns to None (a spin ends when the combat state no longer runs the timeline that started it).
 *  - BeforeCandidates: nothing yet (the bore's hit-group mask is the fallback of seam S1, D6).
 *  - AfterExecute: the refresh events of the frame give the air spins back (the jump state after this frame's jump step: grounded, or
 *    just launched), then the request of the frame (spin, air spin, cancel, dive, latch, bounce, ground), the hit count, then the end
 *    conditions of a latched driller (UGSTN_DrillMath::ResolveEnd). A spin request comes from a timeline that started this frame and
 *    carries a FGSTN_SpinTrack (its sense and air flag).
 *  - BeforeTagProjection: Combat.State.Spinning (+ .Clockwise / .CounterClockwise), .AirSpinSpent, .Drilling, .DrillLatched SET from the
 *    fragment, never added, so a re-simulated frame ends with the same tags; and the low gravity effect follows the fragment's window
 *    (UGSTN_GE_AirSpinLowGravity::Sync).
 *
 * Every transition returns the fragment to its defaults once nothing is live, so an entity that stopped spinning costs no payload.
 * Simulation code: deterministic, no net mode, no resimulation branch, state only in the fragment.
 */
class GSTNSPINDRILLNP_API FGSTN_DrillTickExtension : public IGSTN_CombatTickExtension
{
public:
	/** After the parts (100), the defence (110) and the links (120) */
	static constexpr int32 ExtensionPriority = 130;

	virtual uint8 GetTypeId() const override;
	virtual int32 GetPriority() const override { return ExtensionPriority; }
	virtual void OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) override;

	// ---- The phase machine, pure (tests drive it directly)

	/** AfterClocks on a fragment that exists */
	static void StepAfterClocks(FGSTN_DrillFragment& Fragment, int32 Frame, const FGSTN_DrillFrameInputs& Inputs, const FGSTN_DrillSimTuning& Tuning);

	/** AfterExecute; Fragment may be a fresh default one */
	static void StepAfterExecute(FGSTN_DrillFragment& Fragment, int32 Frame, const FGSTN_DrillFrameInputs& Inputs, const FGSTN_DrillSimTuning& Tuning);

	/** The state tags the fragment holds at Frame (null fragment: none) */
	static void GatherStateTags(const FGSTN_DrillFragment* Fragment, const FGSTN_DrillSimTuning& Tuning, FGameplayTagContainer& OutTags);

	/** Every tag this extension sets or clears */
	static const FGameplayTagContainer& GetProjectableTags();

	// ---- Reading the combat state (pure, tests drive them directly)

	/** A Spinning fragment whose timeline is no longer the one the combat state runs (it ended, was cancelled or was replaced) */
	static bool HasSpinTimelineEnded(const FGSTN_DrillFragment& Fragment, const FGSTN_CombatSyncCore& Core);

	/** The request a spin track asks for on the frame its timeline starts: Spin or AirSpin, the sense, the frames of the low gravity */
	static void ReadSpinStart(const struct FGSTN_SpinTrack& Track, FGSTN_DrillFrameInputs& OutInputs);

	/** The low gravity window of the air spin holds at Frame: what UGSTN_GE_AirSpinLowGravity::Sync is asked to match */
	static bool WantsLowGravity(const FGSTN_DrillFragment* Fragment, int32 Frame);

protected:
	/**
	 * The frame's inputs at a hook point: at AfterClocks the end of a spin's timeline; at AfterExecute the refresh events of the jump state
	 * and the spin request of a timeline that started this frame. The dive, latch and bore inputs come with D8 to D10
	 */
	virtual FGSTN_DrillFrameInputs GatherInputs(EGSTN_CombatHookPoint Point, const FGSTN_CombatTickContext& Context) const;

	/** The entity's tuning: the attached kit tuning (FGSTN_SpinDrillAttach), else the defaults */
	virtual FGSTN_DrillSimTuning GetTuning(const FGSTN_CombatTickContext& Context) const;

	/**
	 * The frame data of a timeline that started on this frame (the combat state's TimelineStartFrame is the frame), null otherwise. Content:
	 * the class default of the registry entry in the table set of the state's revision, the same on every machine. The context's
	 * RunningTimeline cannot serve, it is taken before a command starts its timeline
	 */
	virtual const FGSTN_CombatTimelineData* ResolveStartedTimeline(const FGSTN_CombatTickContext& Context) const;

private:
	void ProjectTags(FGSTN_CombatTickContext& Context, const FGSTN_DrillSimTuning& Tuning) const;
};
