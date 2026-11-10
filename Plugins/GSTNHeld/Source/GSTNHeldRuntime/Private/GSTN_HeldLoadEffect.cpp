// Held objects, the carry load effect (M10 G6).

#include "GSTN_HeldLoadEffect.h"

#include "Attributes/GSTN_CombatAttributeSet.h"
#include "GSTN_HeldSettings.h"
#include "GSTN_HeldTags.h"
#include "GSTN_HeldTypes.h"

namespace
{
	uint8 PayloadOf(const FGameplayEffectSpec& Spec)
	{
		return FGSTN_HeldLoad::PayloadFromMagnitude(Spec.GetSetByCallerMagnitude(GSTNHeldTags::SetByCaller_Held_Load, false, 0.f));
	}

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

float UGSTN_GE_HoldLoad::SpeedScaleOf(uint8 Payload)
{
	int32 SizeIndex = 0;
	float Ratio = 0.f;
	FGSTN_HeldLoad::DecodePayload(Payload, SizeIndex, Ratio);
	return FGSTN_HeldLoad::SpeedScale(UGSTN_HeldSettings::Get().GetSizeClass(SizeIndex), Ratio);
}

float UGSTN_GE_HoldLoad::JumpScaleOf(uint8 Payload)
{
	int32 SizeIndex = 0;
	float Ratio = 0.f;
	FGSTN_HeldLoad::DecodePayload(Payload, SizeIndex, Ratio);
	return FGSTN_HeldLoad::JumpScale(UGSTN_HeldSettings::Get().GetSizeClass(SizeIndex), Ratio);
}

float UGSTN_HoldLoadSpeedScale::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	return UGSTN_GE_HoldLoad::SpeedScaleOf(PayloadOf(Spec));
}

float UGSTN_HoldLoadJumpScale::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	return UGSTN_GE_HoldLoad::JumpScaleOf(PayloadOf(Spec));
}

UGSTN_GE_HoldLoad::UGSTN_GE_HoldLoad()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	AddScale(*this, UGSTN_CombatAttributeSet::GetWalkSpeedAttribute(), UGSTN_HoldLoadSpeedScale::StaticClass());
	AddScale(*this, UGSTN_CombatAttributeSet::GetJumpVelocityAttribute(), UGSTN_HoldLoadJumpScale::StaticClass());
	AddScale(*this, UGSTN_CombatAttributeSet::GetAirJumpVelocityAttribute(), UGSTN_HoldLoadJumpScale::StaticClass());
}
