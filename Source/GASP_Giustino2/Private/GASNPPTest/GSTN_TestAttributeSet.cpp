// GAS + NPP + Mover network test harness.

#include "GASNPPTest/GSTN_TestAttributeSet.h"

UGSTN_TestAttributeSet::UGSTN_TestAttributeSet()
{
	InitHealth(50.f);
	InitMaxHealth(100.f);
	InitStamina(100.f);
	InitMaxStamina(100.f);
	InitProbeEventCount(0.f);
	InitProbeTagCount(0.f);
	InitProbeSpeedScale(1.f);
	InitProbeGravityScale(1.f);
}

void UGSTN_TestAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UGSTN_TestAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampAttribute(Attribute, NewValue);
}

void UGSTN_TestAttributeSet::ClampAttribute(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	}
	else if (Attribute == GetProbeSpeedScaleAttribute() || Attribute == GetProbeGravityScaleAttribute())
	{
		// A gravity scale of exactly 0 makes Mover derive the up direction from the default, keep it positive
		NewValue = FMath::Max(NewValue, 0.1f);
	}
}
