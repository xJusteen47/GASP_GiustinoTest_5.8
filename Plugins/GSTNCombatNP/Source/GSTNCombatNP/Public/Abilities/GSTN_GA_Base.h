// Combat abilities (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GATypes.h"
#include "Abilities/NpGameplayAbility.h"
#include "DataTypes/SyncedvariableTypes/BasicSyncedVariablesTypes.h"
#include "GameplayTagContainer.h"
#include "Simulation/GSTN_CommandSourceInterface.h"
#include "GSTN_GA_Base.generated.h"

class UGSTN_CombatSimComponent;

/**
 * Base of the abilities that run under the combat model (combat-m3 6.1). It adds to UNpGameplayAbility what an ability needs
 * to know about the combatant, all of it readable from the simulation and rolled back with the ability instance:
 *
 * - why it started: the activation context (which command, which button, which source), copied once in ActivateAbility into sync
 *   variables because the input simulation only holds it on the activation frame. The command is kept as its index in the
 *   command table, never as a tag: no tag goes into a sync variable.
 * - when the trigger button goes up: bEndOnTriggerRelease ends a held-button ability (a stance, a hold-to-aim) from the input
 *   history, so the answer is the rolled back one on re-simulation.
 * - the combat clock: simulation frame minus hitstop frames. WaitCombatFrames counts on it, so a wait stands still in hitstop.
 * - the camera aim of the frame's command (GetAimRotation).
 * - what to do when something that is not a command starts it while a timeline runs (InterruptPolicy).
 *
 * Lock tags, costs, cooldowns, block and cancel tags, cues and triggers are the ability system's own and work as usual. A command
 * binding never meets InterruptPolicy: the combat tick has decided that the command may start, and the ability sees that through
 * FGSTN_InputHostUtils::IsCommandActivation, a property of the call that is the same on every machine and in every re-simulation.
 *
 * Derive in C++ or Blueprint (call the parent's ActivateAbility first, so the context is captured before the ability reads it).
 * Everything an override reads must be deterministic and rollback safe (gas-npp.md): state in sync variables, none in members.
 */
UCLASS(Abstract, Blueprintable)
class GSTNCOMBATNP_API UGSTN_GA_Base : public UNpGameplayAbility
{
	GENERATED_BODY()

public:
	UGSTN_GA_Base(const FObjectInitializer& ObjectInitializer);

	/** Ends the ability when the button that started it is released (see FGSTN_GARules::EndsOnRelease). Has no effect on a start without a button */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Activation")
	bool bEndOnTriggerRelease = false;

	/** What this ability does when it is started by something other than a command while a timeline runs */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Activation")
	EGSTN_InterruptPolicy InterruptPolicy = EGSTN_InterruptPolicy::SkipIfBusy;

	/**
	 * For InterruptCurrent: the running abilities that carry any of these ability tags are cancelled when this one starts (not this one).
	 * The timeline abilities add the tag of their kind in their constructor
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Activation", meta = (Categories = "Ability"))
	FGameplayTagContainer InterruptTags;

	/** The combat component of the avatar, cached when the avatar is set. Null if the avatar has none */
	UFUNCTION(BlueprintPure, Category = "Combat|Ability")
	UGSTN_CombatSimComponent* GetCombatComponent() const;

	/**
	 * Why this activation happened, rebuilt from the sync variables captured in ActivateAbility. Invalid (no command, source Other)
	 * for an activation that no command started. Valid for the whole life of the ability, unlike the component's own context
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Ability")
	FGSTN_ActivationContext GetActivationContext() const;

	/** The combat clock of the frame being simulated (UGSTN_CombatSimComponent::GetCombatClock); INDEX_NONE without a combat component */
	UFUNCTION(BlueprintPure, Category = "Combat|Ability")
	int32 GetCombatClock() const;

	/** The camera aim of the frame's command; zero without a combat component or an aim in the command */
	UFUNCTION(BlueprintPure, Category = "Combat|Ability")
	FRotator GetAimRotation() const;

	/**
	 * Calls OnCombatFramesElapsed once Frames combat frames have passed (at least 1; hitstop frames do not count). The start
	 * clock is a sync variable, so a rollback restores the wait. One wait at a time: calling it again re-arms it. Call it from
	 * ActivateAbility or the ability's tick, not from presentation code
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Ability")
	void WaitCombatFrames(int32 Frames);

	/** Stops a wait without calling anything */
	UFUNCTION(BlueprintCallable, Category = "Combat|Ability")
	void CancelCombatWait();

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;

protected:
	/** The wait of WaitCombatFrames is over. Runs inside the simulation tick, never while a frame is being restored */
	virtual void OnCombatFramesElapsed();

	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Ability", meta = (DisplayName = "On Combat Frames Elapsed"))
	void K2_OnCombatFramesElapsed();

private:
	static UGSTN_CombatSimComponent* FindCombatComponent(const FGameplayAbilityActorInfo* ActorInfo);

	/** Acceleration only (a lookup of the avatar's component), not state: valid or re-found, never rolled back */
	mutable TWeakObjectPtr<UGSTN_CombatSimComponent> CachedCombat;

	// ---- Sync variables: rolled back with the ability instance. The ability is instanced per actor, so every activation rewrites them

	/** Index in the command table of the command that started the ability, INDEX_NONE if none did */
	UPROPERTY()
	FSyncInteger CommandIndex;

	/** Frame of the press that completed the command, INDEX_NONE if none */
	UPROPERTY()
	FSyncInteger PressFrame;

	/** Index (in the input config) of the button that started the ability, INDEX_NONE if none */
	UPROPERTY()
	FSyncInteger TriggerButton;

	/** EGSTN_ActivationSource */
	UPROPERTY()
	FSyncByte ActivationSource;

	/** Combat clock when WaitCombatFrames was called */
	UPROPERTY()
	FSyncInteger WaitStartClock;

	/** Frames WaitCombatFrames waits, 0 = no wait pending */
	UPROPERTY()
	FSyncInteger WaitFrames;
};
