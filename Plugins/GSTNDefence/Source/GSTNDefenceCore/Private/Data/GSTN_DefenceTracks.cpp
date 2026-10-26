// Defence, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_DefenceTracks.h"

#include "Data/GSTN_DefenceRules.h"

void FGSTN_HoldWindowTrack::Validate(const FString& Label, TArray<FString>& OutErrors) const
{
	if (EndFrame <= StartFrame)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the window [%d, %d) is empty or inverted."), *Label, StartFrame, EndFrame));
	}
	if (!HoldButton.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("%s: no hold button is set, so the window could never stall the timeline."), *Label));
	}
}

bool FGSTN_ParryWindowTrack::Bake(const UGSTN_DefenceRules& Rules)
{
	ProfileIndex = Rules.FindProfileIndex(ProfileName);
	return ProfileIndex != INDEX_NONE;
}

bool FGSTN_ParryWindowTrack::Affects(FName HurtboxName, const FGameplayTagContainer& HurtboxTags) const
{
	if (AffectedHurtboxes.IsEmpty() && AffectedTags.IsEmpty())
	{
		return true;
	}
	if (AffectedHurtboxes.Contains(HurtboxName))
	{
		return true;
	}
	return !AffectedTags.IsEmpty() && HurtboxTags.HasAny(AffectedTags);
}

void FGSTN_ParryWindowTrack::Validate(const FString& Label, TArray<FString>& OutErrors) const
{
	if (EndFrame <= StartFrame)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the window [%d, %d) is empty or inverted."), *Label, StartFrame, EndFrame));
	}
	if (ProfileIndex == INDEX_NONE)
	{
		OutErrors.Add(FString::Printf(TEXT("%s: the profile name '%s' is not baked to a profile of the rules (unknown name, or not baked yet)."), *Label, *ProfileName.ToString()));
	}
	for (const FName& Name : AffectedHurtboxes)
	{
		if (Name.IsNone())
		{
			OutErrors.Add(FString::Printf(TEXT("%s lists an empty hurtbox name."), *Label));
		}
	}
}
