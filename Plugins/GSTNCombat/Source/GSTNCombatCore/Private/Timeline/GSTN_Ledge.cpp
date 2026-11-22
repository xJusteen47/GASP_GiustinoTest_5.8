// Pure combat logic (no GAS, no Network Prediction).

#include "Timeline/GSTN_Ledge.h"

const FGSTN_TimelineTrack_LedgeCheck* FGSTN_LedgeRules::FindWindow(const FGSTN_CombatTimelineData& Data, int32 Frame, int32* OutIndex)
{
	for (int32 TrackIndex = 0; TrackIndex < Data.Tracks.Num(); ++TrackIndex)
	{
		const FGSTN_TimelineTrack_LedgeCheck* Track = Data.Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack_LedgeCheck>();
		if (Track && Track->Contains(Frame))
		{
			if (OutIndex)
			{
				*OutIndex = TrackIndex;
			}
			return Track;
		}
	}
	if (OutIndex)
	{
		*OutIndex = INDEX_NONE;
	}
	return nullptr;
}

bool FGSTN_LedgeRules::HasLedgeTracks(const FGSTN_CombatTimelineData& Data)
{
	return Data.Tracks.ContainsByPredicate([](const TInstancedStruct<FGSTN_TimelineTrack>& Track) { return Track.GetPtr<FGSTN_TimelineTrack_LedgeCheck>() != nullptr; });
}

bool FGSTN_LedgeRules::HasStopTracks(const FGSTN_CombatTimelineData& Data)
{
	return Data.Tracks.ContainsByPredicate([](const TInstancedStruct<FGSTN_TimelineTrack>& Track)
	{
		const FGSTN_TimelineTrack_LedgeCheck* Ledge = Track.GetPtr<FGSTN_TimelineTrack_LedgeCheck>();
		return Ledge && Ledge->Action == EGSTN_LedgeAction::Stop;
	});
}

const TSoftClassPtr<UObject>* FGSTN_LedgeRules::GetContinueMove(const FGSTN_CombatTimelineData& Data, int32 TrackIndex)
{
	const FGSTN_TimelineTrack_LedgeCheck* Ledge = Data.Tracks.IsValidIndex(TrackIndex) ? Data.Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack_LedgeCheck>() : nullptr;
	return Ledge && Ledge->Action == EGSTN_LedgeAction::ContinueWithMove && !Ledge->ContinueMove.IsNull() ? &Ledge->ContinueMove : nullptr;
}
