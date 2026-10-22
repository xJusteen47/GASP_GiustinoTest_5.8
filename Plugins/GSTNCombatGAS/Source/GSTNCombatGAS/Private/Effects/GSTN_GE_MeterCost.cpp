// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Effects/GSTN_GE_MeterCost.h"
#include "GSTN_CombatTags.h"

UGSTN_GE_MeterCost::UGSTN_GE_MeterCost()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
}

void UGSTN_GE_MeterCost::RebuildModifiers()
{
	if (!Meter.IsValid())
	{
		// Nothing picked yet: leave whatever modifiers a designer added by hand
		return;
	}

	FSetByCallerFloat Cost;
	Cost.DataTag = GSTNCombatTags::SetByCaller_MeterCost;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = Meter;
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Cost);

	Modifiers.Reset();
	Modifiers.Add(Modifier);
}

void UGSTN_GE_MeterCost::SetCost(FGameplayEffectSpec& Spec, float Cost)
{
	Spec.SetSetByCallerMagnitude(GSTNCombatTags::SetByCaller_MeterCost, -FMath::Max(Cost, 0.f));
}

void UGSTN_GE_MeterCost::PostLoad()
{
	Super::PostLoad();
	RebuildModifiers();
}

#if WITH_EDITOR
void UGSTN_GE_MeterCost::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UGSTN_GE_MeterCost, Meter))
	{
		RebuildModifiers();
	}
}
#endif
