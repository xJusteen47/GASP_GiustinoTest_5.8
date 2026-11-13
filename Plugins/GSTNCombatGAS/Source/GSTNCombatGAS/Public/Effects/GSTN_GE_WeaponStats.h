// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "ActiveGameplayEffectHandle.h"
#include "Attributes/GSTN_KitStats.h"
#include "GSTN_GE_WeaponStats.generated.h"

class UAbilitySystemComponent;

/**
 * Base class of the stats effect of a weapon: infinite, so the stats hold while the weapon is active and vanish when the
 * effect is removed. A Blueprint child adds the modifiers on the combat attribute set (magnitudes read from set-by-caller
 * stat keys) and may grant the weapon role tags through a target-tags component.
 *
 * The effect is found again by its class, never by a stored handle, so the swap request of the combat model (which only
 * carries the old weapon index) can remove it. That makes one stats effect class per weapon a rule the weapon data
 * validation enforces: two weapons sharing a class would remove each other.
 */
UCLASS(Blueprintable)
class GSTNCOMBATGAS_API UGSTN_GE_WeaponStats : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UGSTN_GE_WeaponStats();

	/**
	 * Applies EffectClass to ASC, sourced by ASC itself, with Modifiers as set-by-caller magnitudes. Returns an invalid handle
	 * when the class is null or the effect was not applied. Does nothing and returns the live handle when the effect is
	 * already active, so a repeated activation (a resimulated frame) never stacks it.
	 */
	static FActiveGameplayEffectHandle Apply(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_WeaponStats> EffectClass,
		TConstArrayView<FGSTN_StatValue> Modifiers, float Level = 1.f);

	/** Removes every active instance of EffectClass from ASC. Returns how many were removed. */
	static int32 Remove(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_WeaponStats> EffectClass);

	/** True while an instance of EffectClass is active on ASC. */
	static bool IsApplied(const UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_WeaponStats> EffectClass);
};
