// Pure combat logic (no GAS, no Network Prediction).

#include "Timeline/GSTN_HoldUntilLanded.h"

#include "Animation/AnimMontage.h"

namespace
{
	/** A tenth of a millisecond of montage time: under any animation frame, above float noise on times of a few seconds */
	constexpr float MontageTolerance = 1.e-4f;
}

const FGSTN_TimelineTrack_HoldUntilLanded* FGSTN_HoldUntilLanded::FindTrack(const FGSTN_CombatTimelineData& Data, int32* OutIndex)
{
	for (int32 TrackIndex = 0; TrackIndex < Data.Tracks.Num(); ++TrackIndex)
	{
		if (const FGSTN_TimelineTrack_HoldUntilLanded* Track = Data.Tracks[TrackIndex].GetPtr<FGSTN_TimelineTrack_HoldUntilLanded>())
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

FGSTN_HoldClockDecision FGSTN_HoldUntilLanded::ResolveClock(const FGSTN_CombatTimelineData& Data, int32 CurrentFrame, bool bGroundKnown, bool bOnGround,
	int32 ElapsedFrames)
{
	FGSTN_HoldClockDecision Decision;
	int32 TrackIndex = INDEX_NONE;
	const FGSTN_TimelineTrack_HoldUntilLanded* Track = bGroundKnown ? FindTrack(Data, &TrackIndex) : nullptr;
	if (!Track || Track->EndFrame <= Track->StartFrame)
	{
		return Decision;
	}
	const int32 Next = CurrentFrame + 1;
	const int32 Land = Track->ResolveLandFrame();
	if (bOnGround)
	{
		// Still in the air part (before or inside the hold) and the land frame is not the next one anyway: the land part starts now
		if (Next >= Track->LandFromFrame && CurrentFrame < Track->EndFrame && Next < Land)
		{
			Decision.Action = EGSTN_HoldClockAction::Land;
			Decision.TargetFrame = Land;
			Decision.TrackIndex = TrackIndex;
		}
		return Decision;
	}
	// Air timeout: still in the air part (not landed, not played through) after the move's air budget
	if (Track->MaxAirFrames > 0 && ElapsedFrames >= Track->MaxAirFrames && CurrentFrame < Track->EndFrame)
	{
		Decision.Action = EGSTN_HoldClockAction::Timeout;
		Decision.TrackIndex = TrackIndex;
		return Decision;
	}
	if (CurrentFrame == Track->EndFrame - 1)
	{
		Decision.Action = EGSTN_HoldClockAction::Stall;
		Decision.TrackIndex = TrackIndex;
	}
	return Decision;
}

FGSTN_HoldMontageTimes FGSTN_HoldUntilLanded::ResolveMontageTimes(const FGSTN_TimelineTrack_HoldUntilLanded& Track, const UAnimMontage* Montage, float BaseTime,
	float SecondsPerFrame)
{
	FGSTN_HoldMontageTimes Times;
	Times.LoopStart = BaseTime + Track.StartFrame * SecondsPerFrame;
	Times.LoopEnd = BaseTime + Track.EndFrame * SecondsPerFrame;
	Times.LandStart = BaseTime + Track.ResolveLandFrame() * SecondsPerFrame;
	if (!Montage)
	{
		return Times;
	}
	if (!Track.LoopSection.IsNone())
	{
		const int32 Section = Montage->GetSectionIndex(Track.LoopSection);
		if (Section != INDEX_NONE)
		{
			Montage->GetSectionStartAndEndTime(Section, Times.LoopStart, Times.LoopEnd);
		}
	}
	if (!Track.LandSection.IsNone())
	{
		const int32 Section = Montage->GetSectionIndex(Track.LandSection);
		if (Section != INDEX_NONE)
		{
			float End = 0.f;
			Montage->GetSectionStartAndEndTime(Section, Times.LandStart, End);
		}
	}
	return Times;
}

FGSTN_HoldMontageDecision FGSTN_HoldUntilLanded::ResolveMontage(const FGSTN_TimelineTrack_HoldUntilLanded& Track, const FGSTN_HoldMontageTimes& Times,
	int32 TimelineFrame, float MontageTime, float AdvanceSeconds, float SecondsPerFrame)
{
	FGSTN_HoldMontageDecision Decision;
	const int32 Land = Track.ResolveLandFrame();
	if (TimelineFrame >= Land)
	{
		// Landed (or played through without a ground fact): only a montage still before its land part moves. The frames past the land frame are a
		// late call (never the case when the ability runs every tick): the montage lands where the timeline is
		if (MontageTime < Times.LandStart - MontageTolerance)
		{
			Decision.Action = EGSTN_HoldMontageAction::Land;
			Decision.Time = Times.LandStart + (TimelineFrame - Land) * SecondsPerFrame;
		}
		return Decision;
	}
	if (Track.Contains(TimelineFrame) && Times.HasLoop() && MontageTime >= Times.LoopStart - MontageTolerance
		&& MontageTime + AdvanceSeconds >= Times.LoopEnd - MontageTolerance)
	{
		Decision.Action = EGSTN_HoldMontageAction::Wrap;
		Decision.Time = MontageTime - (Times.LoopEnd - Times.LoopStart);
	}
	return Decision;
}
