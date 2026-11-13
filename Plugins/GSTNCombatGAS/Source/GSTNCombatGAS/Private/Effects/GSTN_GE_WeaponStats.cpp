// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Effects/GSTN_GE_WeaponStats.h"
#include "AbilitySystemComponent.h"

namespace
{
	TArray<FActiveGameplayEffectHandle> FindActive(const UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_WeaponStats> EffectClass)
	{
		FGameplayEffectQuery Query;
		Query.EffectDefinition = EffectClass;
		return ASC.GetActiveEffects(Query);
	}
}

UGSTN_GE_WeaponStats::UGSTN_GE_WeaponStats()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

FActiveGameplayEffectHandle UGSTN_GE_WeaponStats::Apply(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_WeaponStats> EffectClass,
	TConstArrayView<FGSTN_StatValue> Modifiers, float Level)
{
	if (EffectClass == nullptr)
	{
		return FActiveGameplayEffectHandle();
	}

	const TArray<FActiveGameplayEffectHandle> Active = FindActive(ASC, EffectClass);
	if (!Active.IsEmpty())
	{
		return Active[0];
	}

	FGameplayEffectSpecHandle SpecHandle = ASC.MakeOutgoingSpec(EffectClass, Level, ASC.MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	FGSTN_StatValue::ApplyToSpec(*SpecHandle.Data, Modifiers);
	return ASC.ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
}

int32 UGSTN_GE_WeaponStats::Remove(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_WeaponStats> EffectClass)
{
	if (EffectClass == nullptr)
	{
		return 0;
	}

	int32 Removed = 0;
	for (const FActiveGameplayEffectHandle& Handle : FindActive(ASC, EffectClass))
	{
		Removed += ASC.RemoveActiveGameplayEffect(Handle) ? 1 : 0;
	}
	return Removed;
}

bool UGSTN_GE_WeaponStats::IsApplied(const UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_WeaponStats> EffectClass)
{
	return EffectClass != nullptr && !FindActive(ASC, EffectClass).IsEmpty();
}
