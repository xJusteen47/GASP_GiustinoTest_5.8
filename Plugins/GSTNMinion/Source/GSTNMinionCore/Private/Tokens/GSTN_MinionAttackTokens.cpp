// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#include "Tokens/GSTN_MinionAttackTokens.h"

void FGSTN_MinionAttackTokens::SetMaxPerTarget(int32 InMaxPerTarget)
{
	MaxPerTarget = FMath::Max(InMaxPerTarget, 1);
}

int32 FGSTN_MinionAttackTokens::FindEntry(int32 TargetId, int32 SlotIndex) const
{
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (Entries[Index].TargetId == TargetId && Entries[Index].SlotIndex == SlotIndex)
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void FGSTN_MinionAttackTokens::Request(int32 TargetId, int32 SlotIndex)
{
	if (FindEntry(TargetId, SlotIndex) != INDEX_NONE)
	{
		return;
	}

	FEntry NewEntry;
	NewEntry.TargetId = TargetId;
	NewEntry.SlotIndex = SlotIndex;

	// Insert at the sorted position so Resolve walks the entries in grant order
	int32 InsertAt = 0;
	while (InsertAt < Entries.Num()
		&& (Entries[InsertAt].TargetId < TargetId || (Entries[InsertAt].TargetId == TargetId && Entries[InsertAt].SlotIndex < SlotIndex)))
	{
		++InsertAt;
	}
	Entries.Insert(NewEntry, InsertAt);
}

int32 FGSTN_MinionAttackTokens::Resolve()
{
	int32 Granted = 0;
	int32 RunStart = 0;
	while (RunStart < Entries.Num())
	{
		// One run = all entries of one target (contiguous because the array is sorted)
		int32 RunEnd = RunStart;
		int32 Holders = 0;
		while (RunEnd < Entries.Num() && Entries[RunEnd].TargetId == Entries[RunStart].TargetId)
		{
			Holders += Entries[RunEnd].bGranted ? 1 : 0;
			++RunEnd;
		}

		for (int32 Index = RunStart; Index < RunEnd && Holders < MaxPerTarget; ++Index)
		{
			if (!Entries[Index].bGranted)
			{
				Entries[Index].bGranted = true;
				++Holders;
				++Granted;
			}
		}
		RunStart = RunEnd;
	}
	return Granted;
}

bool FGSTN_MinionAttackTokens::Holds(int32 TargetId, int32 SlotIndex) const
{
	const int32 Index = FindEntry(TargetId, SlotIndex);
	return Index != INDEX_NONE && Entries[Index].bGranted;
}

bool FGSTN_MinionAttackTokens::HoldsAny(int32 SlotIndex) const
{
	for (const FEntry& Entry : Entries)
	{
		if (Entry.bGranted && Entry.SlotIndex == SlotIndex)
		{
			return true;
		}
	}
	return false;
}

bool FGSTN_MinionAttackTokens::IsWaiting(int32 TargetId, int32 SlotIndex) const
{
	const int32 Index = FindEntry(TargetId, SlotIndex);
	return Index != INDEX_NONE && !Entries[Index].bGranted;
}

bool FGSTN_MinionAttackTokens::Release(int32 TargetId, int32 SlotIndex)
{
	const int32 Index = FindEntry(TargetId, SlotIndex);
	if (Index == INDEX_NONE)
	{
		return false;
	}
	Entries.RemoveAt(Index);
	return true;
}

int32 FGSTN_MinionAttackTokens::ReleaseSlot(int32 SlotIndex)
{
	return Entries.RemoveAll([SlotIndex](const FEntry& Entry) { return Entry.SlotIndex == SlotIndex; });
}

int32 FGSTN_MinionAttackTokens::ReleaseTarget(int32 TargetId)
{
	return Entries.RemoveAll([TargetId](const FEntry& Entry) { return Entry.TargetId == TargetId; });
}

int32 FGSTN_MinionAttackTokens::NumHolders(int32 TargetId) const
{
	int32 Count = 0;
	for (const FEntry& Entry : Entries)
	{
		Count += (Entry.TargetId == TargetId && Entry.bGranted) ? 1 : 0;
	}
	return Count;
}

int32 FGSTN_MinionAttackTokens::NumWaiting(int32 TargetId) const
{
	int32 Count = 0;
	for (const FEntry& Entry : Entries)
	{
		Count += (Entry.TargetId == TargetId && !Entry.bGranted) ? 1 : 0;
	}
	return Count;
}
