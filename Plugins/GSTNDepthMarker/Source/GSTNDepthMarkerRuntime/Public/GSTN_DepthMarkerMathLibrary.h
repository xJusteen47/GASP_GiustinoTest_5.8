// Blueprint access to the depth marker maths (depth-marker.md §5). Thin wrappers over FGSTN_DepthMarkerMath in the Core module.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_DepthMarkerTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_DepthMarkerMathLibrary.generated.h"

/** Pure geometry for Blueprint selectors and presenters; hot work stays in C++ */
UCLASS()
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthMarkerMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Keeps blocking hits that are below the feet, face up and pass the behaviour query; nearest first */
	UFUNCTION(BlueprintPure, Category = "DepthMarker|Math")
	static TArray<FGSTN_DepthCandidate> FilterCandidates(const TArray<FGSTN_DepthCandidate>& Candidates, FVector Up,
		const FGSTN_DepthSelectorParams& Params, const FGameplayTagQuery& TargetBehaviours);

	/** The nearest candidate; false when the list is empty */
	UFUNCTION(BlueprintPure, Category = "DepthMarker|Math")
	static bool ChooseFirstBelow(const TArray<FGSTN_DepthCandidate>& Filtered, FGSTN_DepthCandidate& OutCandidate);

	/** The point of the plane straight along Up from the feet; S is positive when it lies along Up. False for a parallel plane */
	UFUNCTION(BlueprintPure, Category = "DepthMarker|Math")
	static bool ProjectOntoPlane(FVector Feet, FVector Up, FVector PlanePoint, FVector PlaneNormal, FVector& OutPoint, float& OutS);

	/** The normal rotated towards Up to at most MaxTiltDegrees from it */
	UFUNCTION(BlueprintPure, Category = "DepthMarker|Math")
	static FVector ClampTilt(FVector Normal, FVector Up, float MaxTiltDegrees = 50.f);

	/** Alpha and radius factors over the height above the target */
	UFUNCTION(BlueprintPure, Category = "DepthMarker|Math")
	static void ComputeFade(float Height, const FGSTN_DepthFadeParams& Fade, float& OutAlpha, float& OutRadiusScale);
};
