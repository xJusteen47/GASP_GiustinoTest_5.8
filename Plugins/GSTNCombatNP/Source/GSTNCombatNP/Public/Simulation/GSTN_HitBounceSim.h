// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Simulation/GSTN_CombatTickExtension.h"
#include "Sync/GSTN_SyncFragment.h"
#include "Timeline/GSTN_HitBounce.h"
#include "GSTN_HitBounceSim.generated.h"

struct FGSTN_AttackerHit;
struct FGSTN_CombatSyncCore;

/**
 * The hit bounce of one entity (owner 2026-11-23, FGSTN_TimelineTrack_HitBounce: the Cr.L Aerial stomp bounces off what it hits) as combat
 * sync state: rolled back and re-simulated with the rest of the combat state, so a predicted bounce that the authority did not make is undone
 * by the reconcile like any other predicted state. Integers and frames only, written by FGSTN_HitBounceTickExtension and nowhere else.
 * Timers are stamps (ArmedFrame, LaunchFrame), so nothing counts down and a re-simulated frame reads the same values.
 *
 * Lifecycle: a bouncing hit inside the track's window ARMS it (the hit's frame and the timeline it belongs to); the first frame after the
 * attacker's hitstop it FIRES (the move ends or hands over to its bounce move, the launch is queued on Mover, Count goes up, the lock starts at
 * LaunchFrame); the lock ends LockFrames later. Count is the bounces in a row: back to 0 on the ground or in a hit reaction. A fragment with
 * nothing live is removed, so an entity that is not bouncing pays nothing on the wire.
 *
 * Wire format: nothing for a fragment at its defaults, else an 8 bit presence mask and the groups present: count (1 byte), armed (hit frame 4,
 * timeline start 4), launch (frame 4, lock frames 1).
 */
USTRUCT()
struct GSTNCOMBATNP_API FGSTN_HitBounceFragment : public FGSTN_SyncFragment
{
	GENERATED_BODY()

	/** Bounces in a row since the entity last stood on the ground or was hit */
	uint8 Count = 0;

	/** The frame of the bouncing hit while the bounce waits for the end of the hitstop; INDEX_NONE when nothing is armed */
	int32 ArmedFrame = INDEX_NONE;

	/** The start frame of the timeline the hit belongs to: a timeline that ended or was replaced before the bounce fires disarms it */
	int32 ArmedTimelineStart = INDEX_NONE;

	/** The launch frame of the bounce under way; INDEX_NONE once its lock is over */
	int32 LaunchFrame = INDEX_NONE;

	/** Frames from LaunchFrame the entity cannot act (the tuning's ActionableFrames when it fired, kept so a change of the tuning mid-flight does not move it) */
	uint8 LockFrames = 0;

	bool IsArmed() const { return ArmedFrame != INDEX_NONE; }
	bool IsLocked(int32 Frame) const { return LaunchFrame != INDEX_NONE && Frame >= LaunchFrame && Frame < LaunchFrame + LockFrames; }

	/** Every field at its default: written as zero bytes */
	bool IsDefault() const;

	bool operator==(const FGSTN_HitBounceFragment& Other) const;

	virtual void NetSerialize(FArchive& Ar) override;

	/** Exact integer equality: nothing here is a float */
	virtual bool ShouldReconcile(const FGSTN_SyncFragment& Authority) const override;

	virtual void Interpolate(const FGSTN_SyncFragment* From, const FGSTN_SyncFragment* To, float PCT) override {}

	/** "HitBounce n 1 armed 120/100 launch 131+24" with the groups that are live */
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
};

/** Registers FGSTN_HitBounceFragment under GSTNFeatureIds::HitBounce (the module does it at startup; a test can restore it) */
struct GSTNCOMBATNP_API FGSTN_HitBounceFragmentRegistration
{
	static bool Register();
	static void Unregister();
};

/** What the bounce asks of the movement when it fires: read from Mover's state at the end of the previous frame */
struct FGSTN_HitBounceMoverFrame
{
	/** The entity's facing (any length; flattened on the plane of up by the launch maths) */
	FVector Facing = FVector::ForwardVector;
	/** Size of the movement mode's gravity, cm/s^2 */
	double Gravity = 980.0;
	/** The entity has a Mover to launch */
	bool bValid = false;
};

/**
 * The hit bounce inside the combat tick. Priority 140: after the parts (100), the defence (110), the links (120) and the drill (130), so a
 * hit reaction or a link those wrote this frame is already in the state it reads.
 *
 *  - AfterClocks: a hit reaction (or a KO) resets everything; the lock that ran out is cleared; an armed bounce whose timeline is gone is dropped,
 *    and one whose hitstop is over fires: the move's bounce move starts Forced (when the kit grants it) or the move is cancelled, the launch
 *    is queued on Mover (UGSTN_GE_Knockback: the up and back speed, the rise's gravity scale, RiseFrames long), Count goes up and the lock
 *    starts. Fired one frame AFTER the hitstop window, like a victim's launch: the freeze finishes on the first frame outside the window and
 *    Mover hands its finish velocity back over a move started on that frame. Then the ground resets Count (not while armed or on the two
 *    frames after a launch, which may still read the ground of before).
 *  - AfterExecute: a hit of this frame that bounces (FGSTN_HitBounceRules::IsBouncingHit), made by the running timeline inside its Hit Bounce
 *    track's window while Count is under the maximum, arms the bounce. At the maximum nothing happens: the move plays on as a normal move.
 *  - BlocksActions: true while the lock runs (no command starts; movement input still steers the air drift).
 *
 * Every number comes from the track with the gstn.Combat.Debug.HitBounce* console overrides applied (ReadOverrides). Those are simulation
 * inputs: in a networked session every machine must hold the same values (leave them at -1 to use the data).
 * Simulation code: deterministic, no net mode, no resimulation branch, state only in the fragment.
 */
class GSTNCOMBATNP_API FGSTN_HitBounceTickExtension : public IGSTN_CombatTickExtension
{
public:
	static constexpr int32 ExtensionPriority = 140;

	/** The fixed simulation step (62.5 Hz) the frame counts of the track are written in */
	static constexpr double FrameSeconds = 0.016;

	virtual ~FGSTN_HitBounceTickExtension() = default;

	virtual uint8 GetTypeId() const override { return GSTNFeatureIds::HitBounce; }
	virtual int32 GetPriority() const override { return ExtensionPriority; }
	virtual void OnCombatHook(EGSTN_CombatHookPoint Point, FGSTN_CombatTickContext& Context) override;
	virtual bool BlocksActions(const FGSTN_CombatTickContext& Context) const override;

	/** The console overrides of this machine (gstn.Combat.Debug.HitBounce*) */
	static FGSTN_HitBounceOverrides ReadOverrides();

	// ---- The rules, pure (tests drive them directly)

	/** The first hit of Hits that bounces under Tuning; INDEX_NONE when none does */
	static int32 FindBouncingHit(TConstArrayView<FGSTN_AttackerHit> Hits, const FGSTN_HitBounceTuning& Tuning);

	/** The timeline frame Core evaluates on Frame (the attacker step's count: frames since the start minus the paused ones) */
	static int32 TimelineFrameOf(const FGSTN_CombatSyncCore& Core, int32 Frame);

	/**
	 * Writes the arm into Fragment when the hit is inside Track's window, Core runs the timeline that made it (not one started on this frame, not
	 * its recoil) and Count is under the maximum. True when it armed
	 */
	static bool TryArm(FGSTN_HitBounceFragment& Fragment, const FGSTN_CombatSyncCore& Core, int32 Frame, const FGSTN_TimelineTrack_HitBounce& Track,
		const FGSTN_HitBounceTuning& Tuning, bool bHasBouncingHit);

	/** What AfterClocks decides for an armed fragment */
	enum class EArmedAction : uint8
	{
		None,
		/** Still in the hitstop (or on the frame the freeze finishes) */
		Wait,
		/** The timeline of the hit ended or was replaced: drop the arm */
		Disarm,
		Fire
	};
	static EArmedAction ResolveArmed(const FGSTN_HitBounceFragment& Fragment, const FGSTN_CombatSyncCore& Core, int32 Frame);

	/**
	 * The bookkeeping of the frame before the arm is looked at: a hit reaction or KO resets the fragment, the lock that ran out is cleared, the
	 * ground resets Count (bGrounded = the entity stood on a floor at the end of the previous frame)
	 */
	static void StepBookkeeping(FGSTN_HitBounceFragment& Fragment, int32 Frame, bool bInReactionOrKO, bool bGrounded);

	/** The fragment after a bounce fired on Frame */
	static void MarkLaunched(FGSTN_HitBounceFragment& Fragment, int32 Frame, const FGSTN_HitBounceTuning& Tuning);

protected:
	/** Mover's facing and gravity at the end of the previous frame. Virtual so a test without a world can hand its own */
	virtual FGSTN_HitBounceMoverFrame ReadMoverFrame(const FGSTN_CombatTickContext& Context) const;

	/** Queues the launch on the entity's Mover through the core's knockback move effect. Virtual so a test can record it */
	virtual void ApplyLaunch(FGSTN_CombatTickContext& Context, const FGSTN_HitBounceLaunch& Launch) const;

	/** The overrides the hooks use; the console's by default, a test can fix them */
	virtual FGSTN_HitBounceOverrides GetOverrides() const { return ReadOverrides(); }

private:
	void Fire(FGSTN_CombatTickContext& Context, FGSTN_HitBounceFragment& Fragment) const;
};
