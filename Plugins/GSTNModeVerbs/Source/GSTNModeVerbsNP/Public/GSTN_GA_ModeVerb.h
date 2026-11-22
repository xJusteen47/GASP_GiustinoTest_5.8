// Mode verbs, Network Prediction half (needs the GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_Base.h"
#include "GSTN_ModeVerbTypes.h"
#include "GSTN_GA_ModeVerb.generated.h"

/**
 * A mode verb: the player's action in a mode, as an ability (owner rule 2026-10-23, game-modes 2.4). Activate a terminal, call a drop, call
 * extraction. The framework owns state and rules; this ability owns the action: the input binding, the animation and presentation, a predicted
 * hold, and ONE request at its confirmation frame.
 *
 * Two halves, kept apart on purpose:
 *   - inside the simulation (predicted, rolled back, runs again on a client that re-simulates): the hold. It lives in sync variables and the
 *     combat clock, so a rollback restores it. At the confirmation frame it appends a plain request to UGSTN_VerbSubsystem::QueueConfirm
 *     and ends. It appends in every tick on every machine with no role or re-simulation branch (CLAUDE.md rule 1) and decides nothing.
 *   - outside the simulation (UGSTN_VerbSubsystem, host only): the request is validated against the prop's own state and published ONCE as
 *     Message.Mode.Verb on the authoritative event stream. A refusal changes nothing; the client's ability already ended at its confirmation frame,
 *     so its hold bar is gone (the single cancel of the spec) and there is no state to roll back.
 *
 * Data is VerbSpec (tag, HoldFrames, target source, range). HoldFrames 0 confirms in the activation frame, for a command that already is the hold
 * (Command.Mode.Interact = Hold(Interact) requiring Interact.Target.Objective); a positive HoldFrames is a channel the player keeps up and cancels
 * by releasing the button. Bind it to a command in the kit's slot set (data); grant it through the mode's ability set (Game Feature). Add the verb tag
 * (VerbSpec.VerbTag) to the class's Asset Tags too: the constructor adds only the Ability.Mode root.
 *
 * Blueprint: derive it, set the VerbSpec, play the montage and the hold bar from the events. The events run inside the simulation tick like any ability
 * event: presentation only, they may run again on a re-simulation (never gameplay state).
 */
UCLASS(Blueprintable)
class GSTNMODEVERBSNP_API UGSTN_GA_ModeVerb : public UGSTN_GA_Base
{
	GENERATED_BODY()

public:
	UGSTN_GA_ModeVerb(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mode Verb")
	FGSTN_VerbSpec VerbSpec;

	/** Carried to the event as its Magnitude (a count, a strength, a tier: whatever the handler defines) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mode Verb")
	float VerbMagnitude = 1.f;

	/** 0..1 progress of the hold, 1 once confirmed, 0 when idle. For the hold bar; reads rolled-back state, so it is right after a re-simulation */
	UFUNCTION(BlueprintPure, Category = "Mode Verb")
	float GetHoldProgress() const;

	/** True from the confirmation frame on: the request is queued and the host decides */
	UFUNCTION(BlueprintPure, Category = "Mode Verb")
	bool IsVerbConfirmed() const;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	/** The hold ran its HoldFrames: confirm */
	virtual void OnCombatFramesElapsed() override;

	/** The confirmation frame: the request was queued. Start the "done" presentation (a sound, the terminal's lights) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Mode Verb", meta = (DisplayName = "On Verb Confirmed"))
	void K2_OnVerbConfirmed();

	/** The ability ended before its confirmation (button released, interrupted): once per activation. Hide the hold bar */
	UFUNCTION(BlueprintImplementableEvent, Category = "Mode Verb", meta = (DisplayName = "On Verb Cancelled"))
	void K2_OnVerbCancelled();

private:
	enum EVerbState : uint8
	{
		Idle = 0,
		Holding = 1,
		Confirmed = 2,
		Cancelled = 3
	};

	/** Queues the request, ends the ability. Runs once per activation: the state guards it, and the state is rolled back */
	void ConfirmVerb();

	// ---- Sync variables: rolled back with the ability instance. The instance is reused, so ActivateAbility rewrites them

	/** Combat clock when the hold started */
	UPROPERTY(Transient)
	FSyncInteger HoldStartClock;

	/** EVerbState */
	UPROPERTY(Transient)
	FSyncByte VerbState;
};
