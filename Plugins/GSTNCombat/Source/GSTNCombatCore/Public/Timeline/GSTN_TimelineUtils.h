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

	/**
	 * Authoring helper for editor scripts (Python cannot build a TInstancedStruct): a copy of Data with one hitbox track appended, active for
	 * [StartFrame, EndFrame) and addressing Part. Setup scripts build the test timeline assets with it; it is not for the simulation
	 */
	UFUNCTION(BlueprintCallable, Category = "Combat|Timeline|Authoring")
	static FGSTN_CombatTimelineData AddHitboxTrack(const FGSTN_CombatTimelineData& Data, int32 StartFrame, int32 EndFrame, const FGSTN_PartSelector& Part, const TArray<FGSTN_HitboxSpec>& Hitboxes);
};
