// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Attributes/GSTN_MeterSetup.h"
#include "AbilitySystemComponent.h"

bool UGSTN_MeterUtils::FindMeterSetup(const TArray<FGSTN_MeterSetup>& Setups, FGameplayTag Meter, FGSTN_MeterSetup& OutSetup)
{
	for (const FGSTN_MeterSetup& Setup : Setups)
	{
		if (Setup.Meter == Meter)
		{
			OutSetup = Setup;
			return true;
		}
	}
	OutSetup = FGSTN_MeterSetup();
	return false;
}

int32 UGSTN_MeterUtils::GetFullBars(const FGSTN_MeterSetup& Setup, float Amount)
{
	return Setup.PointsPerBar > 0.f ? FMath::Max(0, FMath::FloorToInt32(Amount / Setup.PointsPerBar)) : 0;
}

float UGSTN_MeterUtils::GetMeterAmount(const UAbilitySystemComponent* ASC, const FGSTN_MeterSetup& Setup)
{
	bool bFound = false;
	const float Amount = ASC != nullptr && Setup.Value.IsValid() ? ASC->GetGameplayAttributeValue(Setup.Value, bFound) : 0.f;
	return bFound ? Amount : 0.f;
}

float UGSTN_MeterUtils::GetMeterMax(const UAbilitySystemComponent* ASC, const FGSTN_MeterSetup& Setup)
{
	bool bFound = false;
	const float Amount = ASC != nullptr && Setup.Max.IsValid() ? ASC->GetGameplayAttributeValue(Setup.Max, bFound) : 0.f;
	return bFound ? Amount : 0.f;
}
