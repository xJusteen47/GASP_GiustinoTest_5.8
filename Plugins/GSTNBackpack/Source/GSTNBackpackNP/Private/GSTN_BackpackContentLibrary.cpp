// Backpacks, Network Prediction half (M11 L7c).

#include "GSTN_BackpackContentLibrary.h"

#include "GSTN_HitStep_BackpackDome.h"
#include "Hits/GSTN_HitSteps.h"
#include "Rules/GSTN_CombatRules.h"

namespace
{
	bool IsStepOf(const TSoftClassPtr<UGSTN_HitResolutionStep>& Step, const UClass* Class)
	{
		return Step.ToSoftObjectPath() == FSoftObjectPath(Class);
	}
}

bool UGSTN_BackpackContentLibrary::AddDomeStepToRules(UGSTN_CombatRules* Rules)
{
	if (!Rules)
	{
		return false;
	}
	TArray<TSoftClassPtr<UGSTN_HitResolutionStep>>& Pipeline = Rules->HitPipeline;
	if (Pipeline.ContainsByPredicate([](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step) { return IsStepOf(Step, UGSTN_HitStep_BackpackDome::StaticClass()); }))
	{
		return true;
	}
	// Right after TeamFilter: a hit the relationship refuses never reaches the dome, and the dome stops one before Parry and Shield see it
	int32 At = 0;
	for (int32 Index = 0; Index < Pipeline.Num(); ++Index)
	{
		if (IsStepOf(Pipeline[Index], UGSTN_HitStep_TeamFilter::StaticClass()))
		{
			At = Index + 1;
			break;
		}
	}
	Pipeline.Insert(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_BackpackDome::StaticClass()), At);
	Rules->ResolveHitPipeline();
	Rules->MarkPackageDirty();
	return true;
}

bool UGSTN_BackpackContentLibrary::RemoveDomeStepFromRules(UGSTN_CombatRules* Rules)
{
	if (!Rules)
	{
		return false;
	}
	const int32 Removed = Rules->HitPipeline.RemoveAll([](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step)
	{
		return IsStepOf(Step, UGSTN_HitStep_BackpackDome::StaticClass());
	});
	if (Removed > 0)
	{
		Rules->ResolveHitPipeline();
		Rules->MarkPackageDirty();
	}
	return Removed > 0;
}
