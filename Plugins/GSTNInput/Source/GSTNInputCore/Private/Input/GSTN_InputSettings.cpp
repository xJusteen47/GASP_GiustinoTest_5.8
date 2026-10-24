// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputSettings.h"

UGSTN_InputSettings::UGSTN_InputSettings()
{
	CategoryName = TEXT("Plugins");
	PriorityCriteria = MakeDefaultPriorityCriteria();
}

TArray<TInstancedStruct<FGSTN_PriorityCriterion>> UGSTN_InputSettings::MakeDefaultPriorityCriteria()
{
	TArray<TInstancedStruct<FGSTN_PriorityCriterion>> Criteria;
	Criteria.Add(TInstancedStruct<FGSTN_PriorityCriterion>::Make<FGSTN_PriorityCriterion_Tier>());
	Criteria.Add(TInstancedStruct<FGSTN_PriorityCriterion>::Make<FGSTN_PriorityCriterion_Specificity>());
	Criteria.Add(TInstancedStruct<FGSTN_PriorityCriterion>::Make<FGSTN_PriorityCriterion_ManualOffset>());
	return Criteria;
}

int32 UGSTN_InputSettings::GetCommandTier(const FGameplayTag& Command) const
{
	for (FGameplayTag Tag = Command; Tag.IsValid(); Tag = Tag.RequestDirectParent())
	{
		if (const int32* Tier = CommandTiers.Find(Tag))
		{
			return *Tier;
		}
		// Contributions are few and their order does not matter unless two plugins tier the same tag, which is a mistake
		// the highest tier resolves the same way everywhere
		int32 Best = 0;
		bool bFound = false;
		for (const TPair<FName, FContribution>& Pair : Contributions)
		{
			if (const int32* Tier = Pair.Value.Tiers.Find(Tag))
			{
				Best = bFound ? FMath::Max(Best, *Tier) : *Tier;
				bFound = true;
			}
		}
		if (bFound)
		{
			return Best;
		}
	}
	return 0;
}

bool UGSTN_InputSettings::IsExclusiveButton(const FGameplayTag& Button) const
{
	if (ExclusiveButtons.Contains(Button))
	{
		return true;
	}
	for (const TPair<FName, FContribution>& Pair : Contributions)
	{
		if (Pair.Value.ExclusiveButtons.Contains(Button))
		{
			return true;
		}
	}
	return false;
}

void UGSTN_InputSettings::SetContribution(FName Source, const TMap<FGameplayTag, int32>& Tiers, const TArray<FGameplayTag>& InExclusiveButtons, const TArray<FGSTN_ExclusiveTagGroup>& InExclusiveGroups)
{
	FContribution& Contribution = Contributions.FindOrAdd(Source);
	Contribution.Tiers = Tiers;
	Contribution.ExclusiveButtons = InExclusiveButtons;
	Contribution.ExclusiveGroups = InExclusiveGroups;
}

TArray<FGSTN_ExclusiveTagGroup> UGSTN_InputSettings::GetAllExclusiveContextGroups() const
{
	TArray<FGSTN_ExclusiveTagGroup> Result = ExclusiveContextGroups;
	for (const TPair<FName, FContribution>& Pair : Contributions)
	{
		Result.Append(Pair.Value.ExclusiveGroups);
	}
	return Result;
}

void UGSTN_InputSettings::RemoveContribution(FName Source)
{
	Contributions.Remove(Source);
}
