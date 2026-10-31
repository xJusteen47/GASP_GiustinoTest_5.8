// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineTypes.h"

/**
 * The frame data of a timeline as fighting-game players read it: startup, active, recovery, first actionable frame (FAF) and
 * total (combat-m5-editor 5.3). Derived, never stored: the timeline asset keeps only the windows and FirstActionableFrame, and
 * this is computed from them on demand. It lives in Core so the editor's phase bar, the validators, the runtime debug draw and
 * the harness all read the same numbers.
 *
 * Rules:
 *  - The active ranges are the merged [StartFrame, EndFrame) of every Hitbox track window. Overlapping or touching windows
 *    become one range. A window whose hitboxes are all ignored for every relationship (FGSTN_HitVariant::bIgnore on Self, Ally,
 *    Enemy and Neutral) hits nobody and does not count; a Hitbox track with no hitbox spec at all still counts (the shell of a
 *    move being authored, which the validator reports separately).
 *  - Startup is the number of frames before the first active frame, i.e. the 0-based index of that frame. The fighting-game
 *    "first hit frame" is ActiveStart + 1 (FirstHitFrame1Based), 0 when there is no hitbox.
 *  - Active runs from the first active frame to the end of the last range. Gaps between two ranges are inside it (a multi-hit
 *    move is one "Active 16"), the ranges themselves are in ActiveRanges.
 *  - Recovery runs from the end of the last range to the FAF, 0 when the FAF is already past it.
 *  - With no hitbox the whole stretch before the FAF is "busy": there is no startup, active or recovery to report.
 *  - The cancellable tail (FGSTN_TimelineTail, DD 6c) is separate: it starts at or after the FAF and is never a phase, so
 *    it changes none of the numbers above. It is reported as TailStart / TailBlendOutFrames.
 *
 * The tick rate is a parameter of FormatSummary and never read from a world: milliseconds are display only.
 */
struct GSTNCOMBATCORE_API FGSTN_TimelinePhases
{
	int32 Total = 0;
	int32 FirstActionableFrame = 0;

	/** At least one hitbox window counts */
	bool bHasActive = false;

	/** Frames before the first active frame, the end (exclusive) of the startup stretch. 0 without a hitbox */
	int32 StartupEnd = 0;

	/** First active frame (0-based index). 0 without a hitbox */
	int32 ActiveStart = 0;

	/** One past the last active frame. 0 without a hitbox */
	int32 ActiveEnd = 0;

	/** The end of the recovery stretch, which is the FAF (what the spec calls RecoveryEnd). Equal to FirstActionableFrame */
	int32 RecoveryEnd = 0;

	/** ActiveStart + 1 (the fighting-game convention), 0 without a hitbox */
	int32 FirstHitFrame1Based = 0;

	/** Merged active ranges as [Start, End), sorted */
	TArray<FIntPoint> ActiveRanges;

	/** First frame of the cancellable tail, INDEX_NONE if there is none. The tail runs to Total */
	int32 TailStart = INDEX_NONE;
	int32 TailBlendOutFrames = 0;

	int32 GetStartupFrames() const { return StartupEnd; }
	int32 GetActiveFrames() const { return ActiveEnd - ActiveStart; }
	int32 GetRecoveryFrames() const { return bHasActive ? FMath::Max(RecoveryEnd - ActiveEnd, 0) : 0; }

	static FGSTN_TimelinePhases Compute(const FGSTN_CombatTimelineData& Data);

	/** Does the hitbox track contribute an active window? See the rules above */
	static bool CountsAsActive(const FGSTN_TimelineTrack_Hitbox& Track);

	/** "Startup 7 | Active 3 | Recovery 14 | FAF 24 | Total 30 (484 ms)", or "No hitbox | FAF 24 | Total 30 (484 ms)" */
	FText FormatSummary(float TickRate) const;
};
