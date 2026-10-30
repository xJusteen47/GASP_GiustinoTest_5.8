// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Sync/GSTN_SyncFragment.h"
#include "Timeline/GSTN_TimelinePlayer.h"

struct FGSTN_ButtonMap;
struct FGSTN_InputSimState;
struct FGSTN_TimelineTrack;
class UScriptStruct;

/**
 * What the combat component lends the track hooks of a tick besides the input state (M6 F7): the button map of its input config (a hold
 * track names its button by tag and the history holds bits) and the entity's sync fragments as the frame is writing them (a plugin's hook
 * reads its own fragment: a stun that keeps a hold stalled, a value that ends it). Both are content or state the component owns for the
 * tick; null in a test that lends nothing, which a hook must accept
 */
struct FGSTN_TrackHookEnvironment
{
	const FGSTN_ButtonMap* Buttons = nullptr;
	const FGSTN_SyncFragmentArray* Fragments = nullptr;
};

/** What the combat clock hands a track hook for one track of the running timeline */
struct FGSTN_TrackHookContext
{
	/** The track the hook was registered for, and its index in the timeline data's track array (what a stamp may name, so the plugin finds the track again) */
	const FGSTN_TimelineTrack& Track;
	int32 TrackIndex = INDEX_NONE;

	/** The simulation frame being ticked */
	int32 Frame = INDEX_NONE;

	/** The simulation frame the timeline started on */
	int32 TimelineStartFrame = INDEX_NONE;

	/**
	 * The timeline frame the timeline is on at the end of the previous tick: the one a stall holds. A tick that does not stall evaluates the
	 * next one. The hook compares it with the track's own range (the clock does not filter by it)
	 */
	int32 TimelineFrame = 0;

	/** The tick is a hitstop frame: the timeline stands still whatever the hook answers */
	bool bHitstopFrame = false;

	/** The entity's input state at the end of the previous frame, history included: the held buttons a hold track reads. Never written */
	const FGSTN_InputSimState& Input;

	/** Tag to bit of the entity's buttons; null when the caller lends none (a test) */
	const FGSTN_ButtonMap* Buttons = nullptr;

	/** The entity's sync fragments as the frame is writing them (the victim step of the frame ran already); null when the caller lends none. Read only */
	const FGSTN_SyncFragmentArray* Fragments = nullptr;

	FGSTN_TrackHookContext(const FGSTN_TimelineTrack& InTrack, const FGSTN_InputSimState& InInput)
		: Track(InTrack), Input(InInput)
	{
	}
};

/** What a track hook decided for the tick */
struct FGSTN_TrackHookResult
{
	/**
	 * Hold the timeline on TimelineFrame for this tick: TimelinePausedFrames grows by one, exactly as a hitstop frame does, so nothing keyed
	 * to a frame fires again and the timeline ends later. The combat clock (hitstop, buffers) keeps running
	 */
	bool bStallThisFrame = false;

	/** Report the window below on the output of this tick (FGSTN_TimelineOutput::StampedWindows) */
	bool bStampWindow = false;
	FGSTN_StampedWindow Window;
};

/**
 * A plugin's behaviour for one kind of track the combat model does not know (a hold or a parry track of the defence plugin): the clock calls it for every
 * track of the running timeline of that struct type, so the model never learns what the track means (modularity-and-tags 5b). C++ only and simulation
 * code: deterministic, a pure function of the context, no component reads, no state of its own (what it stamps the plugin writes into its own fragment).
 */
class GSTNCOMBATNP_API IGSTN_TimelineTrackHook
{
public:
	virtual ~IGSTN_TimelineTrackHook() = default;

	virtual FGSTN_TrackHookResult Evaluate(const FGSTN_TrackHookContext& Context) const = 0;
};

/**
 * The hooks by track struct. The kind enum of the timeline is closed (the player and the editor switch over it), so a track of a plugin is a struct that
 * derives FGSTN_TimelineTrack and reports no known kind, and its type is what the hook is registered for. Game thread only; registered when the module
 * starts and removed when it shuts down, before and after any simulation. Without a hook the clock step costs one empty check.
 */
class GSTNCOMBATNP_API FGSTN_TimelineTrackHookRegistry
{
public:
	static FGSTN_TimelineTrackHookRegistry& Get();

	/** False for a null struct or hook, or a struct that already has one (two hooks for one track type have no defined order) */
	bool Register(const UScriptStruct* TrackStruct, IGSTN_TimelineTrackHook* Hook);

	void Unregister(IGSTN_TimelineTrackHook* Hook);

	bool IsEmpty() const { return Hooks.IsEmpty(); }

	/**
	 * Calls the hook of every track of Data, in the order of the tracks, with the per-tick fields of the context filled from the arguments.
	 * @return true when any hook asked for a stall. The windows the hooks stamped are appended to OutWindows (two at most: more are dropped)
	 */
	bool Run(const FGSTN_CombatTimelineData& Data, int32 Frame, int32 TimelineStartFrame, int32 TimelineFrame, bool bHitstopFrame, const FGSTN_InputSimState& Input,
		FGSTN_StampedWindowList& OutWindows, const FGSTN_TrackHookEnvironment* Environment = nullptr) const;

private:
	struct FEntry
	{
		const UScriptStruct* TrackStruct = nullptr;
		IGSTN_TimelineTrackHook* Hook = nullptr;
	};

	TArray<FEntry> Hooks;
};
