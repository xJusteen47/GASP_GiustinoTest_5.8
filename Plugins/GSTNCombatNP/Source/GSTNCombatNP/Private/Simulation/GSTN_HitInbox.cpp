// Combat simulation (GAS-NPP fork).

#include "Simulation/GSTN_HitInbox.h"

#include "Algo/StableSort.h"

bool FGSTN_HitInbox::Add(const FGSTN_HitInboxEntry& Entry)
{
	if (Entries.Num() >= MaxEntries)
	{
		return false;
	}
	Entries.Add(Entry);
	return true;
}

void FGSTN_HitInbox::Drain(int32 Frame, TArray<FGSTN_HitInboxEntry>& Out)
{
	const int32 First = Out.Num();
	for (int32 Index = 0; Index < Entries.Num();)
	{
		if (Entries[Index].Frame < Frame)
		{
			Out.Add(MoveTemp(Entries[Index]));
			Entries.RemoveAt(Index, EAllowShrinking::No);
		}
		else
		{
			++Index;
		}
	}

	// Stable on the arrival order for entries that tie on all three keys, which is the same on every machine (one attacker, one frame)
	Algo::StableSort(MakeArrayView(Out.GetData() + First, Out.Num() - First), [](const FGSTN_HitInboxEntry& A, const FGSTN_HitInboxEntry& B)
	{
		if (A.AttackerId != B.AttackerId)
		{
			return A.AttackerId < B.AttackerId;
		}
		if (A.AttackerHitboxIndex != B.AttackerHitboxIndex)
		{
			return A.AttackerHitboxIndex < B.AttackerHitboxIndex;
		}
		return A.HurtboxIndex < B.HurtboxIndex;
	});
}

void FGSTN_HitInbox::TrimFrom(int32 FirstResimulatedFrame)
{
	Entries.RemoveAll([FirstResimulatedFrame](const FGSTN_HitInboxEntry& Entry) { return Entry.Frame >= FirstResimulatedFrame; });
}
