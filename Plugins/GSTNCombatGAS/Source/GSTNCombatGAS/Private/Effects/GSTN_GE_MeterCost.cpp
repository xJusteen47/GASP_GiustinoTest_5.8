// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Effects/GSTN_GE_MeterCost.h"

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

	// The scalable float is negated here (Value multiplies the curve, if any), so the data stays a positive cost
	FScalableFloat Spent = Cost;
	Spent.Value = -FMath::Max(Cost.Value, 0.f);

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = Meter;
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Spent);

	Modifiers.Reset();
	Modifiers.Add(Modifier);
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
	// Member name: an edit of Cost reports the inner field of the scalable float
	const FName Member = PropertyChangedEvent.GetMemberPropertyName();
	if (Member == GET_MEMBER_NAME_CHECKED(UGSTN_GE_MeterCost, Meter) || Member == GET_MEMBER_NAME_CHECKED(UGSTN_GE_MeterCost, Cost))
	{
		RebuildModifiers();
	}
}
#endif
