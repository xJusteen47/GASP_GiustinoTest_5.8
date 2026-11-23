// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Movement/GSTN_MomentumSlide.h"
#include "Rules/GSTN_KitExtension.h"
#include "GSTN_DodgeLanding.generated.h"

namespace GSTNCombatTags
{
	/**
	 * The combatant is in the ending lag of a dodge landing (owner 2026-11-17: "the dodge needs ending lag"). Projected by the combat tick from
	 * FGSTN_DodgeLandingState for the frames of the lag, never granted: the hook a landing clip (a dive landing with its recovery) attaches to.
	 */
	GSTNCOMBATCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dodge_EndLag);
}

/**
 * A kit's dodge landing (owner 2026-11-17, scope: the ground long-jump dodge and the air dodge that reuses it). A kit without this extension
 * gets these defaults, so every kit has the lag and the re-dodge lock; the extension is only added to tune them. Data per kit, nothing in the
 * project settings. Copied into the loadout's table set (UGSTN_CombatTableSet::KitDodgeLanding).
 *
 * Owner feel reference (2026-11-17 20:19Z): at least a dive (Mario 64 dive and roll-out, Helldivers dive, Mario 64 long jump), balanced so it is
 * not too strong. The ending lag is the belly landing; the optional recovery commands are the roll-out (off by default, to try in playtest).
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Dodge Landing"))
struct GSTNCOMBATCORE_API FGSTN_DodgeLandingKitExtension : public FGSTN_KitExtension
{
	GENERATED_BODY()

	FGSTN_DodgeLandingKitExtension();

	/**
	 * Frames of ending lag from the landing, counted from 1 (the first frame the simulation sees the ground). In the lag the character cannot act
	 * or walk (the speed it kept brakes with the normal ground friction), is vulnerable, has no intangibility. Below 0 = the move's own
	 * (the movement track's LandingLagFrames: 8 on the default ground and air dodge)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge Landing", meta = (ClampMin = "-1", ClampMax = "255", ForceUnits = "Frames"))
	int32 EndLagFrames = -1;

	/**
	 * Frames from the landing, counted from 1, in which LockedCommands are refused (owner 1A: no dodging again straight after landing). It runs
	 * alongside the ending lag, so only the frames past the lag add anything. The input buffer is the usual one: a dodge pressed in the lock starts
	 * on the first free frame only if the buffer still holds it. 0 = no lock (the behaviour before 2026-11-17)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge Landing", meta = (ClampMin = "0", ClampMax = "255", ForceUnits = "Frames"))
	int32 RedodgeLockFrames = 10;

	/** The commands the re-dodge lock refuses. Empty = Command.Defence.Dodge (the ground and the air dodge share it) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge Landing", meta = (Categories = "Command"))
	FGameplayTagContainer LockedCommands;

	/**
	 * Roll-out (Mario 64 dive recovery): commands that may start inside the ending lag from RecoveryFromFrame on, ending the lag there. Empty = off
	 * (default): the lag runs to its end. A locked command stays refused even when listed here
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge Landing|Recovery", meta = (Categories = "Command"))
	FGameplayTagContainer RecoveryCommands;

	/** The first frame of the ending lag (counted from 1, the landing frame is 1) a recovery command may start on */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge Landing|Recovery", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 RecoveryFromFrame = 1;

	/**
	 * Belly slide on landing (owner 2026-11-17 20:21Z: "something physical, it may slide on landing with momentum"): the momentum slide of the ground
	 * moves (FGSTN_MomentumSlide, same data and friction models) started on the landing frame from the landing velocity, for at most the ending lag
	 * (it stops when the lag ends, is cut by a recovery, or a hit lands; then the walking mode takes over with the speed left). Its friction is
	 * Along/Against, its length the lag, MaxFrames or the speed running out. Defaults: on, a Fixed 4000 cm/s^2 slide along the landing velocity
	 * (1200 cm/s keeps about 690 cm/s after 8 frames at 62.5 Hz); never Against, no run carry, no root motion. bEnabled off = the lag brakes with
	 * the walking mode's own friction
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dodge Landing|Slide")
	FGSTN_MomentumSlide LandingSlide;
};

/**
 * The windows a dodge landing opens, in the combat sync state (rollback safe, serialized; one bit while unset). Written on the landing frame by
 * the dodge's ability, cut by a recovery, cleared by a hit reaction and once both windows are over. Pure data and frame arithmetic.
 */
struct GSTNCOMBATCORE_API FGSTN_DodgeLandingState
{
	/** First frame the simulation saw the ground again, INDEX_NONE while unset */
	int32 LandedFrame = INDEX_NONE;

	/** Ending lag: frames [LandedFrame, LandedFrame + EndLagFrames) */
	uint8 EndLagFrames = 0;

	/** Re-dodge lock: frames [LandedFrame, LandedFrame + LockFrames) */
	uint8 LockFrames = 0;

	bool IsSet() const { return LandedFrame != INDEX_NONE; }

	/** The windows of a landing on LandedFrame; unset when both are 0 */
	static FGSTN_DodgeLandingState Open(int32 InLandedFrame, int32 InEndLagFrames, int32 InLockFrames);

	bool IsEndLag(int32 Frame) const { return IsSet() && Frame >= LandedFrame && Frame < LandedFrame + EndLagFrames; }
	bool IsLocked(int32 Frame) const { return IsSet() && Frame >= LandedFrame && Frame < LandedFrame + LockFrames; }

	/** Frame of the ending lag counted from 1 (the landing frame is 1), 0 outside it */
	int32 EndLagFrameNumber(int32 Frame) const { return IsEndLag(Frame) ? Frame - LandedFrame + 1 : 0; }

	/** Both windows are over on Frame */
	bool HasExpired(int32 Frame) const { return !IsSet() || Frame >= LandedFrame + FMath::Max(EndLagFrames, LockFrames); }

	/** Ends the ending lag on Frame (a recovery started on it); the lock keeps running */
	void CutEndLag(int32 Frame);

	void Reset() { *this = FGSTN_DodgeLandingState(); }

	bool operator==(const FGSTN_DodgeLandingState& Other) const
	{
		return LandedFrame == Other.LandedFrame && (!IsSet() || (EndLagFrames == Other.EndLagFrames && LockFrames == Other.LockFrames));
	}

	void NetSerialize(FArchive& Ar);
};

/** What the dodge landing says about a command on a frame (the gate of the combat model asks it before every other rule) */
enum class EGSTN_DodgeLandingGate : uint8
{
	/** Not the landing's business: the other rules decide */
	Free,

	/** Refused: in the ending lag (not a recovery), or a locked command in the re-dodge lock */
	Refused,

	/** A recovery command in the ending lag: allowed whatever the running move says, and the lag ends */
	Recovery
};

/** The rules of the dodge landing, pure functions of the kit data, the state and the frame */
struct GSTNCOMBATCORE_API FGSTN_DodgeLandingLogic
{
	/** The ending lag the kit gives a move whose movement track has MoveLandingLagFrames */
	static int32 ResolveEndLag(const FGSTN_DodgeLandingKitExtension& Kit, int32 MoveLandingLagFrames);

	/** Command is one the re-dodge lock refuses */
	static bool IsLockedCommand(const FGSTN_DodgeLandingKitExtension& Kit, const FGameplayTag& Command);

	static EGSTN_DodgeLandingGate Gate(const FGSTN_DodgeLandingState& State, const FGSTN_DodgeLandingKitExtension& Kit, int32 Frame, const FGameplayTag& Command);

	/** Keeps the state canonical at the top of a frame: a hit reaction clears it (the lag of a dodge that was hit is over), so do expired windows */
	static void Normalize(FGSTN_DodgeLandingState& State, int32 Frame, bool bInReaction);
};
