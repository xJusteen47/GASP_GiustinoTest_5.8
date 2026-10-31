// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_WeaponStatsSwap.h"

#include "AbilitySystemComponent.h"

bool FGSTN_WeaponStatsSwap::Execute(UAbilitySystemComponent& ASC, TConstArrayView<FGSTN_WeaponStatsEntry> Weapons, int32 OldIndex, int32 NewIndex)
{
	if (OldIndex != NewIndex && Weapons.IsValidIndex(OldIndex))
	{
		UGSTN_GE_WeaponStats::Remove(ASC, Weapons[OldIndex].Effect);
	}
	if (!Weapons.IsValidIndex(NewIndex))
	{
		return false;
	}
	const FGSTN_WeaponStatsEntry& Weapon = Weapons[NewIndex];
	return UGSTN_GE_WeaponStats::Apply(ASC, Weapon.Effect, Weapon.Stats).IsValid() || UGSTN_GE_WeaponStats::IsApplied(ASC, Weapon.Effect);
}
