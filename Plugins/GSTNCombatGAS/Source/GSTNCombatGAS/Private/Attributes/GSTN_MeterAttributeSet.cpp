// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Attributes/GSTN_MeterAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "Settings/GSTN_CombatGASSettings.h"

const TArray<UGSTN_MeterAttributeSet::FMeterPair>& UGSTN_MeterAttributeSet::GetMeterPairs()
{
	static const TArray<FMeterPair> Pairs = {
		{ GetExMeterAttribute(), GetExMeterMaxAttribute() },
		{ GetSuperMeterAttribute(), GetSuperMeterMaxAttribute() },
	};
	return Pairs;
}

void UGSTN_MeterAttributeSet::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		UGSTN_CombatGASSettings::Get().ApplyAttributeDefaults(*this);
	}
}

void UGSTN_MeterAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	ClampMeter(Attribute, NewValue);
}

void UGSTN_MeterAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	ClampMeter(Attribute, NewValue);
}

void UGSTN_MeterAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// A lowered maximum must take the meter down with it; the write goes through the base clamp above
	for (const FMeterPair& Pair : GetMeterPairs())
	{
		if (Data.EvaluatedData.Attribute == Pair.Max)
		{
			Data.Target.SetNumericAttributeBase(Pair.Value, Data.Target.GetNumericAttributeBase(Pair.Value));
		}
	}
}

void UGSTN_MeterAttributeSet::ClampMeter(const FGameplayAttribute& Attribute, float& NewValue) const
{
	for (const FMeterPair& Pair : GetMeterPairs())
	{
		if (Attribute == Pair.Value)
		{
			NewValue = FMath::Clamp(NewValue, 0.f, FMath::Max(Pair.Max.GetNumericValue(this), 0.f));
			return;
		}
		if (Attribute == Pair.Max)
		{
			NewValue = FMath::Max(NewValue, 0.f);
			return;
		}
	}
}
