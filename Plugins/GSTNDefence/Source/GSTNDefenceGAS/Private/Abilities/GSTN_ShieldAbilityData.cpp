// Defence, ability-system half (no Network Prediction, no Mover).

#include "Abilities/GSTN_ShieldAbilityData.h"

#include "Data/GSTN_DefenceRules.h"
#include "Data/GSTN_DefenceTracks.h"
#include "Input/GSTN_CombatButtonTags.h"

FGSTN_ShieldAbilitySpec::FGSTN_ShieldAbilitySpec()
	: HoldButton(GSTNCombatButtonTags::Button_Shield.GetTag())
{
}

int32 FGSTN_ShieldAbilitySpec::HoldStartFrame(const FGSTN_ShieldRules& Rules)
{
	return FMath::Max(Rules.StartupFrames, 0);
}

FGSTN_CombatTimelineData FGSTN_ShieldAbilitySpec::BuildTimeline(const FGSTN_ShieldRules& Rules) const
{
	const int32 HoldStart = HoldStartFrame(Rules);
	const int32 HoldEnd = HoldStart + FMath::Max(HoldFrames, 1);

	FGSTN_CombatTimelineData Timeline;
	Timeline.TotalFrames = HoldEnd + FMath::Max(Rules.ReleaseFrames, 0);
	Timeline.FirstActionableFrame = Timeline.TotalFrames;
	Timeline.Montage = Montage;
	// The shield never replaces another move inside a startup window, and nothing replaces the shield inside its own
	Timeline.OverrideFrames = 0;

	FGSTN_HoldWindowTrack Hold;
	Hold.StartFrame = HoldStart;
	Hold.EndFrame = HoldEnd;
	Hold.HoldButton = HoldButton;
	Hold.bEndOnZeroShield = true;
	Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_HoldWindowTrack>(Hold));

	for (const FGSTN_OutOfShieldOption& Option : OutOfShieldOptions)
	{
		FGSTN_TimelineTrack_CancelWindow Window;
		Window.StartFrame = HoldStart;
		Window.EndFrame = HoldEnd;
		Window.IntoCommands = Option.IntoCommands;
		Window.Condition = EGSTN_CancelCondition::Always;
		Timeline.Tracks.Add(TInstancedStruct<FGSTN_TimelineTrack>::Make<FGSTN_TimelineTrack_CancelWindow>(Window));
	}
	return Timeline;
}
