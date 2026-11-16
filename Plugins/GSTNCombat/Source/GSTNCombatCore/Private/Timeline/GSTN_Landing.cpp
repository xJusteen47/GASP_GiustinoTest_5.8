// Pure combat logic (no GAS, no Network Prediction).

#include "Timeline/GSTN_Landing.h"

const FGSTN_TimelineTrack_Landing* FGSTN_LandingRules::FindWindow(const FGSTN_CombatTimelineData& Data, int32 Frame, int32* OutIndex)
{
	for (int32 TrackIndex = 0; TrackIndex < Data.Tracks.Num(); ++TrackIndex)
	{
		const FGSTN_TimelineTrack_Landing* Track = Data.Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack_Landing>();
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

bool FGSTN_LandingRules::HasLandingTracks(const FGSTN_CombatTimelineData& Data)
{
	return Data.Tracks.ContainsByPredicate([](const TInstancedStruct<FGSTN_TimelineTrack>& Track) { return Track.GetPtr<FGSTN_TimelineTrack_Landing>() != nullptr; });
}

FGSTN_GroundClockDecision FGSTN_LandingRules::Resolve(const FGSTN_CombatTimelineData& Data, int32 Frame, bool bOnGround, bool bFirstTick, bool bAirborneOk)
{
	FGSTN_GroundClockDecision Decision;
	Decision.bAirborneOk = bAirborneOk;
	const bool bGroundMove = Data.GroundRule == EGSTN_MoveGroundRule::Ground;
	if (!bOnGround)
	{
		if (bGroundMove && !bAirborneOk)
		{
			// The take-off window, or a coyote start (in the air on the first tick of a move that may take off)
			if (Data.IsTakeoffFrame(Frame) || (bFirstTick && Data.AllowsTakeoff()))
			{
				Decision.bAirborneOk = true;
			}
			else
			{
				Decision.Action = EGSTN_GroundClockAction::CancelAirborne;
			}
		}
		return Decision;
	}

	// On the ground: a Ground move counts a landing only after it took off; the bit closes, so walking off a ledge later cancels again
	const bool bCanLand = !bGroundMove || bAirborneOk;
	Decision.bAirborneOk = false;
	int32 TrackIndex = INDEX_NONE;
	if (bCanLand && FindWindow(Data, Frame, &TrackIndex))
	{
		Decision.Action = EGSTN_GroundClockAction::Land;
		Decision.TrackIndex = TrackIndex;
	}
	return Decision;
}
