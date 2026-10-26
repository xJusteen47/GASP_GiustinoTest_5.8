// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Attributes/GSTN_KitStats.h"
#include "Effects/GSTN_GE_WeaponStats.h"
#include "GSTN_WeaponStatsSwap.generated.h"

class UAbilitySystemComponent;

/** The stats effect of one weapon of the combatant (combat-m3 10.2): its class, and the stat magnitudes the effect reads as set-by-caller values */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_WeaponStatsEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TSubclassOf<UGSTN_GE_WeaponStats> Effect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon")
	TArray<FGSTN_StatValue> Stats;
};

/**
 * The stats effect swap of a weapon switch (combat-m3 7.3, SwapWeaponStats). The effects are found by their class on the ability
 * system, never by a stored handle, so the swap needs nothing but the two indices the combat state carries and gives the same result
 * run again over the same ability system state: a re-simulated frame removes what is already gone and finds what is already applied.
 */
struct GSTNCOMBATNP_API FGSTN_WeaponStatsSwap
{
	/**
	 * Takes the stats effect of the weapon at OldIndex off ASC (when it is another weapon than the new one) and makes sure the one of
	 * NewIndex is on it. An index the list does not have has no effect to remove or apply.
	 * @return true when the effect of NewIndex is applied afterwards
	 */
	static bool Execute(UAbilitySystemComponent& ASC, TConstArrayView<FGSTN_WeaponStatsEntry> Weapons, int32 OldIndex, int32 NewIndex);
};
