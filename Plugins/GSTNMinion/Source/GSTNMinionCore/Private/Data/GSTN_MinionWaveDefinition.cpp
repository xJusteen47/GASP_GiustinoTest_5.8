// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#include "Data/GSTN_MinionWaveDefinition.h"

#include "Data/GSTN_MinionDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

int32 UGSTN_MinionWaveDefinition::GetTotalCount() const
{
	int32 Total = 0;
	for (const FGSTN_MinionWaveEntry& Entry : Entries)
	{
		Total += FMath::Max(Entry.Count, 0);
	}
	return Total;
}

void UGSTN_MinionWaveDefinition::GetDefinitionPaths(TArray<FSoftObjectPath>& Out) const
{
	for (const FGSTN_MinionWaveEntry& Entry : Entries)
	{
		if (!Entry.Definition.IsNull())
		{
			Out.AddUnique(Entry.Definition.ToSoftObjectPath());
		}
	}
}

void UGSTN_MinionWaveDefinition::GetAssetsToPreload(TArray<FSoftObjectPath>& Out, bool bForClient) const
{
	GetDefinitionPaths(Out);
	for (const FGSTN_MinionWaveEntry& Entry : Entries)
	{
		// Get(), not LoadSynchronous: a definition that is not in memory yet contributes its own path only
		if (const UGSTN_MinionDefinition* Definition = Entry.Definition.Get())
		{
			Definition->GetSoftDependencies(Out, !bForClient);
		}
	}
}

#if WITH_EDITOR

EDataValidationResult UGSTN_MinionWaveDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	const auto Fail = [&Context, &Result](const FString& Message)
	{
		Context.AddError(FText::FromString(Message));
		Result = EDataValidationResult::Invalid;
	};

	if (Entries.IsEmpty())
	{
		Fail(TEXT("The wave has no entries."));
	}
	int32 Total = 0;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FGSTN_MinionWaveEntry& Entry = Entries[Index];
		if (Entry.Definition.IsNull())
		{
			Fail(FString::Printf(TEXT("Entry %d has no minion definition."), Index));
		}
		if (Entry.Count < 1 || Entry.Count > GSTNMinion::MaxWaveEntryCount)
		{
			Fail(FString::Printf(TEXT("Entry %d spawns %d minions; the count must be 1 to %d."), Index, Entry.Count, GSTNMinion::MaxWaveEntryCount));
		}
		if (Entry.DelayFrames < 0)
		{
			Fail(FString::Printf(TEXT("Entry %d has a negative delay."), Index));
		}
		Total += FMath::Max(Entry.Count, 0);
	}
	if (Total > GSTNMinion::MaxWaveTotal)
	{
		Fail(FString::Printf(TEXT("The wave spawns %d minions in total; at most %d."), Total, GSTNMinion::MaxWaveTotal));
	}
	if (Result != EDataValidationResult::Invalid)
	{
		Result = EDataValidationResult::Valid;
	}
	return Result;
}

#endif
