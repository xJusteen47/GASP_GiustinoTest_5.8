// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Attributes/GSTN_HealthAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Settings/GSTN_CombatGASSettings.h"

void UGSTN_HealthAttributeSet::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UGSTN_CombatGASSettings::Get().ApplyAttributeDefaults(*this);
	}
}

void UGSTN_HealthAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampValue(Attribute, NewValue);
}

void UGSTN_HealthAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampValue(Attribute, NewValue);
}

void UGSTN_HealthAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// A lowered maximum takes the health down with it; the write goes through the base clamp above
	if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute())
	{
		Data.Target.SetNumericAttributeBase(GetHealthAttribute(), Data.Target.GetNumericAttributeBase(GetHealthAttribute()));
	}
}

void UGSTN_HealthAttributeSet::ClampValue(const FGameplayAttribute& Attribute, float& NewValue) const
{
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.f, FMath::Max(GetMaxHealthAttribute().GetNumericValue(this), 0.f));
	}
	else if (Attribute == GetMaxHealthAttribute() || Attribute == GetDamagePercentAttribute() || Attribute == GetOutgoingDamageMultiplierAttribute()
		|| Attribute == GetIncomingDamageMultiplierAttribute() || Attribute == GetKnockbackResistanceAttribute() || Attribute == GetHitstunResistanceAttribute())
	{
		// None of these can be negative: a negative multiplier would heal, a negative resistance would amplify
		NewValue = FMath::Max(NewValue, 0.f);
	}
}
