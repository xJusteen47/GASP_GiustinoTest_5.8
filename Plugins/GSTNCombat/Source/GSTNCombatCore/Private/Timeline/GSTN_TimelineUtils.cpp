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
