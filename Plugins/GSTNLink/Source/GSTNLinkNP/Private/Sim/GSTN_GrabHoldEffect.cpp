// Links, Network Prediction half: the grabber's walk and jump while holding (grab-link).

#include "Sim/GSTN_GrabHoldEffect.h"

#include "Attributes/GSTN_CombatAttributeSet.h"
#include "GSTN_LinkSettings.h"

namespace
{
	void AddScale(UGameplayEffect& Effect, const FGameplayAttribute& Attribute, TSubclassOf<UGameplayModMagnitudeCalculation> Calculation)
	{
		FCustomCalculationBasedFloat Scale;
		Scale.CalculationClassMagnitude = Calculation;
		FGameplayModifierInfo Modifier;
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::MultiplyCompound;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(Scale);
		Effect.Modifiers.Add(Modifier);
	}
}

float UGSTN_GrabHoldWalkScale::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	return UGSTN_LinkSettings::Get().GetEffectiveHoldWalkScale();
}

float UGSTN_GrabHoldJumpScale::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	return UGSTN_LinkSettings::Get().GetEffectiveHoldJumpScale();
}

UGSTN_GE_GrabHold::UGSTN_GE_GrabHold()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddScale(*this, UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), UGSTN_GrabHoldWalkScale::StaticClass());
	AddScale(*this, UGSTN_CombatAttributeSet::GetJumpVelocityAttribute(), UGSTN_GrabHoldJumpScale::StaticClass());
	AddScale(*this, UGSTN_CombatAttributeSet::GetAirJumpVelocityAttribute(), UGSTN_GrabHoldJumpScale::StaticClass());
}
