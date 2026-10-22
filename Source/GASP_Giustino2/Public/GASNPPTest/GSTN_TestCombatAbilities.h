// GAS + NPP + Mover network test harness.

#pragma once

#include "CoreMinimal.h"
#include "GAS/GSTN_GameplayAbility.h"
#include "DataTypes/SyncedvariableTypes/BasicSyncedVariablesTypes.h"
#include "GSTN_TestCombatAbilities.generated.h"

/**
 * Placeholder ability bound to the four Command.Attack.* commands (grounded and airborne) by the combat test binding set.
 *
 * It does nothing but own Test.State.Attacking for ActiveFrames simulation frames, so a command that started it (or one
 * that was locked and did not) shows up in the ability system's tags, the harness snapshots and the Test.* log. It never
 * touches the character: the harness checks input and lock behaviour here, not combat moves.
 *
 * Rollback: the frame counter is a sync variable; the ability is instanced per actor, so it is reset on every activation.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestAttack : public UGSTN_GameplayAbility
{
	GENERATED_BODY()

public:
	UGSTN_GA_TestAttack(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;

protected:
	/** Simulation frames the ability (and Test.State.Attacking) lasts */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Test|Attack", meta = (ClampMin = "1"))
	int32 ActiveFrames = 20;

private:
	/** Simulation frames since activation. A sync var: rolled back with the ability instance on corrections */
	UPROPERTY()
	FSyncInteger FramesSinceActivation;
};

/**
 * Placeholder stance, bound to the Command.Special.* commands (the Special button is the stance button of
 * scenario_combat_lock). While active it owns Test.State.Stance and the attack command lock (Input.Lock.Command.Attack, via
 * ActivationOwnedTags), so the attack commands cannot start while the stance is held but a command of another root (the
 * dash) still does.
 *
 * It ends by itself when the button that started it is released, asking the input simulation
 * (IGSTN_CommandSourceInterface::IsCommandButtonHeld) so the answer is the rolled back one on re-simulation. The button is
 * read once from the activation context and kept in a sync variable, because the context is only valid on the activation
 * frame. A start without a button (a forced command) ends at once.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestStance : public UGSTN_GameplayAbility
{
	GENERATED_BODY()

public:
	UGSTN_GA_TestStance(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;

private:
	/** Index (in the input config) of the button that started the stance, INDEX_NONE if none. A sync var: rolled back with the ability instance */
	UPROPERTY()
	FSyncInteger TriggerButton;
};
