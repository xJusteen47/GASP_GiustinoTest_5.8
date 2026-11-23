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

double FGSTN_LandingRules::FallThisFrame(double UpSpeed, double GravityAccel, double StepSeconds)
{
	if (UpSpeed >= 0.0 || StepSeconds <= 0.0)
	{
		return 0.0;
	}
	return -UpSpeed * StepSeconds + 0.5 * FMath::Max(GravityAccel, 0.0) * StepSeconds * StepSeconds;
}

bool FGSTN_LandingRules::ReachesFloorThisFrame(double UpSpeed, double GravityAccel, double StepSeconds, double FloorDistance, double MarginCm)
{
	const double Fall = FallThisFrame(UpSpeed, GravityAccel, StepSeconds);
	return Fall > 0.0 && FloorDistance >= -MarginCm && FloorDistance <= Fall + FMath::Max(MarginCm, 0.0);
}
