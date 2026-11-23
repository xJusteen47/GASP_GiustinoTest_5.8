// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Attributes/GSTN_AttributeAccessors.h"
#include "GSTN_MeterAttributeSet.generated.h"

/**
 * Resource meters of a combatant, as pairs of a current value and its maximum. A meter is addressed by its pair, never by
 * name in code: kit data lists FGSTN_MeterSetup entries (a Combat.Meter tag plus the two attributes), and a cost effect
 * names the current-value attribute it spends. Adding a meter means adding the two attributes here and one line to
 * GetMeterPairs, so the clamps below pick it up.
 *
 * The current value is kept in [0, Max]. When a maximum drops through an instant effect, PostGameplayEffectExecute pulls the
 * current value down with it. A maximum changed by an infinite effect does not trim the current value until the next write
 * to it. Bars are a UI division of the value (see FGSTN_MeterSetup), never stored here.
 *
 * A kit init effect must write a maximum before its meter: the meter is clamped to the maximum that is current at that
 * moment. Health lives in UGSTN_HealthAttributeSet (per-part health is a later milestone). Starting values come from
 * UGSTN_CombatGASSettings::AttributeDefaults. Nothing replicates: Network Prediction does that in the fork.
 */
UCLASS()
class GSTNCOMBATGAS_API UGSTN_MeterAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	/** Current and maximum attribute of one meter. */
	struct FMeterPair
	{
		FGameplayAttribute Value;
		FGameplayAttribute Max;
	};

	/** Every meter this set holds. The only place that pairs a value with its maximum. */
	static const TArray<FMeterPair>& GetMeterPairs();

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Meter")
	FGameplayAttributeData ExMeter;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_MeterAttributeSet, ExMeter)

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Meter")
	FGameplayAttributeData ExMeterMax;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_MeterAttributeSet, ExMeterMax)

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Meter")
	FGameplayAttributeData SuperMeter;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_MeterAttributeSet, SuperMeter)

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Meter")
	FGameplayAttributeData SuperMeterMax;
	GSTN_COMBAT_ATTRIBUTE_ACCESSORS(UGSTN_MeterAttributeSet, SuperMeterMax)

	virtual void PostInitProperties() override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

private:
	void ClampMeter(const FGameplayAttribute& Attribute, float& NewValue) const;
};
