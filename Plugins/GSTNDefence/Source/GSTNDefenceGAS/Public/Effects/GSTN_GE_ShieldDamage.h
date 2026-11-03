// Defence, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayModMagnitudeCalculation.h"
#include "GSTN_GE_ShieldDamage.generated.h"

class UAbilitySystemComponent;

/**
 * The magnitude of the shield mirror effect: the set-by-caller value under Combat.SetByCaller.ShieldDamage, the loss the effect
 * subtracts. A pure read of the spec, no world: the same spec gives the same magnitude on every machine and in a resimulation.
 * An unset key reads 0 (no warning: the model writes the key every time, a hand-applied effect without it is a no-op).
 */
UCLASS()
class GSTNDEFENCEGAS_API UGSTN_ShieldDamageMagnitude : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;
};

/**
 * The instant effect that writes the shield MIRROR (m6 spec 4): one additive modifier on UGSTN_DefenceAttributeSet::ShieldHealth
 * whose magnitude is minus the set-by-caller value Combat.SetByCaller.ShieldDamage. It touches the mirror only: the shield value
 * the simulation plays lives in the combat sync state and is re-stamped by the Shield step; the model applies this effect at the
 * sparse mirror events (blocked hit, raise, release, break, every 16 frames while changing) so UI, AI and effects can read the
 * attribute. ApplyMirror writes an absolute value through the same modifier (the loss is the current base minus the target, which
 * may be negative after regeneration), so one class serves the damage and the mirror and the model's restore detection
 * (UGSTN_GE_ShieldRestore::RequestedDelta) stays consistent: a value the model mirrored is never mistaken for a request.
 *
 * Make a Blueprint child to add a gameplay cue (GameplayCue.Combat.Shield.Hit) or tags; the code never names a cue leaf.
 */
UCLASS(Blueprintable)
class GSTNDEFENCEGAS_API UGSTN_GE_ShieldDamage : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_ShieldDamage();

	/**
	 * Applies EffectClass (null = this class) to ASC, sourced by ASC itself, with Loss as the set-by-caller shield damage. Returns
	 * whether the effect was applied. Outside the simulation it is a plain application; the NP module calls it from its tick,
	 * where the ability system is simulation state that rolls back with the rest
	 */
	static bool Apply(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_ShieldDamage> EffectClass, float Loss);

	/** Writes NewValue into the mirror: the loss is the current base value minus NewValue. Returns whether the effect was applied */
	static bool ApplyMirror(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_ShieldDamage> EffectClass, float NewValue);
};
