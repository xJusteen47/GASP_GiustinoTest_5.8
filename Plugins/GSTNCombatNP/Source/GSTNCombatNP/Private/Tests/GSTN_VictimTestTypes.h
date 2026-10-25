// Extension, step and applier types used only by the NP automation tests of the victim side.

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitExtensions.h"
#include "Hits/GSTN_HitResolution.h"
#include "GSTN_VictimTestTypes.generated.h"

/** An outcome extension the test step copies into the outcome and the test applier counts */
USTRUCT()
struct FGSTN_TestVictimExtension : public FGSTN_HitExtension
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Value = 0;
};

/** Writes two outcome extensions of the test type, to prove the appliers are called once per entry */
UCLASS(Hidden)
class UGSTN_TestVictimExtensionStep : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override
	{
		for (int32 Index = 1; Index <= 2; ++Index)
		{
			FGSTN_TestVictimExtension Extension;
			Extension.Value = Index;
			Out.OutcomeExtensions.Add(FInstancedStruct::Make(Extension));
		}
		return EGSTN_StepResult::Continue;
	}
};
