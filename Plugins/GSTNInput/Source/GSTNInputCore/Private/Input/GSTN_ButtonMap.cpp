// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_ButtonMap.h"

FGSTN_ButtonMap::FGSTN_ButtonMap(TConstArrayView<FGameplayTag> InTags)
{
	checkf(InTags.Num() <= 32, TEXT("A button map holds at most 32 buttons, got %d"), InTags.Num());
	Num = FMath::Min(InTags.Num(), 32);
	for (int32 Index = 0; Index < Num; ++Index)
	{
		Tags[Index] = InTags[Index];
	}
}

int32 FGSTN_ButtonMap::GetIndex(const FGameplayTag& Button) const
{
	if (Button.IsValid())
	{
		for (int32 Index = 0; Index < Num; ++Index)
		{
			if (Tags[Index] == Button)
			{
				return Index;
			}
		}
	}
	return INDEX_NONE;
}

uint32 FGSTN_ButtonMap::GetMask(const FGameplayTag& Button) const
{
	const int32 Index = GetIndex(Button);
	return Index == INDEX_NONE ? 0u : (1u << Index);
}

uint32 FGSTN_ButtonMap::GetMask(const FGameplayTagContainer& Buttons) const
{
	uint32 Mask = 0;
	for (const FGameplayTag& Button : Buttons)
	{
		Mask |= GetMask(Button);
	}
	return Mask;
}
