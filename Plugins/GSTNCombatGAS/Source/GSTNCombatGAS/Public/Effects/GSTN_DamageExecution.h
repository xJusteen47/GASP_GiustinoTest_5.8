// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "GSTN_DamageExecution.generated.h"

/**
 * Turns a hit's damage into a write on the victim (combat-m4-hits 4). The damage is the set-by-caller Combat.SetByCaller.Damage
 * of the effect spec, scaled by the attacker's OutgoingDamageMultiplier and the victim's IncomingDamageMultiplier (a missing
 * set counts as 1). The damage mode comes from the rules carried in the hit effect context (FGSTN_HitEffectContext); a plain
 * context has none and counts as the percent mode.
 *
 *  - Percent: DamagePercent grows by the damage.
 *  - PercentCapped: the same, but DamagePercent stops at MaxHealth. Reaching the cap is a KO, which the combat tick raises.
 *  - Health: Health falls by the damage and stops at 0 (the attribute clamp).
 *
 * A negative damage counts as 0: healing is not a hit. The execution reads attributes without snapshotting, so it sees the
 * values of the frame it runs on, which is what a resimulation needs. It reads no actor, world or clock, so it is deterministic.
 */
UCLASS()
class GSTNCOMBATGAS_API UGSTN_DamageExecution : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:
	UGSTN_DamageExecution();

	virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};
