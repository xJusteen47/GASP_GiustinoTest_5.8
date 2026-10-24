// Native hit resolution steps used only by the Core automation tests of the hit pipeline.

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitResolution.h"
#include "GSTN_PipelineTestTypes.generated.h"

namespace GSTNPipelineTest
{
	/** Ids of the steps that ran, in order; cleared by each test */
	inline TArray<int32>& RunLog()
	{
		static TArray<int32> Log;
		return Log;
	}
}

UCLASS(Hidden)
class UGSTN_TestHitStepOne : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override
	{
		GSTNPipelineTest::RunLog().Add(1);
		return EGSTN_StepResult::Continue;
	}
};

UCLASS(Hidden)
class UGSTN_TestHitStepTwo : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override
	{
		GSTNPipelineTest::RunLog().Add(2);
		return EGSTN_StepResult::Continue;
	}
};

UCLASS(Hidden)
class UGSTN_TestHitStepStop : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override
	{
		GSTNPipelineTest::RunLog().Add(9);
		return EGSTN_StepResult::Stop;
	}
};

UCLASS(Hidden)
class UGSTN_TestHitStepThree : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override
	{
		GSTNPipelineTest::RunLog().Add(3);
		return EGSTN_StepResult::Continue;
	}
};

/** Writes modifiers the way a plugin step would: half the weight, double the knockback, a threshold and a cap */
UCLASS(Hidden)
class UGSTN_TestHitStepModifiers : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override
	{
		Out.WeightScale = 0.5f;
		Out.KnockbackScale = 2.f;
		Out.StaggerThreshold.Value = 5.f;
		Out.MaxReaction = EGSTN_HitReaction::PushBack;
		return EGSTN_StepResult::Continue;
	}
};
