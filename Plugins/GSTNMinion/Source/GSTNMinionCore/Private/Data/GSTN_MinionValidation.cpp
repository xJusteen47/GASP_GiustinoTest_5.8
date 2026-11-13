// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_MinionValidation.h"

#include "Data/GSTN_MinionDefinition.h"
#include "Data/GSTN_MinionWaveDefinition.h"
#include "Hits/GSTN_HurtboxTypes.h"

namespace
{
	/** A soft reference that names something: unset is fine (the field is optional), a path that does not load is not */
	template <typename SoftPtrType>
	void CheckSoftRef(const SoftPtrType& Ref, const TCHAR* Label, TArray<FString>& OutErrors)
	{
		if (!Ref.IsNull() && !Ref.LoadSynchronous())
		{
			OutErrors.Add(FString::Printf(TEXT("%s (%s) does not load."), Label, *Ref.ToSoftObjectPath().ToString()));
		}
	}
}

void GSTNMinionValidation::CheckRegistry(TConstArrayView<const UGSTN_MinionDefinition*> Types, TArray<FString>& OutErrors)
{
	if (Types.Num() > MaxRegistryTypes)
	{
		OutErrors.Add(FString::Printf(TEXT("%d types; the registry holds at most %d (the type index is a byte)."), Types.Num(), MaxRegistryTypes));
	}
	TSet<const UGSTN_MinionDefinition*> SeenDefinitions;
	TMap<FGameplayTag, const UGSTN_MinionDefinition*> SeenTags;
	for (int32 Index = 0; Index < Types.Num(); ++Index)
	{
		const UGSTN_MinionDefinition* Type = Types[Index];
		if (!Type)
		{
			OutErrors.Add(FString::Printf(TEXT("Registry entry %d is null."), Index));
			continue;
		}
		bool bAlreadySeen = false;
		SeenDefinitions.Add(Type, &bAlreadySeen);
		if (bAlreadySeen)
		{
			OutErrors.Add(FString::Printf(TEXT("Registry entry %d repeats %s."), Index, *Type->GetName()));
			continue;
		}
		if (Type->TypeTag.IsValid())
		{
			if (const UGSTN_MinionDefinition* const* Other = SeenTags.Find(Type->TypeTag))
			{
				OutErrors.Add(FString::Printf(TEXT("Registry entry %d (%s) has the type tag %s of %s."), Index, *Type->GetName(), *Type->TypeTag.ToString(), *GetNameSafe(*Other)));
			}
			else
			{
				SeenTags.Add(Type->TypeTag, Type);
			}
		}
	}
}

void GSTNMinionValidation::CheckDefinitionReferences(const UGSTN_MinionDefinition& Definition, TArray<FString>& OutErrors, TArray<FString>& OutWarnings)
{
	CheckSoftRef(Definition.PuppetClass, TEXT("The puppet class"), OutErrors);
	CheckSoftRef(Definition.PuppetMesh, TEXT("The puppet mesh"), OutErrors);
	CheckSoftRef(Definition.PuppetAnimClass, TEXT("The puppet anim class"), OutErrors);
	CheckSoftRef(Definition.Brain, TEXT("The brain"), OutErrors);
	CheckSoftRef(Definition.PromotionFighterClass, TEXT("The promotion fighter class"), OutErrors);

	// A set that loads but holds nothing is a minion nothing can hit; the definition's own check only tests that the set loads
	if (!Definition.HurtboxSet.IsNull())
	{
		if (const UGSTN_HurtboxSetDefinition* Set = Definition.HurtboxSet.LoadSynchronous())
		{
			if (Set->Hurtboxes.IsEmpty())
			{
				OutErrors.Add(TEXT("The hurtbox set has no hurtbox: nothing can hit this minion."));
			}
		}
	}

	if (Definition.Moves.IsEmpty())
	{
		OutWarnings.Add(TEXT("No moves: a brain cannot make this minion attack."));
	}
	for (int32 Index = 0; Index < Definition.Moves.Num(); ++Index)
	{
		if (!(Definition.Moves[Index].MaxRange > 0.f))
		{
			OutWarnings.Add(FString::Printf(TEXT("Move %d has a MaxRange of 0 or less: the brain never sees a target in range."), Index));
		}
	}
	if (!(Definition.Stats.WalkSpeed > 0.f) || !(Definition.Stats.RunSpeed > 0.f))
	{
		OutWarnings.Add(TEXT("Walk or run speed is not above 0: the minion cannot move."));
	}
}

void GSTNMinionValidation::CheckWaveCrossAsset(const UGSTN_MinionWaveDefinition& Wave, int32 Capacity, TArray<FString>& OutErrors)
{
	const int32 Total = Wave.GetTotalCount();
	if (Total > Capacity)
	{
		OutErrors.Add(FString::Printf(TEXT("The wave spawns %d minions; the store holds %d (gstn.Minion.Max)."), Total, Capacity));
	}

	TArray<const UGSTN_MinionDefinition*> Distinct;
	for (int32 Index = 0; Index < Wave.Entries.Num(); ++Index)
	{
		const TSoftObjectPtr<UGSTN_MinionDefinition>& Ref = Wave.Entries[Index].Definition;
		if (Ref.IsNull())
		{
			// The wave's own IsDataValid reports an empty entry
			continue;
		}
		const UGSTN_MinionDefinition* Definition = Ref.LoadSynchronous();
		if (!Definition)
		{
			OutErrors.Add(FString::Printf(TEXT("Entry %d: the definition %s does not load."), Index, *Ref.ToSoftObjectPath().ToString()));
			continue;
		}
		// Several entries may name one type; only two different assets with one tag are a registry error
		Distinct.AddUnique(Definition);
	}
	CheckRegistry(Distinct, OutErrors);
}
