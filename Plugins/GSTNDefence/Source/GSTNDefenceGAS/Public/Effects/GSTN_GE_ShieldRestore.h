// Defence, ability-system half (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GSTN_GE_ShieldRestore.generated.h"

class UAbilitySystemComponent;

/**
 * The instant effect that asks the combat model to restore shield health (m6 spec 4): a pickup, a healer, a script. It adds its
 * amount to the ShieldHealth MIRROR; the model, which owns the real value in the sync state, reads at its next tick how far the
 * mirror's base value moved away from the last value it mirrored (RequestedDelta) and applies that difference as a request, then
 * mirrors the result back. So an effect never writes the simulation's state, the request costs no wire bytes, and a resimulated
 * frame finds the same mirror and makes the same request.
 *
 * The amount is a set-by-caller value under a plain NAME key (GetAmountKey), not a tag: the plugin declares no tag beyond F3's
 * list. Apply sets it; a Blueprint child applied by hand without the key reads 0 and the engine logs the missing key.
 * The attribute clamps the mirror at MaxShieldHealth, so an over-restore is harmless.
 */
UCLASS(Blueprintable)
class GSTNDEFENCEGAS_API UGSTN_GE_ShieldRestore : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_ShieldRestore();

	/** Set-by-caller name of the amount */
	static FName GetAmountKey();

	/**
	 * Applies EffectClass (null = this class) to ASC with Amount as the set-by-caller amount, sourced by Instigator (null = ASC's owner).
	 * Returns whether the effect was applied. For code outside the simulation (a script, a pickup actor); a simulated ability applies
	 * its restore effect through the normal GAS path and sets the key itself
	 */
	static bool Apply(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_ShieldRestore> EffectClass, float Amount, AActor* Instigator);

	/**
	 * What the mirror asks of the model: the mirror's base value minus LastMirroredValue, the value the model last wrote. Positive
	 * = a restore request of that size; 0 = nothing (every write of the model keeps the two equal); negative = a loss something
	 * else applied, which the model also honours. 0 when the ability system has no defence attribute set
	 */
	static float RequestedDelta(const UAbilitySystemComponent& ASC, float LastMirroredValue);
};
