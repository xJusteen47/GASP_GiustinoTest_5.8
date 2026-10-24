// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Timeline/GSTN_TimelineTypes.h"

/**
 * What the player reports for one frame. Allocated once (Reserve) and reused every frame: Evaluate only resets the lengths,
 * so a running timeline costs no heap traffic, which matters for a listen host that simulates every pawn. The track
 * pointers point into the evaluated timeline data (or its ricochet response), which therefore must outlive the output's use
 * for that frame; they are never kept across frames.
 */
struct GSTNCOMBATCORE_API FGSTN_TimelineOutput
{
	/** The timeline frame that was evaluated (frame minus paused frames) */
	int32 Frame = 0;

	/** Length of what is being played: TotalFrames, or the recoil length after a ricochet */
	int32 TotalFrames = 0;

	/** The frame has reached the end: the timeline is over */
	bool bFinished = false;

	/** Before the first actionable frame: the character is held by the timeline */
	bool bBusy = false;

	/** The recoil of a ricochet is being played instead of the timeline */
	bool bRecoil = false;

	/** Inside the cancellable tail: anything cancels the timeline (the tail-cancel flag of plan R-A) */
	bool bTailCancel = false;

	/** First frame of the tail, INDEX_NONE if the timeline has none */
	int32 TailStartFrame = INDEX_NONE;

	/** Blend-out frames of the tail when it is cancelled */
	int32 TailBlendOutFrames = 0;

	/** Inside the startup override window */
	bool bInOverrideWindow = false;

	/** Frames of the override window after the default of the tier was applied; also the deferred commit frame */
	int32 DeferredCommitFrame = 0;

	/** The deferred commit frame was crossed this evaluation: costs commit, events and tracks start */
	bool bCommitThisFrame = false;

	/** Cue of the recoil, set (once) on the evaluation that starts the recoil */
	FGameplayTag RecoilCueTag;

	/** The recoil starts with this evaluation */
	bool bRecoilStartsNow = false;

	/** Cancel windows open at Frame */
	TArray<const FGSTN_TimelineTrack_CancelWindow*> CancelWindows;

	/** Tag windows open at Frame */
	TArray<const FGSTN_TimelineTrack_Tags*> TagWindows;

	/** Movement tracks running at Frame */
	TArray<const FGSTN_TimelineTrack_Movement*> ActiveMovement;

	/** Movement tracks whose start was crossed by this evaluation */
	TArray<const FGSTN_TimelineTrack_Movement*> MovementStarts;

	/** Movement tracks whose end was crossed by this evaluation */
	TArray<const FGSTN_TimelineTrack_Movement*> MovementEnds;

	/** Events whose frame was crossed by this evaluation: each fires once */
	TArray<const FGSTN_TimelineTrack_Event*> Events;

	/** Weapon swaps whose frame was crossed by this evaluation */
	TArray<const FGSTN_TimelineTrack_WeaponSwitch*> WeaponSwitches;

	/**
	 * Grows the arrays to what a timeline (and its recoil) can report, once. Evaluate calls it, and it is a no-op once the
	 * capacity is there, so reserving up front (at ability grant time) keeps the first frame allocation free too.
	 */
	void Reserve(const FGSTN_CombatTimelineData& Data);

	/** Empties the lists and flags but keeps the memory */
	void Reset();
};

/** Inputs of one evaluation. */
struct FGSTN_TimelineEvalParams
{
	/** Frames since the timeline started, hitstop included (the combat state's F - TimelineStartFrame) */
	int32 Frame = 0;

	/** Frames the timeline was paused so far (hitstop, M4): the evaluated frame is Frame - PausedFrames */
	int32 PausedFrames = 0;

	/**
	 * The timeline frame of the previous evaluation, INDEX_NONE before the first. Everything keyed to a frame (events, swaps,
	 * movement starts and ends, the commit) fires when its frame lies in (PreviousFrame, Frame], so it fires exactly once
	 * whether frames are skipped, paused (the frame does not move) or re-simulated from the same state.
	 */
	int32 PreviousFrame = INDEX_NONE;

	/** Attacker outcome: the hit was a ricochet. Switches the evaluation to the recoil data (never set before M4) */
	bool bRicochet = false;

	/**
	 * With bRicochet: frames since the recoil started (the caller keeps that start in its own state), paused frames already
	 * removed. Ignored otherwise.
	 */
	int32 RecoilFrame = 0;

	/** With bRicochet: the recoil frame of the previous evaluation, INDEX_NONE if this evaluation starts the recoil */
	int32 PreviousRecoilFrame = INDEX_NONE;

	/** Fallback when the timeline has no response of its own (the weapon or kit one, M3 C5); null = none */
	const FGSTN_RicochetResponse* RicochetFallback = nullptr;

	/** Override frames to use when the timeline says "default of the command tier" (-1) */
	int32 DefaultOverrideFrames = 0;
};

/** A hitbox that is open at the evaluated frame */
struct FGSTN_ActiveHitbox
{
	const FGSTN_HitboxSpec* Spec = nullptr;

	/** The track that holds it (its Part selector, its range) */
	const FGSTN_TimelineTrack_Hitbox* Track = nullptr;

	/** Frames since the track started: what the keys of the hitbox count in */
	int32 TrackFrame = 0;
};

/** How one hurtbox takes hits at a frame after the open modifier tracks are applied */
struct FGSTN_HurtboxFrameState
{
	EGSTN_HurtState State = EGSTN_HurtState::Normal;

	/** With State Armor: the largest armor threshold of the open Armor windows that address the hurtbox */
	FGSTN_StaggerThreshold ArmorThreshold;
};

/**
 * What the hit side of a timeline reports for one frame: the hitboxes that are open and the hurtbox modifier tracks that are
 * open. Like FGSTN_TimelineOutput it is the caller's scratch: Reserve once (at ability grant), then EvaluateHits only resets
 * the lengths, so a running timeline costs no heap traffic. The pointers point into the evaluated timeline data and are never
 * kept across frames.
 */
struct GSTNCOMBATCORE_API FGSTN_TimelineFrameResult
{
	/** The timeline frame that was evaluated (frame minus paused frames) */
	int32 Frame = 0;

	/** Hitboxes of the hitbox tracks that contain Frame, in track order then array order */
	TArray<FGSTN_ActiveHitbox> Hitboxes;

	/** Hurtbox modifier tracks that contain Frame */
	TArray<const FGSTN_TimelineTrack_HurtboxModifier*> HurtModifiers;

	/** Grows the arrays to what the timeline can report, once. A no-op when the capacity is there */
	void Reserve(const FGSTN_CombatTimelineData& Data);

	/** Empties the lists but keeps the memory */
	void Reset();
};

/**
 * Evaluates a timeline at a frame (combat.md 11.3, modularity 8.5.5). Stateless and pure: the timeline frame lives in the
 * combat state of the caller, which makes the result identical on the server, on a client and after a rollback. O(tracks),
 * no allocation once the output is reserved. Evaluate leaves the hitbox and hurtbox-modifier tracks to EvaluateHits.
 */
struct GSTNCOMBATCORE_API FGSTN_TimelinePlayer
{
	/** Fills Output for the frame described by Params. Resets Output first */
	static void Evaluate(const FGSTN_CombatTimelineData& Data, const FGSTN_TimelineEvalParams& Params, FGSTN_TimelineOutput& Output);

	/**
	 * Fills Result with the open hitboxes and hurtbox modifiers at Frame, the timeline frame with paused frames already
	 * removed (the Frame of FGSTN_TimelineOutput). Resets Result first.
	 */
	static void EvaluateHits(const FGSTN_CombatTimelineData& Data, int32 Frame, FGSTN_TimelineFrameResult& Result);

	/**
	 * The state of one hurtbox under the open modifiers: Intangible beats Invincible beats Armor beats Normal, and Armor keeps
	 * the largest threshold. A modifier with ShapeOverrides covers only a portion of the hurtbox and is left out here: which
	 * portion a hit lands on needs the contact, which the HurtState step (H4) has.
	 */
	static FGSTN_HurtboxFrameState ResolveHurtState(const FGSTN_TimelineFrameResult& Result, const FGSTN_Hurtbox& Hurtbox);

	/** Is Frame inside the cancellable tail of the timeline (false without a tail or at/after TotalFrames)? */
	static bool IsInTail(const FGSTN_CombatTimelineData& Data, int32 Frame);
};
