// Defence, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Data/GSTN_DefenceRules.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_DIRulesMath.generated.h"

/**
 * Directional influence driven by the rules asset (m6 spec 5.5): the M4 launch math (UGSTN_HitMathUtils::ApplyDI) wrapped with the
 * rules' strength curve, elevation curve, minimum knockback, per-victim scales and the hit's DI switch. M4's function is untouched
 * and stays the one place that turns a launch. Stateless; no world, actor or component reads.
 */
UCLASS()
class GSTNDEFENCECORE_API UGSTN_DIRulesMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The rules' strength factor for a stick magnitude (clamped to 0..1 and quantised): StrengthCurve, 1 when the curve is empty */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static double StrengthCurve(const FGSTN_DIRules& Rules, double StickMagnitude);

	/** The factor on MaxDIElevationDot for a launch whose elevation dot (launch . up) has this magnitude: ElevationDeltaByElevation, 1 when empty */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static double ElevationDeltaByElevation(const FGSTN_DIRules& Rules, double AbsElevationDot);

	/** The product of every entry of the map whose tag the victim carries (hierarchical match); 1 with no match. Bosses: an entry of 0 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static double ScaleByVictimTag(const TMap<FGameplayTag, float>& Scales, const FGameplayTagContainer& VictimTags);

	/**
	 * The rules overload of ApplyDI. The launch is returned unchanged when the hit disables DI, when KnockbackMagnitude is below
	 * MinKnockbackForDI, when the stick is zero, or when the strength (stick x StrengthCurve x victim scale) is 0. Otherwise the stick
	 * is rescaled to min(1, |stick| x StrengthCurve(|stick|) x ScaleByVictimTag) along its own direction, MaxDIElevationDot is scaled by
	 * ElevationDeltaByElevation(|elevation dot|) and M4's ApplyDI turns the launch with Rules.Params.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Defence")
	static FVector ApplyDI(const FVector& Launch, const FVector& StickOnPlane, const FVector& Up, const FVector& YawSourceDirection, double KnockbackMagnitude, const FGameplayTagContainer& VictimTags, const FGSTN_DIRules& Rules, bool bHitDisablesDI);
};
