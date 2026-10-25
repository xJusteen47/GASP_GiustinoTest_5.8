// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Simulation/GSTN_CombatSimTypes.h"
#include "Timeline/GSTN_TimelinePlayer.h"

/**
 * The timeline part of the combat tick (combat-m3 7.2 steps 2, 6, 8 and 9), as functions of the sync state and the timeline
 * data so it is tested without a world: the component gathers the data, the settings and the ability system facts, calls
 * these, and writes what they return into its sync state and requests. No time, no role, no allocation (the output is
 * reserved by the caller and reused).
 *
 * The frame order inside the combat tick is: PruneBefore, AdvanceClock (the running timeline), IsCommandAllowed for each
 * candidate (the input simulation asks the host), Start (a timeline command won), then EvaluateStart so that a timeline whose
 * commit frame is its first frame commits on the frame it starts.
 */
struct GSTNCOMBATNP_API FGSTN_CombatTimelineLogic
{
	/**
	 * Step 2. Evaluates the running timeline at Frame into Output. If it reached its end, or its ability is gone (cancelled by
	 * something else; checked only after the start frame, because the ability system activates it later in the start frame),
	 * the combatant goes back to Actionable. A commit crossed by this evaluation adds a CommitTimeline request.
	 * With a recoil under way (Core.RecoilStartFrame) the recoil is evaluated instead, and its end is the end of the timeline: the recoil
	 * frames are the locked part, the combatant is Actionable from the next frame (the rest of the recoil montage is the cancellable tail).
	 * A hitstop frame (bHitstopFrame) adds one to TimelinePausedFrames first: the timeline frame stands still, so nothing keyed
	 * to a frame fires again and the timeline ends later by as many frames as the hitstop lasted.
	 * @return true if a timeline is still running after the step
	 */
	static bool AdvanceClock(FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData* Data, int32 DefaultOverrideFrames,
		bool bAbilityActive, FGSTN_TimelineOutput& Output, bool bHitstopFrame = false, const FGSTN_RicochetResponse* RicochetFallback = nullptr);

	/**
	 * Ricochet (M4 H12). The recoil of the attack: its own response, else Fallback (the weapon's, else the kit's: the caller resolves which
	 * one), null when neither is authored. A ricochet with no response changes nothing about the attacker
	 */
	static const FGSTN_RicochetResponse* ResolveRicochet(const FGSTN_CombatTimelineData* Data, const FGSTN_RicochetResponse* Fallback);

	/**
	 * The attacker side decided a ricochet on Frame: the timeline ends into the recoil, which begins on Frame + 1 (the clock step of Frame
	 * ran before the attacker step). Does nothing when no timeline runs, a recoil is under way, or no response exists.
	 * @return true when the recoil was started
	 */
	static bool StartRecoil(FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData* Data, const FGSTN_RicochetResponse* Fallback);

	/**
	 * Step 6. May the command start now? Actionable: yes. Running: inside the cancellable tail yes; inside the startup override
	 * window if the command matches OverrideInto (empty: its tier is above the running command's); in an open cancel window if
	 * the command matches the window's query and its condition holds (OnHit once a hit of the run is confirmed, OnBlock never before M6, OnWhiff while nothing hit);
	 * from the first actionable frame on yes; otherwise no. Output is the evaluation AdvanceClock made for this frame.
	 */
	static bool IsCommandAllowed(const FGSTN_CombatSyncCore& Core, const FGSTN_CombatTimelineData* Data, const FGSTN_TimelineOutput& Output,
		const FGameplayTag& Command, int32 CommandTier, int32 RunningTier);

	/**
	 * Step 8. A timeline command won: ends the running one (a CancelTimeline request for the old registry entry), writes the
	 * timeline state of the new one and returns. The ability itself is started by the input request of the same frame.
	 */
	static void Start(FGSTN_CombatSyncCore& Core, int32 Frame, int32 TimelineIndex, EGSTN_ActivationSource Source);

	/**
	 * Step 8, the other case: a command that is not a timeline ability won while a timeline runs (the stance that replaced an attack
	 * inside its override window, a jump out of a cancel window). The running timeline is cancelled (CancelTimeline request) and the
	 * combatant is Actionable again; the winner's own ability is started by the input request. Does nothing when idle.
	 */
	static void CancelForOtherCommand(FGSTN_CombatSyncCore& Core, int32 Frame);

	/**
	 * Step 3, adoption (combat-m3 7.4). Awaiting holds the registry indices of the timeline abilities that were activated in the
	 * previous frame by something other than a command and are waiting, in registry order. The first one wins: the timeline
	 * state is written as for a command with source Adopted (a running timeline is cancelled, the newcomer was allowed to activate
	 * by the ability system), and each other one gets a CancelAbility request. No ActivateEntry request: the winner is active
	 * already and starts itself when it sees its start frame in the state.
	 * @return the adopted registry index, INDEX_NONE when Awaiting is empty
	 */
	static int32 Adopt(FGSTN_CombatSyncCore& Core, int32 Frame, TConstArrayView<int32> Awaiting);

	/** Evaluates frame 0 of the timeline Start just began, so a commit on the first frame is requested in the same tick */
	static void EvaluateStart(FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData& Data, int32 DefaultOverrideFrames, FGSTN_TimelineOutput& Output);

	/** Back to Actionable with no timeline */
	static void ClearTimeline(FGSTN_CombatSyncCore& Core);
};
