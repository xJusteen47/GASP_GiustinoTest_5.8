// Depth marker target selector: which surface a marker points at (depth-marker.md §2, §6.5).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_DepthMarkerRuntimeTypes.h"
#include "UObject/Object.h"
#include "GSTN_DepthTargetSelector.generated.h"

/**
 * Chooses the target from trace results. Replaceable (a Blueprint child can implement a cone or a landing preview) and
 * stateless: the per marker memory travels in FGSTN_DepthSelectionState. Presentation only, so Blueprint is free; hot work stays
 * in FGSTN_DepthMarkerMath.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthTargetSelector : public UObject
{
	GENERATED_BODY()

public:
	/** Tuning of the rules of §2; a component that brings its own selector tunes it here, the default one copies the settings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection")
	FGSTN_DepthSelectorParams Params;

	/** Behaviour filter of §2.8 (empty = collision channel only) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DepthMarker|Selection")
	FGameplayTagQuery TargetBehaviours;

	/** The probe segment: from the feet along the entity own Down, ProbeLength long */
	UFUNCTION(BlueprintNativeEvent, Category = "DepthMarker")
	void BuildProbe(const FGSTN_DepthSubjectState& Subject, FVector& OutStart, FVector& OutEnd) const;

	/** Turns the hits of a probe into the below and above targets. bProbed is false on frames without a new probe */
	UFUNCTION(BlueprintNativeEvent, Category = "DepthMarker")
	FGSTN_DepthResult ChooseTarget(const FGSTN_DepthSubjectState& Subject, const TArray<FGSTN_DepthCandidate>& Candidates,
		const FGSTN_DepthFootprintHit& Footprint, const FGSTN_DepthReferenceOverride& Override, UPARAM(ref) FGSTN_DepthSelectionState& State,
		double TimeSeconds, bool bProbed) const;

protected:
	virtual void BuildProbe_Implementation(const FGSTN_DepthSubjectState& Subject, FVector& OutStart, FVector& OutEnd) const;
	virtual FGSTN_DepthResult ChooseTarget_Implementation(const FGSTN_DepthSubjectState& Subject, const TArray<FGSTN_DepthCandidate>& Candidates,
		const FGSTN_DepthFootprintHit& Footprint, const FGSTN_DepthReferenceOverride& Override, FGSTN_DepthSelectionState& State,
		double TimeSeconds, bool bProbed) const;
};

/** The default selector: the first surface straight along gravity with edge tolerance and hysteresis (§2.2) */
UCLASS(Blueprintable, meta = (DisplayName = "Gravity (first surface below)"))
class GSTNDEPTHMARKERRUNTIME_API UGSTN_DepthTargetSelector_Gravity : public UGSTN_DepthTargetSelector
{
	GENERATED_BODY()

protected:
	virtual FGSTN_DepthResult ChooseTarget_Implementation(const FGSTN_DepthSubjectState& Subject, const TArray<FGSTN_DepthCandidate>& Candidates,
		const FGSTN_DepthFootprintHit& Footprint, const FGSTN_DepthReferenceOverride& Override, FGSTN_DepthSelectionState& State,
		double TimeSeconds, bool bProbed) const override;
};
