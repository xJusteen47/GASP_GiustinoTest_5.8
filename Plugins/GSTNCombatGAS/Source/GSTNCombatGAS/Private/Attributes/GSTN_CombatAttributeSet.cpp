// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Attributes/GSTN_CombatAttributeSet.h"
#include "Settings/GSTN_CombatGASSettings.h"

void UGSTN_CombatAttributeSet::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UGSTN_CombatGASSettings::Get().ApplyAttributeDefaults(*this);
	}
}

void UGSTN_CombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampStat(Attribute, NewValue);
}

void UGSTN_CombatAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampStat(Attribute, NewValue);
}

void UGSTN_CombatAttributeSet::ClampStat(const FGameplayAttribute& Attribute, float& NewValue) const
{
	const bool bGravityScale = Attribute == GetJumpRiseGravityScaleAttribute() || Attribute == GetJumpCutGravityScaleAttribute()
		|| Attribute == GetFallGravityScaleAttribute() || Attribute == GetFastFallGravityScaleAttribute();
	if (bGravityScale)
	{
		NewValue = FMath::Max(NewValue, MinGravityScale);
		return;
	}

	// Every other attribute of this set is a speed, a rate, a weight or a count: none is meaningful below zero
	if (Attribute.GetAttributeSetClass() == StaticClass())
	{
		NewValue = FMath::Max(NewValue, 0.f);
	}
}
