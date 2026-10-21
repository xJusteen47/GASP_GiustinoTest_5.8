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
	}
	return 0;
}
