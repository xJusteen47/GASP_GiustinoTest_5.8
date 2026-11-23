// Combat abilities (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_MoveEvents.h"
#include "Abilities/GSTN_TimelinePlay.h"
#include "GameplayTagContainer.h"
#include "Tasks/BasePredictionTask.h"
#include "GSTN_PlayCombatTimelineTask.generated.h"

class UGSTN_CombatTimeline;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSTN_TimelinePlayDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_TimelinePlayKeyDelegate, FGameplayTag, KeyTag, int32, TimelineFrame);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGSTN_TimelinePlayHitDelegate, int32, NewHitGroups, int32, AllHitGroups);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGSTN_TimelinePlayInterruptedDelegate, EGSTN_MoveEndReason, Reason);

/**
 * Play Combat Timeline And Wait (move-timeline-decoupling 1A): the combat counterpart of PlayMontageAndWait. Plays a combat timeline (frame data,
 * hitboxes, cancel windows, montage) in the ability that owns the node and reports it on its pins: On Key (an Event key crossed), On Hit
 * Confirmed, then exactly one of On Completed, On Interrupted (with the reason) or On Cancelled.
 *
 * It is a prediction task of the fork (UBasePredictionTask), not a GAS UAbilityTask: one instance per node and per ability instance, its active bit
 * synced and restored by the fork, so a rollback neither duplicates nor loses it. Everything else about the play (which timeline, which frame,
 * which node owns it) lives in the ability's sync variables (UGSTN_GA_BaseCombat::PlayTaskIndex and the timeline cursors): the task only relays
 * what the ability raises in its simulation step, and only while it is active and the fork is not restoring a frame, so a pin fires once per
 * simulated path. The timeline starts in the frame of the call (FGSTN_TimelinePlayRules). Works in any UGSTN_GA_BaseCombat; in a host (Play
 * Timeline On Activation off) it is how the ability plays anything at all
 */
UCLASS(BlueprintType, meta = (DisplayName = "Play Combat Timeline And Wait"))
class GSTNCOMBATNP_API UGSTN_PlayCombatTimelineTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()

public:
	/** An Event key of the timeline was crossed (once per crossing; it stands still in hitstop). Compare KeyTag */
	UPROPERTY(BlueprintAssignable)
	FGSTN_TimelinePlayKeyDelegate OnKey;

	/** A hit of the timeline was confirmed (hit or blocked). NewHitGroups: the groups that confirmed this frame, bit i = hit group i */
	UPROPERTY(BlueprintAssignable)
	FGSTN_TimelinePlayHitDelegate OnHitConfirmed;

	/** The timeline played to its last frame. A Play from here starts the next one in this same frame */
	UPROPERTY(BlueprintAssignable)
	FGSTN_TimelinePlayDelegate OnCompleted;

	/** The timeline was cut: a hit taken, a command through a cancel window, a tail cancel, a ricochet, another Play in this ability */
	UPROPERTY(BlueprintAssignable)
	FGSTN_TimelinePlayInterruptedDelegate OnInterrupted;

	/** The ability ended while the timeline ran, or the timeline could not start (see the log: not in Playable Timelines, called outside the simulation) */
	UPROPERTY(BlueprintAssignable)
	FGSTN_TimelinePlayDelegate OnCancelled;

	/** Plays Timeline: one of the ability's Playable Timelines, or its own Timeline Asset */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks|Combat", meta = (BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTask(UGSTN_CombatTimeline* Timeline);

	// ---- Called by UGSTN_GA_BaseCombat for the play this task owns, inside its simulation step

	void HandleKey(FGameplayTag KeyTag, int32 TimelineFrame);
	void HandleHitConfirmed(int32 NewHitGroups, int32 AllHitGroups);
	void HandleEnded(EGSTN_TimelinePlayResult Result, EGSTN_MoveEndReason Reason);

protected:
	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
};
