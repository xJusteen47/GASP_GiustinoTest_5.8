// Defence, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Data/GSTN_DefenceRules.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/GSTN_DefenceViews.h"
#include "GSTN_SDIMath.generated.h"

/**
 * Held-direction SDI (m6 spec 5.5): during the victim's hitstop, HOLDING a direction drifts the victim that way, there is no
 * mashing and no direction event. One call gives one frame's drift velocity; the caller accumulates the distance (the sync field
 * SDIDistanceUsed) and passes it back, so a resimulation reproduces every frame. Stateless; no world, actor or component reads.
 */
UCLASS()
class GSTNDEFENCECORE_API UGSTN_SDIMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * The drift for one hitstop frame. Blocked (bBlocked, zero velocity) on a throw, on an armored victim unless the rules allow it,
	 * and in the stagger-in-place tier unless the rules allow it. A stick below MinZone drifts nothing. Otherwise the velocity is
	 * Stick x StrengthCurve(|Stick|) x DriftSpeedCmPerSec x ScaleByVictimTag (the stick's own magnitude, clamped to 1), limited so the
	 * accumulated distance never passes MaxDistanceCmPerHit: the frame that would pass it drifts only the remainder. With
	 * bScaleByHitstopFrames the speed is also capped at MaxDistanceCmPerHit / (HitstopFrames x FrameSeconds), which spreads the maximum
	 * evenly over the hitstop instead of letting a long hitstop reach it early.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static FGSTN_SDIStep NextDisplacement(const FGSTN_SDIRules& Rules, const FVector& StickOnPlane, double AccumulatedDistanceCm, bool bArmored, bool bStaggerInPlace, bool bThrow, const FGameplayTagContainer& VictimTags, int32 HitstopFrames, double FrameSeconds = 0.016);
};
