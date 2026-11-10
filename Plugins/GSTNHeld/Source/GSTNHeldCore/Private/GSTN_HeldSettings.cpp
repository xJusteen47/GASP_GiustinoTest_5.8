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
