// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Attributes/GSTN_KitStats.h"
#include "AbilitySystemComponent.h"
#include "GSTN_CombatTags.h"

#define LOCTEXT_NAMESPACE "GSTNCombatGAS"

void FGSTN_StatValue::ApplyToSpec(FGameplayEffectSpec& Spec, TConstArrayView<FGSTN_StatValue> Values)
{
	for (const FGSTN_StatValue& Entry : Values)
	{
		if (Entry.Stat.IsValid())
		{
			Spec.SetSetByCallerMagnitude(Entry.Stat, Entry.Value);
		}
	}
}

bool FGSTN_StatValue::Validate(TConstArrayView<FGSTN_StatValue> Values, TArray<FText>& OutErrors)
{
	const int32 ErrorsBefore = OutErrors.Num();
	const FGameplayTag Root = GSTNCombatTags::Root_SetByCallerStat.GetTag();
	TSet<FGameplayTag> Seen;
	for (const FGSTN_StatValue& Entry : Values)
	{
		if (!Entry.Stat.IsValid())
		{
			OutErrors.Add(LOCTEXT("StatNone", "A stat value has no tag."));
			continue;
		}
		if (!Entry.Stat.MatchesTag(Root) || Entry.Stat.MatchesTagExact(Root))
		{
			OutErrors.Add(FText::Format(LOCTEXT("StatOutsideRoot", "Stat tag {0} must be a leaf under {1}."),
				FText::FromName(Entry.Stat.GetTagName()), FText::FromName(Root.GetTagName())));
		}
		bool bAlreadyListed = false;
		Seen.Add(Entry.Stat, &bAlreadyListed);
		if (bAlreadyListed)
		{
			OutErrors.Add(FText::Format(LOCTEXT("StatTwice", "Stat tag {0} is listed twice."), FText::FromName(Entry.Stat.GetTagName())));
		}
	}
	return OutErrors.Num() == ErrorsBefore;
}

FActiveGameplayEffectHandle FGSTN_KitStats::ApplyTo(UAbilitySystemComponent& ASC, TSubclassOf<UGameplayEffect> InitEffect, float Level) const
{
	if (InitEffect == nullptr)
	{
		return FActiveGameplayEffectHandle();
	}
	FGameplayEffectSpecHandle SpecHandle = ASC.MakeOutgoingSpec(InitEffect, Level, ASC.MakeEffectContext());
	if (!SpecHandle.IsValid())
	{
		return FActiveGameplayEffectHandle();
	}
	ApplyToSpec(*SpecHandle.Data);
	return ASC.ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
}

#undef LOCTEXT_NAMESPACE
