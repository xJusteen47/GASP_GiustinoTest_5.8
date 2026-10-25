#include "Timeline/GSTN_TimelineUtils.h"
#include "Timeline/GSTN_TimelinePlayer.h"

bool UGSTN_TimelineUtils::IsInTail(const FGSTN_CombatTimelineData& Data, int32 Frame)
{
	return FGSTN_TimelinePlayer::IsInTail(Data, Frame);
}

int32 UGSTN_TimelineUtils::GetTailStartFrame(const FGSTN_CombatTimelineData& Data)
{
	return Data.GetTailStartFrame();
}

FGSTN_CombatTimelineData UGSTN_TimelineUtils::AddHitboxTrack(const FGSTN_CombatTimelineData& Data, int32 StartFrame, int32 EndFrame, const FGSTN_PartSelector& Part, const TArray<FGSTN_HitboxSpec>& Hitboxes)
{
	FGSTN_CombatTimelineData Result = Data;
	FGSTN_TimelineTrack_Hitbox Track;
	Track.StartFrame = StartFrame;
	Track.EndFrame = EndFrame;
	Track.Part = Part;
	Track.Hitboxes = Hitboxes;
	// The derived type must be named or the track is sliced to the base
	Result.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_Hitbox>(Track));
	return Result;
}
