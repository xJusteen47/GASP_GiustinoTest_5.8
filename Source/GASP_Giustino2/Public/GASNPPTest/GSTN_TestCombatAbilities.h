// GAS + NPP + Mover network test harness.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GSTN_GA_Base.h"
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
 * It is a UGSTN_GA_Base with bEndOnTriggerRelease: the base captures the trigger button and ends the stance when the button is
 * released (a start without a button never ends on release). It logs the camera aim it saw on activation (GASNPP_AIM, read by
 * scenario_camera_aim).
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_GA_TestStance : public UGSTN_GA_Base
{
	GENERATED_BODY()

public:
	UGSTN_GA_TestStance(const FObjectInitializer& ObjectInitializer);

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
