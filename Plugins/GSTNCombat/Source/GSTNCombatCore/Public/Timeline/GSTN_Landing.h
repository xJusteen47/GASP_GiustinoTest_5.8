// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_Landing.generated.h"

/** What a landing inside a Landing track's window does (owner 2026-11-16) */
UENUM(BlueprintType)
enum class EGSTN_LandingAction : uint8
{
	/** The move ends on the touchdown: the character is free at once (the autocancel window of an aerial) */
	AutoCancel,
	/** The move ends and the landing lag plays: Lag's frames locked, Lag's montage, cue and movement (played by the recoil player) */
	LandingLag,
	/** The move ends and ContinueMove starts on the same frame: a ground clip that continues the attack (its own hitboxes and frame data) */
	ContinueWithMove
};

/**
 * Landing (owner 2026-11-16): what touching the ground does while the timeline is inside [StartFrame, EndFrame). An aerial puts one window per
 * part, e.g. startup = ContinueWithMove, active = LandingLag 12 frames, late recovery = AutoCancel. Outside every window a landing does nothing
 * (the move plays on, the behaviour before this track existed). A Ground move that took off (the wizard's dropkick) uses it too: its windows count
 * only after the take-off. The ground is Mover's at the end of the previous frame, like Hold Until Landed's; without a Mover nothing happens.
 * A track of no known kind: the player ignores it, the combat clock reads it by struct. Simulation data only, rollback-safe
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Landing"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_Landing : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing")
	EGSTN_LandingAction Action = EGSTN_LandingAction::AutoCancel;

	/** LandingLag: frames locked (RecoilFrames), montage (RecoilMontage), cue, optional movement. Not authored (0 frames) = AutoCancel */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (EditCondition = "Action == EGSTN_LandingAction::LandingLag"))
	FGSTN_RicochetResponse Lag;

	/**
	 * ContinueWithMove: the timeline ability (a UGSTN_GA_BaseCombat child) started on the touchdown frame, like a follow-up (source Forced, its
	 * own frame data; the combat model registers it by itself). None or not registered = AutoCancel
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Landing", meta = (EditCondition = "Action == EGSTN_LandingAction::ContinueWithMove", AllowedClasses = "/Script/GSTNCombatNP.GSTN_GA_BaseCombat"))
	TSoftClassPtr<UObject> ContinueMove;
};

/** What the ground rules ask the combat clock to do on one tick */
enum class EGSTN_GroundClockAction : uint8
{
	None,
	/** A Ground move in the air outside its take-off: the move ends (not "by itself": no follow-up) */
	CancelAirborne,
	/** A landing inside a Landing window: TrackIndex names the window, its Action says what follows */
	Land
};

struct FGSTN_GroundClockDecision
{
	EGSTN_GroundClockAction Action = EGSTN_GroundClockAction::None;
	int32 TrackIndex = INDEX_NONE;
	/** The value the move's "may be airborne" bit takes after this tick (FGSTN_CombatSyncCore::bAirborneOk) */
	bool bAirborneOk = false;
};

/** The ground rules of a move (EGSTN_MoveGroundRule, Landing tracks), pure functions for the combat clock and the tests */
struct GSTNCOMBATCORE_API FGSTN_LandingRules
{
	/** The first Landing track of Data whose window holds Frame, null when none (OutIndex its index) */
	static const FGSTN_TimelineTrack_Landing* FindWindow(const FGSTN_CombatTimelineData& Data, int32 Frame, int32* OutIndex = nullptr);

	/** The Landing tracks' ContinueMove classes may only be resolved where the class is known; this says whether Data has any Landing track */
	static bool HasLandingTracks(const FGSTN_CombatTimelineData& Data);

	/**
	 * The rule of one tick. Frame = the timeline frame this tick evaluates (previous + 1), bOnGround = Mover's ground at the end of the previous
	 * frame, bFirstTick = the first tick after the start frame, bAirborneOk = the move's bit so far (set when it took off in its window or started
	 * in coyote time; cleared on the ground).
	 * Ground move in the air: allowed (bit set) in the take-off window, or on the first tick of a move that may take off (coyote start); else
	 * CancelAirborne. On the ground inside a Landing window: Land, for an Air or Any move always, for a Ground move only after a take-off
	 */
	static FGSTN_GroundClockDecision Resolve(const FGSTN_CombatTimelineData& Data, int32 Frame, bool bOnGround, bool bFirstTick, bool bAirborneOk);

	/**
	 * Touchdown anticipation (UGSTN_CombatRules::bAnticipateTouchdown): how far a fighter falls this frame (UpSpeed cm/s along up, negative when
	 * falling; GravityAccel cm/s^2, positive; StepSeconds the fixed step), and whether that reaches a floor FloorDistance below the capsule
	 * (MarginCm more). Rising or still: never. Pure
	 */
	static double FallThisFrame(double UpSpeed, double GravityAccel, double StepSeconds);
	static bool ReachesFloorThisFrame(double UpSpeed, double GravityAccel, double StepSeconds, double FloorDistance, double MarginCm);
};
