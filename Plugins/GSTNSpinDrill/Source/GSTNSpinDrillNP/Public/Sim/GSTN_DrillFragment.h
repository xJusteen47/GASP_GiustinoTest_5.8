// Spin and drill, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Sync/GSTN_SyncFragment.h"
#include "GSTN_DrillFragment.generated.h"

/** Phase of the spin and drill (spin-drill.md 7.1). Stored as one byte; fits 3 bits */
enum class EGSTN_DrillPhase : uint8
{
	None,
	Spinning,
	Diving,
	Seating,
	Boring,
	Launching,
	Lag,

	Count
};

/** Bits of FGSTN_DrillFragment::Flags */
namespace GSTNDrillFlags
{
	/** The current (or last) spin turns counter-clockwise; clear = clockwise. The opposite-spin cancel keys on it */
	constexpr uint8 CounterClockwise = 1 << 0;

	/** The current spin is the air spin (it counted against AirSpinsUsed) */
	constexpr uint8 AirSpin = 1 << 1;

	/**
	 * The dive just ended on a hittable target that is not drillable (spin-drill.md 7.6): set with the phase back to None on the bounce frame,
	 * cleared at the next AfterClocks. The dive ability reads it to apply the bounce and end; nothing else does
	 */
	constexpr uint8 Bounced = 1 << 2;

	/**
	 * The dive's tip asked for the drill latch and its response is awaited (D9): set while the driller's drill link is Pending, so a link that
	 * goes back to idle without becoming Active (refused, or the request timed out) reads as the bounce. Cleared by the latch or the bounce
	 */
	constexpr uint8 AwaitingLatch = 1 << 3;

	/**
	 * The latch just ended in a detach pop (crouch released early, D9 part 2): set with the phase back to None on the pop frame, cleared at the
	 * next AfterClocks, like Bounced. The bore ability reads it to apply the pop; nothing else does
	 */
	constexpr uint8 Popped = 1 << 4;

	/**
	 * The latch ended in a pop (PopExit) or a launch (LaunchExit) and the link exit that ends the parked mode is still to come (SD-12). The
	 * link's exit teleports the driller and sets its velocity a few frames later, which overwrote the pop's or the launch's velocity; on the
	 * exit's frame the drill rewrites that exit (its own spot, the pop's or the launch's velocity) and clears the flag
	 * (FGSTN_DrillTickExtension::StampExitImpulse). Cleared too when the link ends without an exit
	 */
	constexpr uint8 PopExit = 1 << 5;
	constexpr uint8 LaunchExit = 1 << 6;
	constexpr uint8 ExitImpulse = PopExit | LaunchExit;

	constexpr uint8 All = CounterClockwise | AirSpin | Bounced | AwaitingLatch | Popped | PopExit | LaunchExit;
}

/**
 * The spin and drill state of one entity as per-entity combat sync state (spin-drill.md 7.1, revised by plan 2b: the latch target,
 * anchor and axis move to the GSTNLink latch, so this fragment keeps only the drill's own data). A sync fragment of the combat
 * extension surface: it rides in the combat sync state, is compared and reconciled with it and restored by rollback. An entity that
 * never spun has no fragment, which costs one bit on the wire with the other fragments absent.
 *
 * Integers and frames only, written by the drill tick extension and nowhere else. Phase timers are frames against stamps
 * (PhaseStartFrame, LatchFrame), so nothing counts down and a re-simulated frame reads the same values. The seat length and the
 * maximum bore time are resolved once at the latch (tuning x the target's response) and stored, because the target's data may
 * change or disappear while latched.
 *
 * Wire format: nothing for a fragment at its defaults (IsDefault; the list framing says 0 bytes), else an 8 bit presence mask and the
 * groups present, plain integers: phase (phase 1, flags 1, start frame 4), air (spins used 1, low gravity until 4), latch (latch frame
 * 4, seat frames 2, max bore frames 2, hits 1). Spinning on the ground 1 + 6 bytes, an air spin 1 + 11, latched 1 + 6 + 9 (+ 5 when the
 * air group is live).
 */
USTRUCT()
struct GSTNSPINDRILLNP_API FGSTN_DrillFragment : public FGSTN_SyncFragment
{
	GENERATED_BODY()

	// ---- Phase

	EGSTN_DrillPhase Phase = EGSTN_DrillPhase::None;

	/** GSTNDrillFlags */
	uint8 Flags = 0;

	/** First frame of the current phase; INDEX_NONE while the phase is None */
	int32 PhaseStartFrame = INDEX_NONE;

	// ---- Air spin

	/** Air spins used in the current airtime; cleared by the refresh events (EGSTN_AirSpinRefresh) */
	uint8 AirSpinsUsed = 0;

	/** Low gravity of the air spin holds before this frame (exclusive); INDEX_NONE = none. Read by the gravity contributor (seam S6, later) */
	int32 LowGravityUntilFrame = INDEX_NONE;

	// ---- Latch

	/** First latched frame, the seat and wobble time origin on every machine; INDEX_NONE while not latched */
	int32 LatchFrame = INDEX_NONE;

	/** Seat length resolved at the latch (tuning x target scale) */
	uint16 SeatFrames = 0;

	/** Longest bore resolved at the latch (target override or tuning) */
	uint16 MaxBoreFrames = 0;

	/** Hits landed on the latched target, saturating at 255 */
	uint8 HitsLanded = 0;

	// ---- Derived reads

	bool IsSpinning() const { return Phase == EGSTN_DrillPhase::Spinning; }
	bool IsClockwise() const { return (Flags & GSTNDrillFlags::CounterClockwise) == 0; }
	bool IsDrilling() const { return Phase == EGSTN_DrillPhase::Diving || Phase == EGSTN_DrillPhase::Seating || Phase == EGSTN_DrillPhase::Boring; }
	bool IsLatched() const { return Phase == EGSTN_DrillPhase::Seating || Phase == EGSTN_DrillPhase::Boring; }
	bool IsLowGravity(int32 Frame) const { return LowGravityUntilFrame != INDEX_NONE && Frame < LowGravityUntilFrame; }

	/** Frames since the phase started (0 on its first frame); 0 while the phase is None */
	int32 FramesInPhase(int32 Frame) const { return PhaseStartFrame == INDEX_NONE ? 0 : Frame - PhaseStartFrame; }

	/** Frames since the latch (0 on the latch frame); 0 while not latched */
	int32 FramesSinceLatch(int32 Frame) const { return LatchFrame == INDEX_NONE ? 0 : Frame - LatchFrame; }

	/** Every field at its default: written as zero bytes */
	bool IsDefault() const;

	bool operator==(const FGSTN_DrillFragment& Other) const;

	virtual void NetSerialize(FArchive& Ar) override;

	/** Exact integer equality of every field: nothing here is a float, so there is no tolerance */
	virtual bool ShouldReconcile(const FGSTN_SyncFragment& Authority) const override;

	/** Discrete state: the copy of the later sample the registry hands over is already the result */
	virtual void Interpolate(const FGSTN_SyncFragment* From, const FGSTN_SyncFragment* To, float PCT) override {}

	/** "Drill Boring@120 cw air 1 lowg 140 latch 100 seat 12 max 90 hits 3" with the groups that are live */
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
};

/**
 * Registers and unregisters FGSTN_DrillFragment under GSTNFeatureIds::SpinDrill. The module does it at startup and shutdown, before the
 * first simulation tick; exposed so a test can restore the registration it changed.
 */
struct GSTNSPINDRILLNP_API FGSTN_DrillFragmentRegistration
{
	/** True when the fragment is registered after the call (registering twice is fine) */
	static bool Register();

	static void Unregister();
};
