// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_Ledge.generated.h"

/** What a Ledge Check window does when there is no floor ahead (owner 2026-11-16) */
UENUM(BlueprintType)
enum class EGSTN_LedgeAction : uint8
{
	/** The move plays on, the character stays where it is (on the lip) while the window is open and the void is ahead. Root motion and the
	 *  Movement track are held, the animation is not: a dash that should brake at the edge */
	Stop,
	/** The move ends at once (an interrupt, no follow-up): the character is free on the lip */
	EndMove,
	/** The move ends into Response (frames locked, montage, cue, optional movement), played by the recoil player: a teeter or a brake clip */
	Recoil,
	/** The move ends and ContinueMove starts on the same frame (its own frame data): e.g. a dash that turns into a hop off the ledge */
	ContinueWithMove
};

/**
 * Ledge Check (owner 2026-11-16): while the timeline is inside [StartFrame, EndFrame) and the character is on the ground, the floor is probed
 * ProbeAhead cm in front of the character (along its facing) down to ProbeDepth cm under its feet. No floor there = a ledge (or a hole) ahead,
 * and Action decides what the move does. Outside the window, or in the air, nothing is probed (the Ground Rule of the move still applies when the
 * character walks off). The probe uses Mover's pose at the end of the previous frame, like every ground rule of the clock, so the character
 * travels at most one frame further: keep ProbeAhead above the move's speed per frame (850 cm/s = 14 cm per frame at 62.5 Hz). The probe runs
 * from the second frame of the move (timeline frame 1) and never on a hitstop frame.
 * A track of no known kind: the player ignores it, the combat clock reads it by struct. Simulation data only, rollback-safe
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Ledge Check"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_LedgeCheck : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ledge")
	EGSTN_LedgeAction Action = EGSTN_LedgeAction::Stop;

	/** How far in front of the character's centre the floor must exist (cm, along the facing). With Stop it is how far from the lip it halts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ledge", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ProbeAhead = 40.f;

	/** How far under the feet the probe looks for floor (cm): a drop shallower than this is a step, not a ledge */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ledge", meta = (ClampMin = "1", ForceUnits = "cm"))
	float ProbeDepth = 60.f;

	/** Recoil: frames locked (RecoilFrames), montage (RecoilMontage), cue, optional movement. Not authored (0 frames) = EndMove */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ledge", meta = (EditCondition = "Action == EGSTN_LedgeAction::Recoil"))
	FGSTN_RicochetResponse Response;

	/**
	 * ContinueWithMove: the timeline ability (a UGSTN_GA_BaseCombat child) started on the frame the ledge is seen, like a follow-up (source Forced,
	 * its own frame data; the combat model registers it by itself). None or not registered = EndMove
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ledge", meta = (EditCondition = "Action == EGSTN_LedgeAction::ContinueWithMove", AllowedClasses = "/Script/GSTNCombatNP.GSTN_GA_BaseCombat"))
	TSoftClassPtr<UObject> ContinueMove;
};

/** The ledge rules of a move (Ledge Check tracks), pure functions for the combat clock and the tests */
struct GSTNCOMBATCORE_API FGSTN_LedgeRules
{
	/** The first Ledge Check track of Data whose window holds Frame, null when none (OutIndex its index) */
	static const FGSTN_TimelineTrack_LedgeCheck* FindWindow(const FGSTN_CombatTimelineData& Data, int32 Frame, int32* OutIndex = nullptr);

	/** Data has at least one Ledge Check track */
	static bool HasLedgeTracks(const FGSTN_CombatTimelineData& Data);

	/** Data has a Ledge Check track with the Stop action (its ability queues the ledge guard move on Mover) */
	static bool HasStopTracks(const FGSTN_CombatTimelineData& Data);

	/** The ContinueMove of Data's track TrackIndex when it is a Ledge Check with ContinueWithMove, null otherwise */
	static const TSoftClassPtr<UObject>* GetContinueMove(const FGSTN_CombatTimelineData& Data, int32 TrackIndex);
};
