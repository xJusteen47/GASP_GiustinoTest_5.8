// Native pipeline steps used only by the Core automation tests of the rules asset.

#pragma once

#include "CoreMinimal.h"
#include "Rules/GSTN_CombatStep.h"
#include "GSTN_RulesTestTypes.generated.h"

namespace GSTNRulesTest
{
	/** Ids of the steps that ran, in order; cleared by each test */
	inline TArray<int32>& RunLog()
	{
		static TArray<int32> Log;
		return Log;
	}
}

UCLASS(Hidden)
class UGSTN_TestStepOne : public UGSTN_CombatStep
{
	GENERATED_BODY()

public:
	virtual FGSTN_CombatStepResult Evaluate_Implementation(const FGSTN_CombatStepView& View) const override
	{
		GSTNRulesTest::RunLog().Add(1);
		return FGSTN_CombatStepResult();
	}
};

UCLASS(Hidden)
class UGSTN_TestStepTwo : public UGSTN_CombatStep
{
	GENERATED_BODY()

public:
	virtual FGSTN_CombatStepResult Evaluate_Implementation(const FGSTN_CombatStepView& View) const override
	{
		GSTNRulesTest::RunLog().Add(2);
		return FGSTN_CombatStepResult();
	}
};

UCLASS(Hidden)
class UGSTN_TestStepThree : public UGSTN_CombatStep
{
	GENERATED_BODY()

public:
	virtual FGSTN_CombatStepResult Evaluate_Implementation(const FGSTN_CombatStepView& View) const override
	{
		GSTNRulesTest::RunLog().Add(3);
		return FGSTN_CombatStepResult();
	}
};

/** Rejects every candidate; logs 9 */
UCLASS(Hidden)
class UGSTN_TestStepReject : public UGSTN_CombatStep
{
	GENERATED_BODY()

public:
	virtual FGSTN_CombatStepResult Evaluate_Implementation(const FGSTN_CombatStepView& View) const override
	{
		GSTNRulesTest::RunLog().Add(9);
		FGSTN_CombatStepResult Result;
		Result.Verdict = EGSTN_CombatStepVerdict::Reject;
		return Result;
	}
};
