#include "GSTN_ExperienceDefinition.h"
#include "GSTN_ExperienceActionSet.h"
#include "GSTN_PawnData.h"
#include "GameFeatureAction.h"

FPrimaryAssetId UGSTN_ExperienceDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GetAssetType(), GetFName());
}

void UGSTN_ExperienceDefinition::CollectGameFeatureNames(TArray<FString>& OutNames) const
{
	const auto AddNames = [&OutNames](const TArray<FString>& Names)
	{
		for (const FString& Name : Names)
		{
			if (!Name.IsEmpty())
			{
				OutNames.AddUnique(Name);
			}
		}
	};

	AddNames(GameFeaturesToEnable);
	for (const UGSTN_ExperienceActionSet* Set : ActionSets)
	{
		if (Set)
		{
			AddNames(Set->GameFeaturesToEnable);
		}
	}
}

void UGSTN_ExperienceDefinition::CollectActions(TArray<UGameFeatureAction*>& OutActions) const
{
	for (UGameFeatureAction* Action : Actions)
	{
		if (Action)
		{
			OutActions.Add(Action);
		}
	}
	for (const UGSTN_ExperienceActionSet* Set : ActionSets)
	{
		if (!Set)
		{
			continue;
		}
		for (UGameFeatureAction* Action : Set->Actions)
		{
			if (Action)
			{
				OutActions.Add(Action);
			}
		}
	}
}

void UGSTN_ExperienceDefinition::CollectPrimaryAssetIds(TArray<FPrimaryAssetId>& OutIds) const
{
	OutIds.AddUnique(GetPrimaryAssetId());
	if (DefaultPawnData)
	{
		OutIds.AddUnique(DefaultPawnData->GetPrimaryAssetId());
	}
	for (const UGSTN_ExperienceActionSet* Set : ActionSets)
	{
		if (Set)
		{
			OutIds.AddUnique(Set->GetPrimaryAssetId());
		}
	}
}
