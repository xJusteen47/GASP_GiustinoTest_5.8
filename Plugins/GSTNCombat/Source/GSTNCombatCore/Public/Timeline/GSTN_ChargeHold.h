// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_ChargeHold.generated.h"

struct FGSTN_InputHistory;

/**
 * Charge Hold (owner 2026-11-25, aerial charge prototype): a move whose button is still held when its timeline reaches StartFrame STANDS on that
 * frame, in its pose, for as long as the button stays down (at most MaxHoldFrames), then plays the rest of its startup on the release. A tap plays
 * through as if the track were not there. In the air the character keeps falling and drifting with the move stick meanwhile, so the player picks the
 * aerial with the press and positions with the stick before committing it, with no back-and-forth on the stick.
 *
 * "Held" means held CONTINUOUSLY since the move started: a press buffered before the start counts, a tap released before StartFrame and pressed
 * again does not hold the move. The stall is the hitstop's and Hold Until Landed's (TimelinePausedFrames grows, nothing keyed to a frame fires
 * again, tracks open on StartFrame stay open); the combat clock and the buffers keep running.
 *
 * Pose, after Smash Ultimate's smash charge (owner 2026-11-25): the move stops a few frames into its startup, in an anticipation pose, and goes on
 * with the REST of the startup on the release, so a tap costs nothing and a hold adds only the time held. Without LoopSection the synced montage is
 * paused on the hold frame (the combat model pauses it like a hitstop: the pose holds, no root motion repeats); with one, the timeline ability
 * plays that section in a loop while the move stands (Ultimate's trembling charge loop) and puts the montage back on the hold frame's pose at
 * the release. The glow and the sound of the charge are presentation: a move cosmetic window on [StartFrame, StartFrame + 1) stays live for the
 * whole hold, since the timeline frame stands on it.
 *
 * Direction (bLockDirection, owner choice 2A of 2026-11-25): on, the start aim and the reverse window end at StartFrame and the steering holds
 * the yaw while the move stands, so whatever the stick does during the hold only drifts the character. The steering window goes on after the
 * release: the move's small steering cone (MaxSteerDegrees) is the angle tolerance at the confirm, as for every attack.
 *
 * Simulation data only: the decision is a pure function of the timeline frame, the frames since the move started and the input history at the end
 * of the previous frame, so a rollback replays the same hold. Read by the combat clock by struct (FGSTN_ChargeHold), never by the player.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Charge Hold"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_ChargeHold : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** The button that holds the move. Usually the attack button (the button of the move's own command) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge Hold", meta = (Categories = "Input.Button"))
	FGameplayTag HoldButton;

	/**
	 * A button layout function ("Combat.Attack"). Set, the clock reads that function's button in the active layout, with HoldButton as the default
	 * (as the shield's Hold Window does), so a player who remapped the attack still holds with it. None = HoldButton as authored
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge Hold")
	FName LayoutFunction = TEXT("Combat.Attack");

	/**
	 * Longest hold, in simulation frames (62.5 per second) spent standing on StartFrame; then the move goes on by itself, as a fully charged smash
	 * attack does in Smash Ultimate (60 of its frames, about 1 s). 0 = the track is off
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge Hold", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 MaxHoldFrames = 60;

	/**
	 * Damage multiplier of a full hold (owner option 1C: per move, off by default). The move's hits scale their damage linearly from 1 (a tap) to this
	 * after MaxHoldFrames; the knockback grows with the damage through the formula, as Smash Ultimate's smash charge (1.4 there). 1 = a pure pose hold
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge Hold", meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float FullChargeDamageScale = 1.f;

	/**
	 * The move's direction is fixed before the hold: start aim, reverse window and steering end at StartFrame (owner option 2A). Off, they keep
	 * their own windows, so a stick flicked back during a hold inside the reverse window still turns the move (option 2B)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge Hold")
	bool bLockDirection = true;

	/**
	 * Montage section looped while the move stands on the hold frame (a charge tremble). None = the montage is paused on the hold frame's pose.
	 * At the release the montage goes back to the hold frame's time and the startup plays on from there
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Charge Hold|Montage")
	FName LoopSection = NAME_None;

	/** HoldButton, or LayoutFunction's button in the active layout */
	FGameplayTag ResolveHoldButton() const;
};

/** The rules of the Charge Hold track, pure functions for the combat clock, the startup steering and the tests */
struct GSTNCOMBATCORE_API FGSTN_ChargeHold
{
	/** The first Charge Hold track of Data that is on (MaxHoldFrames > 0), null when it has none (OutIndex its index) */
	static const FGSTN_TimelineTrack_ChargeHold* FindTrack(const FGSTN_CombatTimelineData& Data, int32* OutIndex = nullptr);

	/**
	 * Whether Button has been down without a break since the move's start: down at the latest frame of History (the end of the previous simulation
	 * frame) and pressed on TimelineStartFrame or before. False for INDEX_NONE or a button past the mask
	 */
	static bool IsHeldSinceStart(const FGSTN_InputHistory& History, int32 Button, int32 TimelineStartFrame);

	/**
	 * Whether the clock stands on the hold frame this tick. TimelineFrame = the frame the previous tick evaluated (the one a stall holds);
	 * FramesSinceStart = simulation frames since the move started (this tick's frame minus the start frame). The start tick never stalls; the k-th
	 * stalled tick has FramesSinceStart = StartFrame + k (no pause before the hold: a hit in the startup interrupts the move), so the cap is exact
	 */
	static bool ShouldStall(const FGSTN_TimelineTrack_ChargeHold& Track, int32 TimelineFrame, int32 FramesSinceStart, bool bHeldSinceStart);

	/**
	 * The frame the start aim, the reverse window and the steering must end at (exclusive) because of a Charge Hold with bLockDirection:
	 * its StartFrame. MAX_int32 without one, so callers take the minimum with their own end
	 */
	static int32 GetDirectionLockFrame(const FGSTN_CombatTimelineData& Data);

	/** The damage scale of Data's hits after ChargeFrames ticks on the hold frame: 1 without a Charge Hold or with FullChargeDamageScale 1 */
	static float GetDamageScale(const FGSTN_CombatTimelineData& Data, int32 ChargeFrames);

	/** Appends one message per problem: no button, a hold frame at 0 (the start tick never stalls) or past the first active frame */
	static void Validate(const FGSTN_CombatTimelineData& Data, const FString& Label, TArray<FString>& OutErrors);
};
