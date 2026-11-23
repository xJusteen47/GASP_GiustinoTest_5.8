// A native loadout rule used only by the Core automation tests of the loadout rules asset.

#pragma once

#include "CoreMinimal.h"
#include "Loadout/GSTN_LoadoutRules.h"
#include "GSTN_LoadoutRulesTestTypes.generated.h"

/** Stands in for a designer's Blueprint rule: rejects a loadout that carries a skin and warns about every additional weapon */
UCLASS(Hidden)
class UGSTN_TestLoadoutRuleNoSkin : public UGSTN_LoadoutRule
{
	GENERATED_BODY()

public:
	virtual FGSTN_LoadoutRuleResult Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const override
	{
		FGSTN_LoadoutRuleResult Result;
		if (View.Loadout.Skin.IsValid())
		{
			Result.Errors.Add(FText::FromString(TEXT("Custom rule: no skins in this game.")));
		}
		if (!View.Loadout.AdditionalWeapons.IsEmpty())
		{
			Result.Warnings.Add(FText::FromString(TEXT("Custom rule: extra weapons are discouraged.")));
		}
		return Result;
	}
};
