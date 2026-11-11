// Held objects, pure core (no GAS, no Network Prediction).

#include "GSTN_HeldSettings.h"

#include "Input/GSTN_InputSettings.h"

namespace
{
	const FName ContributionSource(TEXT("GSTNHeld"));
}

void UGSTN_HeldSettings::PushToInputSettings() const
{
	GetMutableDefault<UGSTN_InputSettings>()->SetContribution(ContributionSource, CommandTiers, TArray<FGameplayTag>());
}

void UGSTN_HeldSettings::RemoveFromInputSettings()
{
	if (UGSTN_InputSettings* InputSettings = GetMutableDefault<UGSTN_InputSettings>())
	{
		InputSettings->RemoveContribution(ContributionSource);
	}
}

#if WITH_EDITOR
void UGSTN_HeldSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (IsTemplate())
	{
		PushToInputSettings();
	}
}
#endif

const FGSTN_HeldSizeClass* UGSTN_HeldRules::FindSizeClass(const FGameplayTag& SizeTag, int32* OutIndex) const
{
	if (OutIndex)
	{
		*OutIndex = INDEX_NONE;
	}
	if (!SizeTag.IsValid())
	{
		return nullptr;
	}
	for (int32 Index = 0; Index < FMath::Min(SizeClasses.Num(), static_cast<int32>(FGSTN_HeldLoad::MaxSizeClasses)); ++Index)
	{
		if (SizeClasses[Index].SizeTag == SizeTag)
		{
			if (OutIndex)
			{
				*OutIndex = Index;
			}
			return &SizeClasses[Index];
		}
	}
	return nullptr;
}
