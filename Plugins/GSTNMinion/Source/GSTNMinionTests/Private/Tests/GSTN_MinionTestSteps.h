// Test-only hit step for GSTNMinion.Runtime.Hits.* (M8 G5).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitResolution.h"
#include "GSTN_MinionTestSteps.generated.h"

/**
 * Stands in for a part set in the reaction tier test: plugged into the minion subsystem's optional victim step slot, it caps the tier the
 * way an armoured part would (no parts code; the real bridge is M4b P16). Pure, like every hit step.
 */
UCLASS(NotBlueprintable, meta = (Hidden))
class UGSTN_MinionTestStep_CapReaction : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override
	{
		Out.MaxReaction = EGSTN_HitReaction::StaggerInPlace;
		return EGSTN_StepResult::Continue;
	}
};
