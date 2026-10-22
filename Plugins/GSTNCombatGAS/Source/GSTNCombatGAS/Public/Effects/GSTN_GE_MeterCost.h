// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AttributeSet.h"
#include "GSTN_GE_MeterCost.generated.h"

/**
 * Instant effect that spends a meter. Make a Blueprint child per meter (or per cost) and pick the meter attribute and the
 * Cost: the single additive modifier is built from those two, with the cost negated, so a cost can not be wired to the
 * wrong attribute or sign. The magnitude is plain data on the effect, not a set-by-caller value, because as an ability cost
 * the effect goes through the ability's own CheckCost/ApplyCost, which build the spec themselves and give the ability no
 * hook to fill a key first. A kit or weapon therefore sets a move's cost by the cost effect class it assigns to the
 * ability (Cost on the child, or a curve table scaled by the ability level), never at activation time.
 *
 * The standard CheckCost compares the meter with the cost, so "not enough meter" fails the activation. The meter
 * attribute clamps at 0, so applying the effect without that check empties the meter instead of going negative.
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

	/** Amount spent, positive. Evaluated at the ability level, so a curve table can scale it per level. */
	UPROPERTY(EditDefaultsOnly, Category = "Meter")
	FScalableFloat Cost;

	/** Rebuilds the single modifier from Meter and Cost. Runs after load and after Meter or Cost is edited; a native child calls it once from its constructor. */
	void RebuildModifiers();

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
