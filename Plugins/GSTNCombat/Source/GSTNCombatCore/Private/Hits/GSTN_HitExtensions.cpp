// Pure combat logic (no GAS, no Network Prediction).

#include "Hits/GSTN_HitExtensions.h"

#include "Misc/Crc.h"
#include "Rules/GSTN_CombatRules.h"

FGSTN_OutcomeApplyResult UGSTN_OutcomeApplier::Apply_Implementation(const FGSTN_HitContextView& View, const FInstancedStruct& Extension) const
{
	return FGSTN_OutcomeApplyResult();
}

EGSTN_AttackerHitOutcome UGSTN_AttackerOutcome::Evaluate_Implementation(const FGSTN_HitContextView& View) const
{
	return EGSTN_AttackerHitOutcome::Normal;
}

FGSTN_OutcomeApplierRegistry& FGSTN_OutcomeApplierRegistry::Get()
{
	static FGSTN_OutcomeApplierRegistry Registry;
	return Registry;
}

bool FGSTN_OutcomeApplierRegistry::Register(IGSTN_OutcomeExtensionApplier* Applier)
{
	const UScriptStruct* Struct = Applier ? Applier->GetExtensionStruct() : nullptr;
	if (!Struct)
	{
		return false;
	}
	for (const FEntry& Entry : Entries)
	{
		if (Entry.Applier == Applier)
		{
			return false;
		}
	}

	FEntry NewEntry;
	NewEntry.Path = Struct->GetPathName();
	NewEntry.Hash = FCrc::StrCrc32(*NewEntry.Path);
	NewEntry.Applier = Applier;

	// Sorted by the hash of the type name, ties by the name, then in the order of registration: the same on every machine
	int32 At = 0;
	while (At < Entries.Num() && (Entries[At].Hash < NewEntry.Hash || (Entries[At].Hash == NewEntry.Hash && Entries[At].Path.Compare(NewEntry.Path) <= 0)))
	{
		++At;
	}
	Entries.Insert(NewEntry, At);
	return true;
}

void FGSTN_OutcomeApplierRegistry::Unregister(IGSTN_OutcomeExtensionApplier* Applier)
{
	Entries.RemoveAll([Applier](const FEntry& Entry) { return Entry.Applier == Applier; });
}

void FGSTN_OutcomeApplierRegistry::ApplyAll(const UGSTN_CombatRules* Rules, const TArray<FInstancedStruct>& Extensions, const FGSTN_HitContextView& View, FGSTN_OutcomeApplyContext& Context) const
{
	if (Extensions.IsEmpty())
	{
		return;
	}

	for (const FEntry& Entry : Entries)
	{
		const UScriptStruct* Struct = Entry.Applier->GetExtensionStruct();
		for (const FInstancedStruct& Extension : Extensions)
		{
			if (Extension.IsValid() && Extension.GetScriptStruct() == Struct)
			{
				Entry.Applier->Apply(Extension, View, Context);
			}
		}
	}

	if (!Rules)
	{
		return;
	}
	for (const UGSTN_OutcomeApplier* Applier : Rules->OutcomeAppliers)
	{
		if (!Applier || !Applier->ExtensionStruct)
		{
			continue;
		}
		for (const FInstancedStruct& Extension : Extensions)
		{
			if (Extension.IsValid() && Extension.GetScriptStruct() == Applier->ExtensionStruct)
			{
				const FGSTN_OutcomeApplyResult Result = Applier->Apply(View, Extension);
				Context.Result.Events.Append(Result.Events);
				Context.Result.Effects.Append(Result.Effects);
			}
		}
	}
}

FGSTN_AttackerOutcomeRegistry& FGSTN_AttackerOutcomeRegistry::Get()
{
	static FGSTN_AttackerOutcomeRegistry Registry;
	return Registry;
}

bool FGSTN_AttackerOutcomeRegistry::Register(IGSTN_AttackerOutcomeExtension* Extension)
{
	if (!Extension)
	{
		return false;
	}
	for (const FEntry& Entry : Entries)
	{
		if (Entry.Extension == Extension)
		{
			return false;
		}
	}

	FEntry NewEntry;
	NewEntry.Name = Extension->GetName().ToString();
	NewEntry.Hash = FCrc::StrCrc32(*NewEntry.Name);
	NewEntry.Extension = Extension;

	int32 At = 0;
	while (At < Entries.Num() && (Entries[At].Hash < NewEntry.Hash || (Entries[At].Hash == NewEntry.Hash && Entries[At].Name.Compare(NewEntry.Name) <= 0)))
	{
		++At;
	}
	Entries.Insert(NewEntry, At);
	return true;
}

void FGSTN_AttackerOutcomeRegistry::Unregister(IGSTN_AttackerOutcomeExtension* Extension)
{
	Entries.RemoveAll([Extension](const FEntry& Entry) { return Entry.Extension == Extension; });
}

EGSTN_AttackerHitOutcome FGSTN_AttackerOutcomeRegistry::Evaluate(const UGSTN_CombatRules* Rules, const FGSTN_HitContextView& View) const
{
	for (const FEntry& Entry : Entries)
	{
		const EGSTN_AttackerHitOutcome Outcome = Entry.Extension->Evaluate(View);
		if (Outcome != EGSTN_AttackerHitOutcome::Normal)
		{
			return Outcome;
		}
	}
	if (Rules)
	{
		for (const UGSTN_AttackerOutcome* Extension : Rules->AttackerOutcomes)
		{
			if (Extension)
			{
				const EGSTN_AttackerHitOutcome Outcome = Extension->Evaluate(View);
				if (Outcome != EGSTN_AttackerHitOutcome::Normal)
				{
					return Outcome;
				}
			}
		}
	}
	return EGSTN_AttackerHitOutcome::Normal;
}
