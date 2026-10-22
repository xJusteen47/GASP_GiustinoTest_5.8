// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_TimelineUtils.generated.h"

/**
 * Stateless questions about a timeline's data for Blueprint (details-panel helpers, editor utilities, UI). Per-frame
 * evaluation inside the simulation stays in FGSTN_TimelinePlayer, in C++.
 */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_TimelineUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Is Frame inside the cancellable tail of the timeline? False without a tail */
	UFUNCTION(BlueprintPure, Category = "Combat|Timeline")
	static bool IsInTail(const FGSTN_CombatTimelineData& Data, int32 Frame);

	/** First frame of the cancellable tail, -1 if the timeline has none */
	UFUNCTION(BlueprintPure, Category = "Combat|Timeline")
	static int32 GetTailStartFrame(const FGSTN_CombatTimelineData& Data);
};
