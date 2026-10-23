// Combat abilities (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_Base.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_GA_BaseCombat.generated.h"

class UGSTN_CombatTimeline;

/**
 * A combat ability that runs a timeline (combat-m3 6.2): frame data (total and actionable frames, the startup override window,
 * cancel windows, a cancellable tail, a montage) the combat tick plays, so the ability itself holds almost no logic.
 *
 * The combat component owns the timeline clock. When a command starts a timeline ability the combat tick writes the timeline
 * state (which registry entry, from which frame) into the combat sync state and the ability system activates the ability in the same
 * frame; the ability finds itself in that state, plays its montage, and ends when the state no longer names it (the timeline reached
 * its end, was replaced by another command, or was cancelled). The cost is committed by the combat tick when the startup override
 * window ends (CommitTimeline request), so a timeline replaced inside the window leaves no trace but its turn.
 *
 * Only a command can start a timeline in this version: an activation the combat state does not show ends at once (adoption of
 * abilities started by triggers or effects comes with combat-m3 7.4). The frame data comes from TimelineAsset when it is set,
 * else from Timeline; both are read from the class default object by the combat component, never from an instance, because the
 * component needs it before the ability runs.
 */
UCLASS(Abstract, Blueprintable)
class GSTNCOMBATNP_API UGSTN_GA_BaseCombat : public UGSTN_GA_Base
{
	GENERATED_BODY()

public:
	UGSTN_GA_BaseCombat(const FObjectInitializer& ObjectInitializer);

	/** The inline frame data; ignored when TimelineAsset is set */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Timeline")
	FGSTN_CombatTimelineData Timeline;

	/** A shared timeline asset; wins over the inline one. Soft: loaded when the ability is granted, never in the simulation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Timeline")
	TSoftObjectPtr<UGSTN_CombatTimeline> TimelineAsset;

	/** The frame data this class runs: the asset's when it is set and loaded, else the inline one */
	const FGSTN_CombatTimelineData& GetTimelineData() const;

	/** Loads the soft references the simulation needs (the timeline asset, the montage). Call outside the simulation; the grant does */
	void PreloadTimelineContent();

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Combat frames since the timeline started (the clock minus the clock of its start), 0 when not started */
	UFUNCTION(BlueprintPure, Category = "Combat|Timeline")
	int32 GetTimelineFrame() const;

private:
	/** True while the combat state names this ability's class and the start frame it started on */
	bool IsStillTheRunningTimeline() const;

	/** 1 once the ability found itself in the combat state and started (sync var: rolled back with the instance) */
	UPROPERTY()
	FSyncInteger bStarted;

	/** Combat clock at the start, the origin of GetTimelineFrame */
	UPROPERTY()
	FSyncInteger StartClock;

	/** Simulation frame the combat state said the timeline started on */
	UPROPERTY()
	FSyncInteger StartFrame;

	/** Acceleration only, resolved by PreloadTimelineContent */
	UPROPERTY(Transient)
	TObjectPtr<UGSTN_CombatTimeline> LoadedAsset;
};
