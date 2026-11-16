// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_HoldUntilLanded.generated.h"

class UAnimMontage;

/**
 * Hold Until Landed (dropkick, owner 2026-11-15): an air move whose middle part lasts as long as the fall. [StartFrame, EndFrame) is the airborne
 * part of the timeline (the loop of the montage). While the character is in the air the timeline plays into this range and then STANDS on its last
 * frame (EndFrame - 1), exactly like a hold or a hitstop stall: the tracks open there stay open (the dropkick's hitbox), nothing keyed to a frame
 * fires again, the timeline ends later. The montage keeps looping its loop part meanwhile. On the first frame the character is seen on the ground
 * (from LandFromFrame on, before the timeline passed the range) the timeline JUMPS to LandFrame and the montage to its land part, on the same
 * frame: the land clip always starts on the touchdown, however long the fall was. Frames skipped by the jump fire nothing.
 *
 * The ground is Mover's state at the end of the previous frame (one frame late, like the dash's bCancelOnLand), read by the combat tick; without
 * a Mover component the track does nothing and the timeline plays straight through. Simulation data only: the clock and the montage are synced
 * state, so a rollback replays the same hold and the same landing.
 *
 * Montage: by default the timeline's frames map to the montage as they do at the start of the move (frame f = f simulation steps at the timeline's
 * MontagePlayRate from the start of MontageSection), so a montage built with one animation frame per tick needs no sections: the loop is the montage
 * time of [StartFrame, EndFrame) and the land part starts at the time of LandFrame. A montage with sections names them instead (LoopSection,
 * LandSection). A track of no known kind: the player ignores it, the combat clock reads it by struct.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Hold Until Landed"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_HoldUntilLanded : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** The timeline frame the move continues on when it lands: the first frame of its landing part. -1 = EndFrame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold Until Landed", meta = (ClampMin = "-1", ForceUnits = "Frames"))
	int32 LandFrame = -1;

	/**
	 * Landing counts from this timeline frame on (0 = the whole move up to the hold: a dropkick started low lands out of its startup too). Raise it
	 * for a move that starts on the ground and only leaves it a few frames in (the first frames would read as a landing)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold Until Landed", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 LandFromFrame = 0;

	/** Montage section repeated while the hold lasts. None = the montage time of [StartFrame, EndFrame) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold Until Landed|Montage")
	FName LoopSection = NAME_None;

	/** Montage section the montage jumps to on landing. None = the montage time of LandFrame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hold Until Landed|Montage")
	FName LandSection = NAME_None;

	/** LandFrame with -1 resolved */
	int32 ResolveLandFrame() const { return LandFrame >= 0 ? LandFrame : EndFrame; }
};

/** What the hold asks the combat clock to do on one tick */
enum class EGSTN_HoldClockAction : uint8
{
	/** Play on */
	None,
	/** In the air on the last frame of the hold: stand on it this tick */
	Stall,
	/** On the ground: jump to TargetFrame this tick */
	Land
};

struct FGSTN_HoldClockDecision
{
	EGSTN_HoldClockAction Action = EGSTN_HoldClockAction::None;
	/** With Land: the timeline frame to evaluate on this tick */
	int32 TargetFrame = INDEX_NONE;
	/** The track's index in the timeline data, INDEX_NONE with None */
	int32 TrackIndex = INDEX_NONE;
};

/** What the montage of a holding move has to do before the montage ticks (the ability asks this every frame) */
enum class EGSTN_HoldMontageAction : uint8
{
	None,
	/** Back by one loop length, so this tick's advance stays inside the loop */
	Wrap,
	/** The timeline landed: the montage goes to its land part (a new start of the presentation) */
	Land
};

struct FGSTN_HoldMontageDecision
{
	EGSTN_HoldMontageAction Action = EGSTN_HoldMontageAction::None;
	/** The montage time to set */
	float Time = 0.f;
};

/** The montage times of one hold track, in seconds of the montage */
struct FGSTN_HoldMontageTimes
{
	float LoopStart = 0.f;
	float LoopEnd = 0.f;
	float LandStart = 0.f;

	bool HasLoop() const { return LoopEnd > LoopStart; }
};

/** The rules of the Hold Until Landed track, pure functions for the combat clock, the ability and the tests */
struct GSTNCOMBATCORE_API FGSTN_HoldUntilLanded
{
	/** The first Hold Until Landed track of Data, null when it has none (OutIndex its index) */
	static const FGSTN_TimelineTrack_HoldUntilLanded* FindTrack(const FGSTN_CombatTimelineData& Data, int32* OutIndex = nullptr);

	/**
	 * The clock rule. CurrentFrame is the timeline frame the timeline stands on (evaluated by the previous tick); a tick that does nothing evaluates
	 * CurrentFrame + 1. bGroundKnown false (no Mover) = None. On the ground, from LandFromFrame, before the timeline left the hold range and before
	 * it reached the land frame by itself: Land. In the air on the hold's last frame: Stall. Not called on a hitstop tick (it stalls already)
	 */
	static FGSTN_HoldClockDecision ResolveClock(const FGSTN_CombatTimelineData& Data, int32 CurrentFrame, bool bGroundKnown, bool bOnGround);

	/**
	 * The montage times of Track: its sections when named and found in Montage, else the timeline frames mapped as the move's start maps them:
	 * BaseTime + frame * SecondsPerFrame (SecondsPerFrame = the fixed step in seconds times MontagePlayRate). A null montage maps the frames
	 */
	static FGSTN_HoldMontageTimes ResolveMontageTimes(const FGSTN_TimelineTrack_HoldUntilLanded& Track, const UAnimMontage* Montage, float BaseTime, float SecondsPerFrame);

	/**
	 * The montage rule, asked before the montage ticks. TimelineFrame = the frame the clock evaluated this tick, MontageTime = the montage before its
	 * advance, AdvanceSeconds = what this tick adds to it (0 while paused). Landed (TimelineFrame >= the land frame) with the montage still before its
	 * land part: Land, at LandStart plus the frames the timeline is past the land frame. Inside the hold range with the montage in its loop and this
	 * tick's advance reaching the loop's end: Wrap. A montage that already loops its section by itself never reaches the end: nothing to do
	 */
	static FGSTN_HoldMontageDecision ResolveMontage(const FGSTN_TimelineTrack_HoldUntilLanded& Track, const FGSTN_HoldMontageTimes& Times, int32 TimelineFrame,
		float MontageTime, float AdvanceSeconds, float SecondsPerFrame);
};
