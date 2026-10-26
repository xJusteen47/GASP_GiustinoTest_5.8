// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "AttributeSet.h"
#include "GSTN_GE_MeterGain.generated.h"

/**
 * Instant effect that fills a meter (combat-m4-hits 4). Make a Blueprint child per meter and pick the meter attribute: the single
 * additive modifier reads its amount from the set-by-caller key Combat.SetByCaller.Meter, so the code that applies a hit decides
 * the amount (a fraction of the damage, a fixed reward) without a class per amount. The meter attribute clamps at its maximum, so
 * overfilling is harmless.
 *
 * Unlike a meter cost, which is an ability's own cost and cannot be filled in at activation, a gain is applied by the hit code
 * through MakeOutgoingSpec, which can set the key first. An unset key adds nothing.
 */
UCLASS(Blueprintable)
class GSTNCOMBATGAS_API UGSTN_GE_MeterGain : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_MeterGain();

	/** Current-value attribute of the meter this effect fills */
	UPROPERTY(EditDefaultsOnly, Category = "Meter")
	FGameplayAttribute Meter;

	/** Rebuilds the single modifier from Meter. Runs after load and after Meter is edited; a native child calls it once from its constructor */
	void RebuildModifiers();

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
