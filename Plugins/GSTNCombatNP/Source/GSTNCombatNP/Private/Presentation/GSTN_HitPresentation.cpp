// Combat simulation (GAS-NPP fork): the pure parts of the attacker's own hit presentation (combat-m4-hits 9b, M4 H10).

#include "Presentation/GSTN_HitPresentation.h"

void FGSTN_HitPresentationTracker::Update(const FGSTN_HitSyncState& Hit, bool bRunLive, int32 RunStartFrame, FDelta& OutDelta)
{
	OutDelta.Started.Reset();
	OutDelta.Cancelled.Reset();

	TArray<FGSTN_HitPresentationKey, TInlineAllocator<FGSTN_HitSyncState::MaxHitTargets>> Current;
	if (bRunLive)
	{
		for (int32 Age = 0; Age < Hit.NumHitTargets(); ++Age)
		{
			const FGSTN_HitTarget& Target = Hit.GetHitTarget(Age);
			if (Target.TimelineStartFrame == RunStartFrame)
			{
				FGSTN_HitPresentationKey& Key = Current.AddDefaulted_GetRef();
				Key.VictimId = Target.EntityId;
				Key.TimelineStartFrame = Target.TimelineStartFrame;
				Key.HitGroup = Target.HitGroup;
			}
		}
	}

	// The move ended or another began: its targets went with it, which is not a correction
	if (!bRunLive || RunStartFrame != RunStart)
	{
		Active.Reset();
	}

	for (const FGSTN_HitPresentationKey& Key : Current)
	{
		if (!Active.Contains(Key))
		{
			OutDelta.Started.Add(Key);
		}
	}
	for (const FGSTN_HitPresentationKey& Key : Active)
	{
		if (!Current.Contains(Key))
		{
			OutDelta.Cancelled.Add(Key);
		}
	}

	Active.Reset();
	Active.Append(Current);
	RunStart = bRunLive ? RunStartFrame : INDEX_NONE;
}

void FGSTN_HitPresentationTracker::Reset()
{
	Active.Reset();
	RunStart = INDEX_NONE;
}

int32 FGSTN_HitPredictionGrace::Find(const FGSTN_HitPresentationKey& Key) const
{
	return Entries.IndexOfByPredicate([&Key](const FEntry& Entry) { return Entry.Key == Key; });
}

FGSTN_HitPredictionGrace::EStart FGSTN_HitPredictionGrace::NoteStarted(const FGSTN_HitPresentationKey& Key, double NowSeconds)
{
	const int32 Index = Find(Key);
	if (Index != INDEX_NONE)
	{
		Entries[Index].CancelAtSeconds = -1.0;
		Entries[Index].SeenSeconds = NowSeconds;
		return EStart::Returned;
	}
	FEntry& Entry = Entries.AddDefaulted_GetRef();
	Entry.Key = Key;
	Entry.SeenSeconds = NowSeconds;
	return EStart::New;
}

bool FGSTN_HitPredictionGrace::NoteVanished(const FGSTN_HitPresentationKey& Key, double NowSeconds, double GraceSeconds)
{
	const int32 Index = Find(Key);
	if (Index == INDEX_NONE || Entries[Index].CancelAtSeconds >= 0.0)
	{
		return false;
	}
	Entries[Index].CancelAtSeconds = NowSeconds + GraceSeconds;
	return true;
}

void FGSTN_HitPredictionGrace::Update(double NowSeconds, double MemorySeconds, TArray<FGSTN_HitPresentationKey>& OutCancelled)
{
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		const FEntry& Entry = Entries[Index];
		if (Entry.CancelAtSeconds >= 0.0 && NowSeconds >= Entry.CancelAtSeconds)
		{
			OutCancelled.Add(Entry.Key);
			Entries.RemoveAtSwap(Index);
		}
		else if (Entry.CancelAtSeconds < 0.0 && NowSeconds - Entry.SeenSeconds > MemorySeconds)
		{
			Entries.RemoveAtSwap(Index);
		}
	}
}

void FGSTN_HitCueDedup::NoteLocal(uint32 HitId, int32 Frame)
{
	Prune(Frame);
	Played.Emplace(HitId, Frame);
}

void FGSTN_HitCueDedup::Forget(uint32 HitId)
{
	Played.RemoveAll([HitId](const TPair<uint32, int32>& Entry) { return Entry.Key == HitId; });
}

bool FGSTN_HitCueDedup::ConsumeLocal(uint32 HitId)
{
	const int32 Index = Played.IndexOfByPredicate([HitId](const TPair<uint32, int32>& Entry) { return Entry.Key == HitId; });
	if (Index == INDEX_NONE)
	{
		return false;
	}
	Played.RemoveAt(Index);
	return true;
}

void FGSTN_HitCueDedup::Prune(int32 Frame)
{
	Played.RemoveAll([Frame](const TPair<uint32, int32>& Entry) { return Frame - Entry.Value > MaxAgeFrames; });
}

uint32 GSTNHitPresentation::MakeHitId(uint32 AttackerUniqueId, uint32 VictimUniqueId, int32 Timeline, int32 HitboxIndex)
{
	return HashCombineFast(HashCombineFast(::GetTypeHash(AttackerUniqueId), ::GetTypeHash(VictimUniqueId)),
		HashCombineFast(::GetTypeHash(Timeline), ::GetTypeHash(HitboxIndex)));
}
