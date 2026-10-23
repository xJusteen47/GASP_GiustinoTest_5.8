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
	 * @return true if a timeline is still running after the step
	 */
	static bool AdvanceClock(FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData* Data, int32 DefaultOverrideFrames,
		bool bAbilityActive, FGSTN_TimelineOutput& Output);

	/**
	 * Step 6. May the command start now? Actionable: yes. Running: inside the cancellable tail yes; inside the startup override
	 * window if the command matches OverrideInto (empty: its tier is above the running command's); in an open cancel window if
	 * the command matches the window's query and its condition holds (OnHit/OnBlock never before M4, OnWhiff while nothing hit);
	 * from the first actionable frame on yes; otherwise no. Output is the evaluation AdvanceClock made for this frame.
	 */
	static bool IsCommandAllowed(const FGSTN_CombatSyncCore& Core, const FGSTN_CombatTimelineData* Data, const FGSTN_TimelineOutput& Output,
		const FGameplayTag& Command, int32 CommandTier, int32 RunningTier);

	/**
	 * Step 8. A timeline command won: ends the running one (a CancelTimeline request for the old registry entry), writes the
	 * timeline state of the new one and returns. The ability itself is started by the input request of the same frame.
	 */
	static void Start(FGSTN_CombatSyncCore& Core, int32 Frame, int32 TimelineIndex, EGSTN_ActivationSource Source);

	/** Evaluates frame 0 of the timeline Start just began, so a commit on the first frame is requested in the same tick */
	static void EvaluateStart(FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_CombatTimelineData& Data, int32 DefaultOverrideFrames, FGSTN_TimelineOutput& Output);

	/** Back to Actionable with no timeline */
	static void ClearTimeline(FGSTN_CombatSyncCore& Core);
};
