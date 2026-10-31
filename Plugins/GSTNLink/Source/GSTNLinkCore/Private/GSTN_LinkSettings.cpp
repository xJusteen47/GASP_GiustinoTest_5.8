// Links, pure part (no GAS, no Network Prediction, no Mover).

#include "GSTN_LinkSettings.h"

#include "Input/GSTN_InputSettings.h"

namespace
{
	const FName ContributionSource(TEXT("GSTNLink"));
}

void UGSTN_LinkSettings::PushToInputSettings() const
{
	GetMutableDefault<UGSTN_InputSettings>()->SetContribution(ContributionSource, CommandTiers, TArray<FGameplayTag>());
}

void UGSTN_LinkSettings::RemoveFromInputSettings()
{
	if (UGSTN_InputSettings* InputSettings = GetMutableDefault<UGSTN_InputSettings>())
	{
		InputSettings->RemoveContribution(ContributionSource);
	}
}

#if WITH_EDITOR
void UGSTN_LinkSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (IsTemplate())
	{
		PushToInputSettings();
	}
}
#endif
