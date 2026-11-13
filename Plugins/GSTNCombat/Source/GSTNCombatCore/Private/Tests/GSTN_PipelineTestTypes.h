// Native hit resolution steps used only by the Core automation tests of the hit pipeline.

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitExtensions.h"
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

	/** Extension values the Blueprint-form applier test double saw, in order; cleared by each test */
	inline TArray<int32>& AppliedLog()
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

/** Stands for a Blueprint outcome applier: records the value of the extension it was given (the extension struct is set by the test) */
UCLASS(Hidden)
class UGSTN_TestOutcomeApplier : public UGSTN_OutcomeApplier
{
	GENERATED_BODY()

public:
	virtual FGSTN_OutcomeApplyResult Apply_Implementation(const FGSTN_HitContextView& View, const FInstancedStruct& Extension) const override
	{
		GSTNPipelineTest::AppliedLog().Add(Extension.IsValid() ? 1 : 0);
		FGSTN_OutcomeApplyResult Result;
		Result.Events.Add(FGameplayTag());
		return Result;
	}
};

/** Stands for a Blueprint attacker outcome that always bounces */
UCLASS(Hidden)
class UGSTN_TestAttackerOutcomeRicochet : public UGSTN_AttackerOutcome
{
	GENERATED_BODY()

public:
	virtual EGSTN_AttackerHitOutcome Evaluate_Implementation(const FGSTN_HitContextView& View) const override
	{
		return EGSTN_AttackerHitOutcome::Ricochet;
	}
};

/** A Blueprint attacker outcome that overrides nothing: the base class default */
UCLASS(Hidden)
class UGSTN_TestAttackerOutcomeDefault : public UGSTN_AttackerOutcome
{
	GENERATED_BODY()
};
