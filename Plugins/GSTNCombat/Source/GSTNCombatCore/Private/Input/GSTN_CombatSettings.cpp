// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_CombatSettings.h"

#include "Input/GSTN_InputSettings.h"
#include "Rules/GSTN_CombatRules.h"
#include "UObject/StrongObjectPtr.h"

UGSTN_CombatSettings::UGSTN_CombatSettings()
{
	CategoryName = TEXT("Plugins");
}

const UGSTN_CombatRules* UGSTN_CombatSettings::ResolveDefaultRules() const
{
	if (UGSTN_CombatRules* Rules = DefaultRules.LoadSynchronous())
	{
		// An asset is resolved once here so the simulation only runs the resolved lists
		if (Rules->GetResolvedHitPipeline().IsEmpty() && !Rules->HitPipeline.IsEmpty())
		{
			Rules->ResolveHitPipeline();
		}
		return Rules;
	}

	static TStrongObjectPtr<UGSTN_CombatRules> BuiltIn;
	if (!BuiltIn.IsValid())
	{
		BuiltIn.Reset(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
	}
	return BuiltIn.Get();
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

int32 UGSTN_CombatSettings::GetDefaultOverrideFrames(int32 Tier) const
{
	// The entry of the tier itself, else the one of the highest tier below it
	TOptional<int32> BestTier;
	int32 Frames = 0;
	for (const TPair<int32, int32>& Entry : OverrideFramesByTier)
	{
		if (Entry.Key <= Tier && (!BestTier.IsSet() || Entry.Key > BestTier.GetValue()))
		{
			BestTier = Entry.Key;
			Frames = Entry.Value;
		}
	}
	return FMath::Max(Frames, 0);
}
