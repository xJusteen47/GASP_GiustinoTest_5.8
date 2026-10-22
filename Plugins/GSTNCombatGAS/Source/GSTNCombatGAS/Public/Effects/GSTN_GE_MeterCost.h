// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AttributeSet.h"
#include "GSTN_GE_MeterCost.generated.h"

/**
 * Instant effect that spends a meter. Make a Blueprint child per meter and pick the meter attribute: the modifier (additive,
 * magnitude read from the Combat.SetByCaller.MeterCost key) is built from that pick, so a cost can not be wired to the wrong
 * key or sign. The activating ability fills the key with SetCost; as an ability cost the effect is applied through the
 * ability own cost path, which is also what makes "not enough meter" fail the activation.
 *
 * The meter attribute clamps at 0, so spending more than is available empties the meter instead of going negative; the
 * ability must check affordability before committing (the standard CheckCost does).
 */
UCLASS(Blueprintable)
class GSTNCOMBATGAS_API UGSTN_GE_MeterCost : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_MeterCost();

	/** Current-value attribute of the meter this effect spends. */
	UPROPERTY(EditDefaultsOnly, Category = "Meter")
	FGameplayAttribute Meter;

	/** Rebuilds the single modifier from Meter. Runs after load and after Meter is edited; a native child calls it once from its constructor. */
	void RebuildModifiers();

	/** Writes the cost (a positive amount) into Spec under the meter-cost key. The key carries the negated amount. */
	static void SetCost(FGameplayEffectSpec& Spec, float Cost);

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
