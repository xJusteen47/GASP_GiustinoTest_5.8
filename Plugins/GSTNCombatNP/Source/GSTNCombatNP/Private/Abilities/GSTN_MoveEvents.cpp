// Combat abilities (GAS-NPP fork).

#include "Abilities/GSTN_MoveEvents.h"

#include "Timeline/GSTN_TimelineTypes.h"

void FGSTN_MoveEventRules::CollectKeys(const FGSTN_CombatTimelineData& Data, int32 LastFrame, int32 CurrentFrame, FKeyList& OutTracks)
{
	OutTracks.Reset();
	if (CurrentFrame <= LastFrame)
	{
		return;
	}
	for (int32 Index = 0; Index < Data.Tracks.Num(); ++Index)
	{
		const FGSTN_TimelineTrack* Track = Data.Tracks[Index].GetPtr<FGSTN_TimelineTrack>();
		if (Track && Track->GetKind() == EGSTN_TimelineTrackKind::Event && LastFrame < Track->StartFrame && Track->StartFrame <= CurrentFrame)
		{
			OutTracks.Add(Index);
		}
	}
}

EGSTN_MoveEndReason FGSTN_MoveEventRules::ResolveEndReason(bool bWasCancelled, bool bRecoilPlayed, int32 LastFrame, int32 TotalFrames, int32 TailStartFrame)
{
	if (bRecoilPlayed)
	{
		return EGSTN_MoveEndReason::AfterRecoil;
	}
	if (!bWasCancelled && TotalFrames > 0 && LastFrame >= TotalFrames - 1)
	{
		return EGSTN_MoveEndReason::Finished;
	}
	if (TailStartFrame != INDEX_NONE && LastFrame >= TailStartFrame)
	{
		return EGSTN_MoveEndReason::TailCancelled;
	}
	return EGSTN_MoveEndReason::Interrupted;
}
