// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_PartsMinionExtension.h"

#include "Data/GSTN_PartBreakEffects.h"
#include "Data/GSTN_PartsValidation.h"

uint8 FGSTN_PartsMinionExtension::ResolveLethalMask(TConstArrayView<FGSTN_PartDefinition> Parts) const
{
	uint8 Mask = 0;
	const int32 Num = FMath::Min(Parts.Num(), GSTN_PartsLimits::MaxMinionParts);
	for (const FName& Lethal : LethalParts)
	{
		for (int32 Index = 0; Index < Num; ++Index)
		{
			if (Parts[Index].Name == Lethal)
			{
				Mask |= static_cast<uint8>(1u << Index);
				break;
			}
		}
	}
	return Mask;
}

void FGSTN_PartsMinionExtension::Validate(TConstArrayView<FGSTN_PartDefinition> Parts, FGSTN_PartsValidationReport& Report) const
{
	const int32 Num = FMath::Min(Parts.Num(), GSTN_PartsLimits::MaxMinionParts);
	for (const FName& Lethal : LethalParts)
	{
		bool bFound = false;
		for (int32 Index = 0; Index < Num && !bFound; ++Index)
		{
			bFound = Parts[Index].Name == Lethal;
		}
		if (!bFound)
		{
			Report.AddError(FString::Printf(TEXT("The lethal part '%s' is not one of the first %d parts of the set."), *Lethal.ToString(), GSTN_PartsLimits::MaxMinionParts));
		}
	}

	const uint8 LethalMask = ResolveLethalMask(Parts);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		if ((LethalMask & (1u << Index)) != 0)
		{
			continue;
		}
		for (const FInstancedStruct& Entry : Parts[Index].OnBreak)
		{
			const FGSTN_PartBreakEffect_MainDamage* MainDamage = Entry.GetPtr<FGSTN_PartBreakEffect_MainDamage>();
			if (MainDamage && MainDamage->bKill)
			{
				Report.AddWarning(FString::Printf(TEXT("Part %d ('%s') has a Kill break effect but is not a lethal part: a minion ignores the Kill flag, list the part in LethalParts to make its loss fatal."), Index, *Parts[Index].Name.ToString()));
			}
		}
	}
}

bool FGSTN_PartsMinionRules::CanMinionRunBreakEffect(const FInstancedStruct& Effect)
{
	return Effect.GetPtr<FGSTN_PartBreakEffect_Stats>() == nullptr;
}
