// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Attributes/GSTN_AttributeAccessors.h"
#include "GSTN_HealthAttributeSet.generated.h"

/**
 * Damage and hit-resistance state of a combatant (combat-m4-hits 4). One set per concern: movement stats live in
 * UGSTN_CombatAttributeSet and meters in UGSTN_MeterAttributeSet. A game adds the sets it uses, and must add them in the same
 * order on every machine (the fork's simulation state is laid out by set order).
 *
 * The damage mode of the rules decides which attribute a hit writes: DamagePercent grows in the percent modes, Health falls in
 * the health mode. MaxHealth is the full health in the health mode and the cap, in percent points, in PercentCapped. The set does
 * not know the mode, so it only keeps every value in its natural range; the cap of PercentCapped is applied by the damage
 * execution (UGSTN_DamageExecution), which does know it. Reaching the cap or zero health is a KO, and raising that event is the
 * combat tick's job (H9), not an attribute callback.
 *
 * Clamps follow the fork's idiom: PreAttributeChange (aggregator results) and PreAttributeBaseChange (base values) apply the same
 * rule, so the value an ability reads and the value an effect writes never disagree. Health stays in [0, MaxHealth]; when an
 * instant effect lowers MaxHealth, Health is pulled down with it. Starting values come from
 * UGSTN_CombatGASSettings::AttributeDefaults. Nothing replicates: Network Prediction does that in the fork.
 */
UCLASS()
class GSTNCOMBATGAS_API UGSTN_HealthAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	/** Full health (health mode) or the damage cap in percent points (PercentCapped). Write it before Health in an init effect. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Health")
	FGameplayAttributeData MaxHealth;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_HealthAttributeSet, MaxHealth)

	/** Hit points left. Used in the health mode only. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Health")
	FGameplayAttributeData Health;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_HealthAttributeSet, Health)

	/** Damage taken, in percent points. Used in the percent modes; the cap of PercentCapped is MaxHealth. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Health")
	FGameplayAttributeData DamagePercent;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_HealthAttributeSet, DamagePercent)

	/** Scales the damage this combatant deals. 1 = unchanged. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Damage")
	FGameplayAttributeData OutgoingDamageMultiplier;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_HealthAttributeSet, OutgoingDamageMultiplier)

	/** Scales the damage this combatant takes. 1 = unchanged. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Damage")
	FGameplayAttributeData IncomingDamageMultiplier;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_HealthAttributeSet, IncomingDamageMultiplier)

	/** Fraction of the knockback this combatant resists, read by the knockback step. 0 = none. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Resistance")
	FGameplayAttributeData KnockbackResistance;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_HealthAttributeSet, KnockbackResistance)

	/** Fraction of the hitstun this combatant resists, read by the stagger step. 0 = none. */
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Resistance")
	FGameplayAttributeData HitstunResistance;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_HealthAttributeSet, HitstunResistance)

	virtual void PostInitProperties() override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

private:
	void ClampValue(const FGameplayAttribute& Attribute, float& NewValue) const;
};
