// Defence, ability-system half (no Network Prediction, no Mover).

#include "Attributes/GSTN_DefenceAttributeSet.h"

#include "Data/GSTN_DefenceRules.h"
#include "Math/GSTN_DefenceViews.h"

UGSTN_DefenceAttributeSet::UGSTN_DefenceAttributeSet()
{
	// The data defaults of FGSTN_ShieldRules, so a set that is never initialised from rules (a test, a tool) still reads sanely
	const FGSTN_ShieldRules Defaults;
	InitMaxShieldHealth(Defaults.MaxShield);
	InitShieldHealth(Defaults.MaxShield);
	InitShieldRegenRate(Defaults.RegenPerFrame);
	InitShieldDrainRate(Defaults.DrainPerFrame);
	InitIncomingShieldDamageMultiplier(1.f);
	InitGrabResistance(1.f);
}

void UGSTN_DefenceAttributeSet::InitFromRules(const FGSTN_ShieldRules& Rules)
{
	// The maximum first, so the full health is not clamped against the old maximum
	InitMaxShieldHealth(Rules.MaxShield);
	InitShieldHealth(Rules.MaxShield);
	InitShieldRegenRate(Rules.RegenPerFrame);
	InitShieldDrainRate(Rules.DrainPerFrame);
	InitIncomingShieldDamageMultiplier(1.f);
	InitGrabResistance(1.f);
}

FGSTN_ShieldRates UGSTN_DefenceAttributeSet::ReadShieldRates(int32 RegenDelayFrames) const
{
	FGSTN_ShieldRates Rates;
	Rates.Drain = GetShieldDrainRate();
	Rates.Regen = GetShieldRegenRate();
	Rates.Max = GetMaxShieldHealth();
	Rates.RegenDelayFrames = RegenDelayFrames;
	return Rates;
}

void UGSTN_DefenceAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	Clamp(Attribute, NewValue);
}

void UGSTN_DefenceAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	Clamp(Attribute, NewValue);
}

void UGSTN_DefenceAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);
	// A maximum that dropped below the health pulls the health down; the clamp of PreAttributeChange only sees the attribute being written
	if (Attribute == GetMaxShieldHealthAttribute() && GetShieldHealth() > NewValue)
	{
		SetShieldHealth(NewValue);
	}
}

void UGSTN_DefenceAttributeSet::Clamp(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetShieldHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, GetMaxShieldHealth());
		return;
	}
	// Every other attribute of this set is a maximum, a rate or a scale: none is meaningful below zero
	if (Attribute.GetAttributeSetClass() == StaticClass())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}
