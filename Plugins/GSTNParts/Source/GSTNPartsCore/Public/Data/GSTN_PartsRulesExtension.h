// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_PartsPresentationBudget.h"
#include "GSTN_PartsRulesExtension.generated.h"

class UGSTN_PenetrationTable;

/**
 * Game-wide values of the parts plugin that belong to a match's combat rules rather than to one entity (combat-parts.md §2):
 * the default penetration table, the weight defaults and the debris budget. It is meant to be an entry of the rules asset's
 * extension array, so the rules asset never grows a property that names parts. That array's base struct is not declared yet,
 * so this struct stands alone until the pipeline step arrives.
 *
 * Every field falls back on the plugin's settings when left at its "inherit" value, so an empty extension changes nothing.
 * Override order, lowest to highest: plugin settings, this extension, the part set's own value.
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartsRulesExtension
{
	GENERATED_BODY()

	/** Table for part sets that name none. Empty = the settings' table, then the built-in three bands */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts")
	TSoftObjectPtr<UGSTN_PenetrationTable> DefaultPenetrationTable;

	/** Weight scale at the cap of the default weight curve. Negative = the settings' value */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Weight")
	float DefaultWeightScaleAtCap = -1.f;

	/** Floor of the effective weight as a fraction of the base weight. Negative = the settings' value */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Weight")
	float MinWeightScale = -1.f;

	/** Use PresentationBudget instead of the settings' ceilings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Presentation")
	bool bOverridePresentationBudget = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Presentation", meta = (EditCondition = "bOverridePresentationBudget"))
	FGSTN_PartsPresentationBudget PresentationBudget;
};
