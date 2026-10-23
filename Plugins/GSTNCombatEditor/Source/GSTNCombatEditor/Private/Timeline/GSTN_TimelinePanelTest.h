// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineTypes.h"

/**
 * The test tab of the timeline panel (plan m5 E4): a nomad tab that hosts SGSTN_TimelinePanel on a transient timeline, opened by
 * the console command "gstn.Combat.OpenTimelinePanelTest" ("... stress" for 12 keyed tracks of 60 keys, to read the paint time).
 * It exists so the panel can be used and checked before the toolkit of E5 hosts it, and is removed in E5 together with the
 * demo keyed track it borrows from the test types.
 *
 * Editor only: registered by the module on startup, removed on shutdown.
 */
namespace GSTNTimelinePanelTest
{
	/** The name of the console command */
	extern const TCHAR* const OpenCommandName;

	void Register();
	void Unregister();

	/**
	 * Fills a timeline with something to edit: a hitbox pair (so the phase bar has an active range), cancel window, movement, tags,
	 * an event, a weapon switch, a tail, and a track with keys. With bStress, 12 keyed tracks of 60 keys over 120 frames.
	 */
	void FillDemoTimeline(FGSTN_CombatTimelineData& Data, bool bStress);
}
