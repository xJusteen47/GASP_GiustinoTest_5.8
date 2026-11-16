// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Effects/GSTN_GE_MeterGain.h"
#include "GSTN_CombatTags.h"

UGSTN_GE_MeterGain::UGSTN_GE_MeterGain()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
}

void UGSTN_GE_MeterGain::RebuildModifiers()
{
	if (!Meter.IsValid())
	{
		// Nothing picked yet: leave whatever modifiers a designer added by hand
		return;
	}

	FSetByCallerFloat Gain;
	Gain.DataTag = GSTNCombatTags::SetByCaller_Meter;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = Meter;
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Gain);

	Modifiers.Reset();
	Modifiers.Add(Modifier);
}

void UGSTN_GE_MeterGain::PostLoad()
{
	Super::PostLoad();
	RebuildModifiers();
}

#if WITH_EDITOR
void UGSTN_GE_MeterGain::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(UGSTN_GE_MeterGain, Meter))
	{
		RebuildModifiers();
	}
}
#endif
