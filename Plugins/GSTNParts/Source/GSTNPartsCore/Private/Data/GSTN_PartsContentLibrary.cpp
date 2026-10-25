// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_PartsContentLibrary.h"

#include "Data/GSTN_PartBreakEffects.h"
#include "Data/GSTN_PartSetDefinition.h"
#include "Hits/GSTN_HitSteps.h"
#include "Pipeline/GSTN_HitStep_PartResolve.h"
#include "Rules/GSTN_CombatRules.h"

namespace
{
	/** Replaces the effect of type EffectT on a part, or adds it */
	template <typename EffectT>
	void SetEffect(FGSTN_PartDefinition& Part, const EffectT& Effect)
	{
		for (FInstancedStruct& Entry : Part.OnBreak)
		{
			if (Entry.GetPtr<EffectT>())
			{
				Entry = FInstancedStruct::Make(Effect);
				return;
			}
		}
		Part.OnBreak.Add(FInstancedStruct::Make(Effect));
	}

	bool IsStep(const TSoftClassPtr<UGSTN_HitResolutionStep>& Step, const UClass* Class)
	{
		return Step.ToSoftObjectPath() == FSoftObjectPath(Class);
	}
}

bool UGSTN_PartsContentLibrary::ConvertRulesToParts(UGSTN_CombatRules* Rules)
{
	if (!Rules)
	{
		return false;
	}
	TArray<TSoftClassPtr<UGSTN_HitResolutionStep>>& Pipeline = Rules->HitPipeline;
	for (const TSoftClassPtr<UGSTN_HitResolutionStep>& Step : Pipeline)
	{
		if (IsStep(Step, UGSTN_HitStep_PartResolve::StaticClass()))
		{
			return true;
		}
	}
	Pipeline.RemoveAll([](const TSoftClassPtr<UGSTN_HitResolutionStep>& Step)
	{
		return IsStep(Step, UGSTN_HitStep_HealthWeight::StaticClass()) || IsStep(Step, UGSTN_HitStep_ApplyDamage::StaticClass());
	});
	int32 At = Pipeline.Num();
	for (int32 Index = 0; Index < Pipeline.Num(); ++Index)
	{
		if (IsStep(Pipeline[Index], UGSTN_HitStep_ComputeKnockback::StaticClass()))
		{
			At = Index;
			break;
		}
	}
	Pipeline.Insert(TSoftClassPtr<UGSTN_HitResolutionStep>(UGSTN_HitStep_PartResolve::StaticClass()), At);
	Rules->ResolveHitPipeline();
	Rules->MarkPackageDirty();
	return true;
}

bool UGSTN_PartsContentLibrary::SetBreakDisableHitGroups(UGSTN_PartSetDefinition* Set, int32 PartIndex, int32 GroupMask)
{
	if (!Set || !Set->Parts.IsValidIndex(PartIndex))
	{
		return false;
	}
	FGSTN_PartBreakEffect_DisableHitGroups Effect;
	Effect.GroupMask = static_cast<uint8>(GroupMask & 0xFF);
	SetEffect(Set->Parts[PartIndex], Effect);
	Set->MarkPackageDirty();
	return true;
}

bool UGSTN_PartsContentLibrary::SetBreakMainDamage(UGSTN_PartSetDefinition* Set, int32 PartIndex, float AddPercent, bool bKill)
{
	if (!Set || !Set->Parts.IsValidIndex(PartIndex))
	{
		return false;
	}
	FGSTN_PartBreakEffect_MainDamage Effect;
	Effect.AddPercent = AddPercent;
	Effect.bKill = bKill;
	SetEffect(Set->Parts[PartIndex], Effect);
	Set->MarkPackageDirty();
	return true;
}
