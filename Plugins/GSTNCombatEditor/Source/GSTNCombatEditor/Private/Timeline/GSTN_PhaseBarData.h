// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelinePhases.h"
#include "Timeline/GSTN_TimelineTypes.h"

/** What a stretch of the phase bar is */
enum class EGSTN_PhaseKind : uint8
{
	/** Before the first hitbox window */
	Startup,
	/** Inside a merged hitbox window range */
	Active,
	/** After an active range, until the first actionable frame (also the gaps between two ranges) */
	Recovery,
	/** From the first actionable frame to the end */
	Actionable,
	/** A timeline with no hitbox: everything before the first actionable frame */
	Busy
};

struct FGSTN_PhaseSegment
{
	/** [Start, End) in frames */
	int32 Start = 0;
	int32 End = 0;
	EGSTN_PhaseKind Kind = EGSTN_PhaseKind::Busy;
};

/**
 * The frame data of a timeline as the phase bar and the summary line show it: startup, active, recovery, first actionable
 * frame, total, and the cancellable tail.
 *
 * The numbers come from FGSTN_TimelinePhases (GSTNCombatCore, E3), which the runtime debug draw and the harness share; this struct only
 * adds what drawing needs: the bar segments in order. It is a view model, not a second computation.
 *
 * Rules (combat-m5-editor 5.3): the active ranges are the merged [StartFrame, EndFrame) of every Hitbox track; startup is the
 * number of frames before the first one (the 1-based first hit frame is startup + 1); recovery runs from the last active frame to
 * the first actionable frame; no hitbox means the whole stretch before the first actionable frame is "busy". The tail is a
 * separate band after the actionable frame, never a phase.
 */
struct FGSTN_PhaseBarData
{
	int32 TotalFrames = 0;
	int32 FirstActionableFrame = 0;
	bool bHasActive = false;

	/** Frames before the first active frame (the first active frame's 0-based index) */
	int32 StartupFrames = 0;

	/** From the first active frame to the end of the last active range, gaps between ranges included */
	int32 ActiveFrames = 0;

	/** From the end of the last active range to the first actionable frame, 0 if it is already past */
	int32 RecoveryFrames = 0;

	/** Merged active ranges as [Start, End) */
	TArray<FIntPoint> ActiveRanges;

	/** In order, covering [0, max(TotalFrames, FirstActionableFrame)), empty ones left out. They overlap only when a hitbox window runs past the first actionable frame, which the validator reports */
	TArray<FGSTN_PhaseSegment> Segments;

	/** First frame of the cancellable tail, INDEX_NONE when there is none. The tail runs to TotalFrames */
	int32 TailStart = INDEX_NONE;
	int32 TailBlendOutFrames = 0;

	static FGSTN_PhaseBarData Compute(const FGSTN_CombatTimelineData& Data);

	/** "Startup 7 | Active 3 | Recovery 14 | FAF 24 | Total 30 (480 ms)", or with "No hitbox" in place of the first three */
	FText FormatSummary(float TickRate) const;
};
