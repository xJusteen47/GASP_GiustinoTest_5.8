// Destructible parts, ability-system half (no Network Prediction, no Mover).

#include "Projection/GSTN_PartTagProjection.h"

#include "AbilitySystemComponent.h"
#include "Data/GSTN_PartBreakEffects.h"
#include "GSTN_PartsTags.h"

void FGSTN_PartTagProjection::BuildTags(const FGSTN_PartsProjection& Projection, const FGameplayTagContainer& BaseTags, FGameplayTagContainer& Out)
{
	Out.Reset();
	for (const FGameplayTag& Tag : BaseTags)
	{
		// MatchesAny, not HasTag: a removed parent tag suppresses its children, the other way round would not
		if (!Tag.MatchesAny(Projection.Removed))
		{
			Out.AddTag(Tag);
		}
	}
	Out.AppendTags(Projection.Granted);
	Projection.AppendNativeTags(Out);
}

void FGSTN_PartTagProjection::GetUniverse(TConstArrayView<FGSTN_PartDefinition> Parts, const FGameplayTagContainer& BaseTags, FGameplayTagContainer& Out)
{
	Out.Reset();
	Out.AddTag(GSTNPartsTags::Part_Broken);
	Out.AddTag(GSTNPartsTags::Part_GrabLocked);
	Out.AddTag(GSTNPartsTags::Part_AllBroken);
	Out.AppendTags(BaseTags);
	for (const FGSTN_PartDefinition& Part : Parts)
	{
		for (const FInstancedStruct& Entry : Part.OnBreak)
		{
			if (const FGSTN_PartBreakEffect_Tags* Tags = Entry.GetPtr<FGSTN_PartBreakEffect_Tags>())
			{
				Out.AppendTags(Tags->Granted);
			}
		}
	}
}

int32 FGSTN_PartTagProjection::Apply(UAbilitySystemComponent& ASC, TConstArrayView<FGSTN_PartDefinition> Parts, const FGSTN_PartsProjection& Projection,
	const FGameplayTagContainer& BaseTags)
{
	FGameplayTagContainer Wanted;
	BuildTags(Projection, BaseTags, Wanted);
	FGameplayTagContainer Universe;
	GetUniverse(Parts, BaseTags, Universe);

	int32 Changed = 0;
	for (const FGameplayTag& Tag : Universe)
	{
		const int32 Count = Wanted.HasTagExact(Tag) ? 1 : 0;
		if (ASC.GetTagCount(Tag) != Count)
		{
			ASC.SetLooseGameplayTagCount(Tag, Count);
			++Changed;
		}
	}
	return Changed;
}
