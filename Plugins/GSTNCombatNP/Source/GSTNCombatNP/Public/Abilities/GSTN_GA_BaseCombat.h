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
 * Anything that is not a command can start the ability too (a gameplay event or tag trigger, an effect, another ability, AI). It
 * finds no start in the combat state, so it only marks itself as waiting (the loose tag Combat.Ability.AwaitingAdoption and the sync
 * variable behind IsAwaitingAdoption) and the combat tick of the next frame adopts it: it writes the timeline state with source
 * Adopted, and the ability starts in its own tick of that frame (combat-m3 7.4). The class has to be in the component's registry
 * for that (UGSTN_CombatSimComponent::AdoptableTimelines, or a command binding); an ability the registry does not know is cancelled
 * after MaxAdoptionWaitFrames. The frame data comes from TimelineAsset when it is set,
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

	/** Loads the soft references the simulation needs (the timeline asset, the montage) and keeps them in this instance. Call outside the simulation; the avatar being set does */
	void PreloadTimelineContent();

	/**
	 * Loads the same content and appends the loaded objects to OutKeep, for an owner that has to keep them alive itself: the combat
	 * component does this for every registry class, because it reads the class default object, which holds nothing. Not in a tick
	 */
	void LoadTimelineContent(TArray<TObjectPtr<UObject>>& OutKeep) const;

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	/** Combat frames since the timeline started (the clock minus the clock of its start), 0 when not started */
	UFUNCTION(BlueprintPure, Category = "Combat|Timeline")
	int32 GetTimelineFrame() const;

	/** Frames an activation without a command waits to be adopted before it is cancelled (another ability won the adoption) */
	static constexpr int32 MaxAdoptionWaitFrames = 2;

	/** True from an activation the combat state did not start until the combat tick adopts it (or it is cancelled). Read by the combat tick through the spec's instance */
	bool IsAwaitingAdoption() const { return bAwaitingAdoption.Value; }

	/** True once the ability found itself in the combat state and its timeline runs (false while it waits for adoption and after it ended) */
	bool HasTimelineStarted() const { return bStarted.Value; }

protected:
	/**
	 * Called once when the timeline starts: in ActivateAbility for a command, in the adoption tick for an activation that waited.
	 * The movement abilities apply their impulse here. Simulation code: deterministic, no server-only branches
	 */
	virtual void OnTimelineBegan(int32 StateStartFrame) {}

private:
	/** Starts the timeline whose start the combat state shows: sync variables, montage */
	void BeginTimeline(int32 StateStart);

	/** Drops the waiting state and its loose tag, if it holds them */
	void ClearAwaitingAdoption();

	/** True while the combat state names this ability's class and the start frame it started on */
	bool IsStillTheRunningTimeline() const;

	/** True once the ability found itself in the combat state and started (sync var: rolled back with the instance) */
	UPROPERTY(Transient)
	FSyncBool bStarted;

	/** Combat clock at the start, the origin of GetTimelineFrame */
	UPROPERTY(Transient)
	FSyncInteger StartClock;

	/** Simulation frame the combat state said the timeline started on */
	UPROPERTY(Transient)
	FSyncInteger StartFrame;

	/** True from an activation that waits for adoption until it is adopted, cancelled or ended */
	UPROPERTY(Transient)
	FSyncBool bAwaitingAdoption;

	/** Simulation frame the wait began on */
	UPROPERTY(Transient)
	FSyncInteger AwaitSinceFrame;

	/** What PreloadTimelineContent loaded (the timeline asset and the montage): nothing else keeps them in memory, and a montage collected on one machine only would desynchronise the played montages */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UObject>> LoadedContent;
};
