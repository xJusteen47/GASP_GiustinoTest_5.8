// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_CombatTimeline.generated.h"

/**
 * A shared timeline: the same FGSTN_CombatTimelineData as an ability's inline one, in an asset a binding can point to, so
 * one ability class serves several weapons. Referenced softly by abilities and bindings; primary asset type
 * GSTN_CombatTimeline.
 */
UCLASS(BlueprintType)
class GSTNCOMBATCORE_API UGSTN_CombatTimeline : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timeline")
	FGSTN_CombatTimelineData Data;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
