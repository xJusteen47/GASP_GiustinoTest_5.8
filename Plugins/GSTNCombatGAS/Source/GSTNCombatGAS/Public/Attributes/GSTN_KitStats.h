// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ActiveGameplayEffectHandle.h"
#include "GSTN_KitStats.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
struct FGameplayEffectSpec;

/**
 * One designer number for a stat, keyed by the set-by-caller tag the stat's effect modifier reads. The tag is the only link
 * between the number and the attribute it ends up in, so adding a stat is a tag in an ini plus a modifier on an effect, and
 * no C++ names a stat.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_StatValue
{
	GENERATED_BODY()

	/** Set-by-caller key of the effect modifier this number feeds. A leaf under the stat set-by-caller root. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat", meta = (Categories = "Combat.SetByCaller.Stat"))
	FGameplayTag Stat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat")
	float Value = 0.f;

	/** Sets every value as a set-by-caller magnitude on Spec. A stat the effect has no modifier for is harmless. */
	static void ApplyToSpec(FGameplayEffectSpec& Spec, TConstArrayView<FGSTN_StatValue> Values);

	/** Reports an invalid tag, a tag outside the stat set-by-caller root, or a tag listed twice. */
	static bool Validate(TConstArrayView<FGSTN_StatValue> Values, TArray<FText>& OutErrors);
};

/**
 * Base stats of a primary kit: one value per movement stat and one per meter maximum. They reach the pawn through the kit
 * instant init effect, whose modifiers read these numbers as set-by-caller magnitudes, so designers edit numbers here and
 * never the effect. The init effect must override a meter maximum before the meter itself (see UGSTN_MeterAttributeSet).
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_KitStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	TArray<FGSTN_StatValue> Values;

	void ApplyToSpec(FGameplayEffectSpec& Spec) const { FGSTN_StatValue::ApplyToSpec(Spec, Values); }

	bool Validate(TArray<FText>& OutErrors) const { return FGSTN_StatValue::Validate(Values, OutErrors); }

	/**
	 * Applies InitEffect to ASC as an instant effect sourced by ASC itself, with the values as magnitudes. Meant for the
	 * loadout, outside the simulation. Returns an invalid handle when InitEffect is null or the effect was not applied.
	 */
	FActiveGameplayEffectHandle ApplyTo(UAbilitySystemComponent& ASC, TSubclassOf<UGameplayEffect> InitEffect, float Level = 1.f) const;
};
