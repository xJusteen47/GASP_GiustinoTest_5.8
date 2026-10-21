// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputSettings.h"

UGSTN_InputSettings::UGSTN_InputSettings()
{
	CategoryName = TEXT("Plugins");
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
