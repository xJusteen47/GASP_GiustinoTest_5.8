// Defence, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Rules/GSTN_CombatRules.h"
#include "GSTN_DefenceRulesExtension.generated.h"

class UGSTN_DefenceRules;

/**
 * The defence rules of a match, as an entry of the combat rules asset's extension array (UGSTN_CombatRules::RulesExtensions), so
 * the rules asset never grows a property that names defence (m6 spec 1d). It is the default for kits without a defence
 * extension of their own; the asset is chosen per game mode like the rest of the combat rules (PvP: bShieldEnabled true, PvE:
 * false). Empty = the plugin's settings name the default, and if they name none there are no defence rules.
 */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_DefenceRulesExtension : public FGSTN_RulesExtension
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence")
	TSoftObjectPtr<UGSTN_DefenceRules> Rules;
};
