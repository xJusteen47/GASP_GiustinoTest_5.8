// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitResolution.h"

class UGSTN_CombatRules;

/** What a run of the pipeline did */
struct FGSTN_HitRunResult
{
	int32 StepsRun = 0;

	/** A step returned Stop */
	bool bStopped = false;

	/** Index in the resolved pipeline of the step that stopped it; INDEX_NONE if none did */
	int32 StoppedAtStep = INDEX_NONE;
};

/**
 * Runs the resolved hit pipeline of a rules asset over one context (combat-m4-hits 3). Stateless; safe to call from the
 * simulation: it reads only the resolved list and the step class defaults.
 */
struct GSTNCOMBATCORE_API FGSTN_HitPipelineRunner
{
	/**
	 * Runs the steps in order. Each gets a view of the context as the previous steps left it and writes a result that is merged
	 * before the next one runs. A step that returns Stop ends the run. An empty pipeline changes nothing.
	 */
	static FGSTN_HitRunResult Run(const UGSTN_CombatRules& Rules, FGSTN_HitResolutionContext& Context);

	/** Merges one step's result into the context (see FGSTN_StepResult for the rule of each field) */
	static void Merge(const FGSTN_StepResult& Result, FGSTN_HitResolutionContext& Context);

	/** The const view a step receives */
	static void MakeView(const UGSTN_CombatRules* Rules, const FGSTN_HitResolutionContext& Context, FGSTN_HitContextView& OutView);

	/**
	 * The victim after this outcome: its damage percent (or health fraction) moved by Outcome.Damage, as the damage mode counts
	 * it. The caller applies it between two entries of the same inbox drain, so the second hit sees the weight the first one left
	 * (combat-m4-hits 7, drain order). Does nothing for a hit that did not land.
	 */
	static void AdvanceVictim(const UGSTN_CombatRules& Rules, const FGSTN_HitOutcome& Outcome, FGSTN_HitVictimView& Victim);
};
