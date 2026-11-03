#include "GSTN_DepthTargetSelector.h"

#include "GSTN_DepthMarkerMath.h"

void UGSTN_DepthTargetSelector::BuildProbe_Implementation(const FGSTN_DepthSubjectState& Subject, FVector& OutStart, FVector& OutEnd) const
{
	OutStart = Subject.Feet;
	OutEnd = Subject.Feet - Subject.Up.GetSafeNormal() * Params.ProbeLength;
}

FGSTN_DepthTarget UGSTN_DepthTargetSelector::ChooseApproach_Implementation(const FGSTN_DepthSubjectState& Subject, const FGSTN_DepthTarget& Primary,
	const TArray<FGSTN_DepthPlaneCandidate>& Probed, const TArray<FGSTN_DepthPlaneCandidate>& Explicit, FGSTN_DepthSelectionState& State,
	double TimeSeconds) const
{
	FGSTN_DepthTarget Approach;
	FGSTN_DepthMarkerMath::EvaluateApproach(State, Subject, Params, Primary, Probed, Explicit, TimeSeconds, Approach);
	return Approach;
}

FGSTN_DepthResult UGSTN_DepthTargetSelector::ChooseTarget_Implementation(const FGSTN_DepthSubjectState& Subject,
	const TArray<FGSTN_DepthCandidate>& Candidates, const FGSTN_DepthFootprintHit& Footprint, const FGSTN_DepthReferenceOverride& Override,
	FGSTN_DepthSelectionState& State, double TimeSeconds, bool bProbed) const
{
	return FGSTN_DepthResult();
}

FGSTN_DepthResult UGSTN_DepthTargetSelector_Gravity::ChooseTarget_Implementation(const FGSTN_DepthSubjectState& Subject,
	const TArray<FGSTN_DepthCandidate>& Candidates, const FGSTN_DepthFootprintHit& Footprint, const FGSTN_DepthReferenceOverride& Override,
	FGSTN_DepthSelectionState& State, double TimeSeconds, bool bProbed) const
{
	return FGSTN_DepthMarkerMath::Evaluate(State, Subject, Candidates, Footprint, Override, Params, TargetBehaviours, bProbed, TimeSeconds);
}
