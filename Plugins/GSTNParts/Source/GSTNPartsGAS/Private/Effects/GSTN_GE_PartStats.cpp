// Destructible parts, ability-system half (no Network Prediction, no Mover).

#include "Effects/GSTN_GE_PartStats.h"

#include "AbilitySystemComponent.h"

namespace
{
	TArray<FActiveGameplayEffectHandle> FindActive(const UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_PartStats> EffectClass, int32 PartIndex)
	{
		TArray<FActiveGameplayEffectHandle> Found;
		if (EffectClass == nullptr || PartIndex < 0)
		{
			return Found;
		}
		FGameplayEffectQuery Query;
		Query.EffectDefinition = EffectClass;
		const int32 Level = FMath::RoundToInt(UGSTN_GE_PartStats::LevelForPart(PartIndex));
		for (const FActiveGameplayEffectHandle& Handle : ASC.GetActiveEffects(Query))
		{
			const FActiveGameplayEffect* Active = ASC.GetActiveGameplayEffect(Handle);
			if (Active && FMath::RoundToInt(Active->Spec.GetLevel()) == Level)
			{
				Found.Add(Handle);
			}
		}
		return Found;
	}
}

UGSTN_GE_PartStats::UGSTN_GE_PartStats()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
}

FActiveGameplayEffectHandle UGSTN_GE_PartStats::Apply(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_PartStats> EffectClass, int32 PartIndex,
	TConstArrayView<FGSTN_StatValue> Values)
{
	if (EffectClass == nullptr || PartIndex < 0)
	{
		return FActiveGameplayEffectHandle();
	}
	const TArray<FActiveGameplayEffectHandle> Active = FindActive(ASC, EffectClass, PartIndex);
	if (!Active.IsEmpty())
	{
		return Active[0];
	}
	FGameplayEffectSpecHandle SpecHandle = ASC.MakeOutgoingSpec(EffectClass, LevelForPart(PartIndex), ASC.MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	FGSTN_StatValue::ApplyToSpec(*SpecHandle.Data, Values);
	return ASC.ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
}

int32 UGSTN_GE_PartStats::Remove(UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_PartStats> EffectClass, int32 PartIndex)
{
	int32 Removed = 0;
	for (const FActiveGameplayEffectHandle& Handle : FindActive(ASC, EffectClass, PartIndex))
	{
		Removed += ASC.RemoveActiveGameplayEffect(Handle) ? 1 : 0;
	}
	return Removed;
}

bool UGSTN_GE_PartStats::IsApplied(const UAbilitySystemComponent& ASC, TSubclassOf<UGSTN_GE_PartStats> EffectClass, int32 PartIndex)
{
	return !FindActive(ASC, EffectClass, PartIndex).IsEmpty();
}
