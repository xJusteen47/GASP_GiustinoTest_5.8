// Native stand-ins for the Blueprint rule classes, used only by the Core automation tests.

#pragma once

#include "CoreMinimal.h"
#include "Input/GSTN_BlueprintRules.h"
#include "GSTN_TestBlueprintRules.generated.h"

/** Holds when the Strong button (the second test button) is down at the anchor frame. */
UCLASS(Hidden)
class UGSTN_TestCondition_StrongHeld : public UGSTN_InputConditionBlueprint
{
	GENERATED_BODY()

public:
	virtual bool Matches_Implementation(const FGSTN_BlueprintConditionView& View) const override;
};

/** Scores 50 on a special-tier command, the slot's manual offset otherwise: a rule that reads the view only. */
UCLASS(Hidden)
class UGSTN_TestCriterion_TierBonus : public UGSTN_PriorityCriterionBlueprint
{
	GENERATED_BODY()

public:
	virtual int32 Score_Implementation(const FGSTN_BlueprintPriorityView& View) const override;
};

/** Flushes when the frame events carry any event tag, or after 4 idle frames. */
UCLASS(Hidden)
class UGSTN_TestResetRule_AnyEventOrIdle : public UGSTN_BufferResetRuleBlueprint
{
	GENERATED_BODY()

public:
	virtual bool ShouldFlush_Implementation(const FGSTN_BlueprintResetView& View) const override;
};
