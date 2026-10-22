// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_CombatSettings.h"

#include "Input/GSTN_InputSettings.h"

UGSTN_CombatSettings::UGSTN_CombatSettings()
{
	CategoryName = TEXT("Plugins");
}

FName UGSTN_CombatSettings::GetContributionSource()
{
	return TEXT("GSTNCombat");
}

void UGSTN_CombatSettings::PushToInputSettings() const
{
	GetMutableDefault<UGSTN_InputSettings>()->SetContribution(GetContributionSource(), CommandTiers, ExclusiveButtons);
}

void UGSTN_CombatSettings::RemoveFromInputSettings()
{
	if (UGSTN_InputSettings* InputSettings = GetMutableDefault<UGSTN_InputSettings>())
	{
		InputSettings->RemoveContribution(GetContributionSource());
	}
}

#if WITH_EDITOR
void UGSTN_CombatSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (IsTemplate())
	{
		PushToInputSettings();
	}
}
#endif

void UGSTN_CombatSettings::PostReloadConfig(FProperty* PropertyThatWasLoaded)
{
	Super::PostReloadConfig(PropertyThatWasLoaded);
	if (IsTemplate())
	{
		PushToInputSettings();
	}
}
