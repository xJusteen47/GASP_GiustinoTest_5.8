#include "GSTN_DepthMarkerMathLibrary.h"

#include "GSTN_DepthMarkerMath.h"

TArray<FGSTN_DepthCandidate> UGSTN_DepthMarkerMath::FilterCandidates(const TArray<FGSTN_DepthCandidate>& Candidates, FVector Up,
	const FGSTN_DepthSelectorParams& Params, const FGameplayTagQuery& TargetBehaviours)
{
	TArray<FGSTN_DepthCandidate> Out;
	FGSTN_DepthMarkerMath::FilterCandidates(Candidates, Up, Params, TargetBehaviours, Out);
	return Out;
}

bool UGSTN_DepthMarkerMath::ChooseFirstBelow(const TArray<FGSTN_DepthCandidate>& Filtered, FGSTN_DepthCandidate& OutCandidate)
{
	return FGSTN_DepthMarkerMath::ChooseFirstBelow(Filtered, OutCandidate);
}

bool UGSTN_DepthMarkerMath::ProjectOntoPlane(FVector Feet, FVector Up, FVector PlanePoint, FVector PlaneNormal, FVector& OutPoint, float& OutS)
{
	return FGSTN_DepthMarkerMath::ProjectOntoPlane(Feet, Up, PlanePoint, PlaneNormal, OutPoint, OutS);
}

FVector UGSTN_DepthMarkerMath::ClampTilt(FVector Normal, FVector Up, float MaxTiltDegrees)
{
	return FGSTN_DepthMarkerMath::ClampTilt(Normal, Up, MaxTiltDegrees);
}

void UGSTN_DepthMarkerMath::ComputeFade(float Height, const FGSTN_DepthFadeParams& Fade, float& OutAlpha, float& OutRadiusScale)
{
	FGSTN_DepthMarkerMath::ComputeFade(Height, Fade, OutAlpha, OutRadiusScale);
}
