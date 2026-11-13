#include "Simulation/GSTN_CombatTraceBuild.h"

#include "Simulation/GSTN_CombatSimTypes.h"

FGSTN_CombatTraceFrame GSTNCombatTraceNP::MakeFrame(int32 EntityId, int32 Frame, bool bResimulated, const FGSTN_CombatSyncCore& Core)
{
	FGSTN_CombatTraceFrame Out;
	Out.EntityId = EntityId;
	Out.Frame = Frame;
	Out.bResimulated = bResimulated;
	Out.ActionState = static_cast<uint8>(Core.ActionState);
	if (Core.ActionState == EGSTN_ActionState::InTimeline)
	{
		Out.TimelineIndex = Core.ActiveTimeline;
		// The same clock the timeline reads: the frames paused in hitstop do not advance it
		Out.TimelineFrame = FMath::Max(Frame - Core.TimelineStartFrame - Core.TimelinePausedFrames, 0);
	}
	// Frames still to come after this one
	Out.HitstopFrames = Core.HitstopEndFrame != INDEX_NONE ? FMath::Max(Core.HitstopEndFrame - Frame - 1, 0) : 0;
	Out.HitstunFrames = Core.Hit.HitstunEndFrame != INDEX_NONE ? FMath::Max(Core.Hit.HitstunEndFrame - Frame - 1, 0) : 0;
	Out.HistoryHead = Core.Input.History.GetLatestFrame();
	return Out;
}
