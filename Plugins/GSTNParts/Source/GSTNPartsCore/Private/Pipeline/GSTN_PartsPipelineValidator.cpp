// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#include "Pipeline/GSTN_PartsPipelineValidator.h"

#include "Data/GSTN_PartSetDefinition.h"
#include "Rules/GSTN_CombatRules.h"
#include "Rules/GSTN_CombatRulesValidation.h"

namespace
{
	FName ShortStepName(const TSoftClassPtr<UGSTN_HitResolutionStep>& Entry)
	{
		FString Text = FGSTN_CombatRulesValidation::GetStepName(Entry).ToString();
		Text.RemoveFromStart(TEXT("GSTN_HitStep_"));
		return FName(*Text);
	}

	void Add(TArray<FGSTN_ReactionMessage>& Out, bool bError, const FString& Text)
	{
		FGSTN_ReactionMessage& Message = Out.AddDefaulted_GetRef();
		Message.bError = bError;
		Message.Text = Text;
	}

	int32 FindStep(const UGSTN_CombatRules& Rules, const TCHAR* ShortName)
	{
		for (int32 Index = 0; Index < Rules.HitPipeline.Num(); ++Index)
		{
			if (!Rules.HitPipeline[Index].IsNull() && ShortStepName(Rules.HitPipeline[Index]) == ShortName)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}
}

bool FGSTN_PartsPipelineValidator::HasPartResolve(const UGSTN_CombatRules& Rules)
{
	return FindStep(Rules, TEXT("PartResolve")) != INDEX_NONE;
}

void FGSTN_PartsPipelineValidator::Validate(const UGSTN_CombatRules& Rules, TArray<FGSTN_ReactionMessage>& OutMessages)
{
	const int32 PartResolveIndex = FindStep(Rules, TEXT("PartResolve"));
	if (PartResolveIndex == INDEX_NONE)
	{
		return;
	}

	for (const TCHAR* Blocker : { TEXT("Parry"), TEXT("Shield") })
	{
		const int32 BlockerIndex = FindStep(Rules, Blocker);
		if (BlockerIndex != INDEX_NONE && BlockerIndex > PartResolveIndex)
		{
			Add(OutMessages, true, FString::Printf(TEXT("Hit step %d (PartResolve) runs before %s (step %d): a blocked or parried hit would damage a part"), PartResolveIndex, Blocker, BlockerIndex));
		}
	}

	const int32 ApplyDamageIndex = FindStep(Rules, TEXT("ApplyDamage"));
	if (ApplyDamageIndex != INDEX_NONE)
	{
		Add(OutMessages, true, FString::Printf(TEXT("Hit step %d (ApplyDamage) is in the pipeline next to PartResolve: PartResolve already requests the damage (the part's share), so the hit would be applied twice. Remove ApplyDamage"), ApplyDamageIndex));
	}
}

void FGSTN_PartsPipelineValidator::ValidateKit(const UGSTN_PartSetDefinition& PartSet, const UGSTN_CombatRules& Rules, TArray<FGSTN_ReactionMessage>& OutMessages)
{
	if (!HasPartResolve(Rules))
	{
		Add(OutMessages, true, FString::Printf(TEXT("The part set %s is used under rules whose hit pipeline has no PartResolve step: no part would ever take a hit"), *PartSet.GetName()));
	}
}
