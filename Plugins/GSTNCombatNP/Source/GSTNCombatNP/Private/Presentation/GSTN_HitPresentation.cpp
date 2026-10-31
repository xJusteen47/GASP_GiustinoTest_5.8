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
