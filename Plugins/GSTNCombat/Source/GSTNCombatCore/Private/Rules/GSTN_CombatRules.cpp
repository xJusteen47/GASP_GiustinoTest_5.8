#include "Rules/GSTN_CombatRules.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogGSTNCombatRules, Log, All);

namespace
{
	/** Null when the entry names nothing usable; Reason says why */
	UClass* LoadStepClass(const TSoftClassPtr<UGSTN_CombatStep>& Entry, FString& Reason)
	{
		if (Entry.IsNull())
		{
			Reason = TEXT("no class set");
			return nullptr;
		}

		UClass* Class = Entry.LoadSynchronous();
		if (!Class)
		{
			Reason = FString::Printf(TEXT("class %s was not found (is its plugin loaded?)"), *Entry.ToString());
		}
		else if (Class->HasAnyClassFlags(CLASS_Abstract))
		{
			Reason = FString::Printf(TEXT("class %s is abstract"), *Class->GetName());
			Class = nullptr;
		}
		return Class;
	}
}

UGSTN_CombatRules::UGSTN_CombatRules()
{
	Pipeline.Add(TSoftClassPtr<UGSTN_CombatStep>(UGSTN_CombatStep_Activation::StaticClass()));
}

void UGSTN_CombatRules::ResolvePipeline()
{
	ResolvedPipeline.Reset();
	for (int32 Index = 0; Index < Pipeline.Num(); ++Index)
	{
		FString Reason;
		if (UClass* Class = LoadStepClass(Pipeline[Index], Reason))
		{
			ResolvedPipeline.Add(Class);
		}
		else
		{
			UE_LOG(LogGSTNCombatRules, Warning, TEXT("%s: pipeline step %d skipped, %s"), *GetPathName(), Index, *Reason);
		}
	}
}

FGSTN_CombatStepResult UGSTN_CombatRules::RunPipeline(const FGSTN_CombatStepView& View) const
{
	for (const TSubclassOf<UGSTN_CombatStep>& StepClass : ResolvedPipeline)
	{
		const UGSTN_CombatStep* Step = StepClass ? GetDefault<UGSTN_CombatStep>(StepClass) : nullptr;
		if (!Step)
		{
			continue;
		}

		const FGSTN_CombatStepResult Result = Step->Evaluate(View);
		if (Result.Verdict == EGSTN_CombatStepVerdict::Reject)
		{
			return Result;
		}
	}
	return FGSTN_CombatStepResult();
}

#if WITH_EDITOR
EDataValidationResult UGSTN_CombatRules::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (int32 Index = 0; Index < Pipeline.Num(); ++Index)
	{
		FString Reason;
		if (!LoadStepClass(Pipeline[Index], Reason))
		{
			Context.AddError(FText::FromString(FString::Printf(TEXT("Pipeline step %d: %s. It is skipped at run time."), Index, *Reason)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
