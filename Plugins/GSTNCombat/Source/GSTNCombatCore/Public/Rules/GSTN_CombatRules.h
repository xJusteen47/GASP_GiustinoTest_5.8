// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Rules/GSTN_CombatStep.h"
#include "GSTN_CombatRules.generated.h"

/**
 * The rules of a match's combat, as data (combat extension surface, modularity-and-tags 5b). Today that is the activation
 * pipeline: the list of step classes a candidate goes through. Later milestones add the hit resolution pipeline and the
 * tuning of hits to the same asset.
 *
 * It needs no GAS type, so it lives in the Core module; a step that does needs a GAS-aware plugin and is added to the list
 * by that plugin's own rules asset.
 *
 * The list holds classes, not objects: steps are stateless (see UGSTN_CombatStep). Classes are soft references, loaded once
 * by ResolvePipeline outside the simulation, and the simulation only runs the resolved list.
 */
UCLASS(BlueprintType)
class GSTNCOMBATCORE_API UGSTN_CombatRules : public UDataAsset
{
	GENERATED_BODY()

public:
	UGSTN_CombatRules();

	/** Steps in the order they run. A class that is missing (its plugin is not loaded), abstract or null is reported by validation and skipped */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rules|Pipeline")
	TArray<TSoftClassPtr<UGSTN_CombatStep>> Pipeline;

	/**
	 * Loads the classes of Pipeline and keeps the usable ones, in order. Call it once when the match's rules are chosen
	 * (BeginPlay), never while simulating. Calling it again rebuilds the list
	 */
	void ResolvePipeline();

	/** The classes ResolvePipeline kept */
	const TArray<TSubclassOf<UGSTN_CombatStep>>& GetResolvedPipeline() const { return ResolvedPipeline; }

	/**
	 * Runs the resolved steps on View in order; the first Reject ends it and is the result, otherwise the result is
	 * Continue. Safe to call from the simulation: it reads only the resolved list and the step class defaults.
	 */
	FGSTN_CombatStepResult RunPipeline(const FGSTN_CombatStepView& View) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(Transient)
	TArray<TSubclassOf<UGSTN_CombatStep>> ResolvedPipeline;
};
