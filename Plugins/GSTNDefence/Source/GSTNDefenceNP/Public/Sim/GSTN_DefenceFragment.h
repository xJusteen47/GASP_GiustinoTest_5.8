// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Math/GSTN_DefenceViews.h"
#include "Sync/GSTN_SyncFragment.h"
#include "GSTN_DefenceFragment.generated.h"

/** Bits of FGSTN_DefenceFragment::ShieldFlags */
namespace GSTNShieldFlags
{
	/** The shield is up: pressed and not yet released; the value drains. It blocks from ShieldPressFrame + StartupFrames on */
	constexpr uint8 Raised = 1 << 0;

	/** The shield broke from damage and has not recovered yet (until BreakEndFrame): no bubble, no raise */
	constexpr uint8 Breaking = 1 << 1;

	/** The value reached zero from decay while raised: the hold window ends (a forced drop, never a break). Cleared with Raised */
	constexpr uint8 Empty = 1 << 2;
}

/**
 * The defence state of one entity as per-entity combat sync state (m6 spec 5.1): the analytic shield (a value at a stamp frame, flags,
 * the press and release frames), the parry window of the running move, the stuns as end-frame stamps, the tech, knockdown and grab
 * immunity stamps (written by F8 and F12), the pushback a blocked hit queued, and the bookkeeping of the attribute mirror. A sync fragment
 * of the combat extension surface, so it rides in the combat sync state beside the core, is compared and reconciled with it, and is
 * restored by rollback. An entity that never raised a shield, parried or was punished has no fragment, which costs nothing on the wire.
 *
 * It is simulated state: integers only, written by the outcome applier (the hit's own frame plus one), the punish inbox handler and
 * the tick extension, and nowhere else (never from GAS, never from presentation). There is no action state of its own: a stun is the
 * frame against its end stamp (IsShieldStunned, IsPunished...), a break the Breaking flag, so a window that ran out needs no clearing and
 * the states the spec names (ShieldStun, ShieldBroken, Punished, Knockdown) are derived, not stored (deviation recorded in F7's deltas).
 *
 * Wire format: nothing at all for a fragment at its defaults (IsDefault; the list framing says 0 bytes), else a 16 bit presence mask and
 * the groups that are present, each as plain integers: shield (value 2, stamp 4, flags 1, press 4, release 4), move parry window (start 4,
 * end 4, profile 1, hurtbox mask 2), last parry 4, stuns (shield stun 4, parry recovery 4, break 4), punish (end 4, kind 1), tech (lockout 4,
 * last press 4, end 4, type 1), knockdown 4, grab immunity 4, pushback (start 4, velocity 6, frames 1), mirror (value 2, frame 4). A shield that
 * is only raised costs 2 + 15 bytes; everything live at once 2 + 85.
 */
USTRUCT()
struct GSTNDEFENCENP_API FGSTN_DefenceFragment : public FGSTN_SyncFragment
{
	GENERATED_BODY()

	/** Shield value and mirror quantum: quarter points */
	static constexpr double ValueUnitsPerPoint = 4.0;

	/** Pushback velocity quantum: 4 cm/s per unit, like the pending launch of the hit state */
	static constexpr double VelocityUnitsPerCmPerSec = 0.25;

	// ---- Shield (m6 spec 5.1, 5.3)

	/** The shield value at ShieldStampFrame, in quarter points. Meaningless (the full value) while ShieldStampFrame is INDEX_NONE */
	int16 ShieldValueQ = 0;
	int32 ShieldStampFrame = INDEX_NONE;
	uint8 ShieldFlags = 0;

	/** The frame the shield was last pressed (the start of its timeline); INDEX_NONE = never */
	int32 ShieldPressFrame = INDEX_NONE;

	/** The frame of the last release; INDEX_NONE while raised or never raised. The bubble outlasts it by the rules' ReleaseFrames */
	int32 ShieldReleaseFrame = INDEX_NONE;

	// ---- Move parry window (m6 spec 5.4, source Move)

	/** [MoveParryStartFrame, MoveParryEndFrame) in simulation frames, stamped when the track starts; INDEX_NONE = no window */
	int32 MoveParryStartFrame = INDEX_NONE;
	int32 MoveParryEndFrame = INDEX_NONE;

	/** Index into the rules' ParryProfiles (bits 0..6); bit 7 = the track closes the window on a parry (FGSTN_DefenceStepUtils::CloseOnParryBit) */
	uint8 MoveParryProfile = 0;

	/** Bit i = the window covers hurtbox i of the entity's set (i < 16); 0 = any hurtbox */
	uint16 MoveParryHurtboxMask = 0;

	/** The frame of the last parried hit, both sources: drives the OnParry cancel condition */
	int32 LastParryFrame = INDEX_NONE;

	// ---- Stuns, as end-frame stamps (exclusive)

	int32 ShieldStunEndFrame = INDEX_NONE;
	int32 ParryRecoveryEndFrame = INDEX_NONE;
	int32 BreakEndFrame = INDEX_NONE;

	/** The attacker's stun after a parry (the punish inbox kind) */
	int32 PunishEndFrame = INDEX_NONE;
	uint8 PunishKind = 0;

	// ---- Tech, knockdown, grab immunity (F8, F12 write them; carried from F7 so the wire layout does not move)

	int32 TechLockoutEndFrame = INDEX_NONE;
	int32 LastTechPressFrame = INDEX_NONE;
	int32 TechEndFrame = INDEX_NONE;
	uint8 TechType = 0;
	int32 KnockdownEndFrame = INDEX_NONE;
	int32 GrabImmunityEndFrame = INDEX_NONE;

	// ---- The pushback of a blocked hit: queued by the applier at the hit's frame plus one, consumed by the tick extension on its frame

	/** The frame the push back move starts (the end of the hit's hitstop); INDEX_NONE = none pending */
	int32 PushbackStartFrame = INDEX_NONE;
	int16 PushbackVelocityQ[3] = { 0, 0, 0 };
	uint8 PushbackFrames = 0;

	// ---- The attribute mirror (m6 spec 4): the value the model last wrote into ShieldHealth, and when

	int16 MirrorValueQ = 0;
	int32 MirrorFrame = INDEX_NONE;

	// ---- Derived reads (frames against stamps)

	bool IsRaised() const { return (ShieldFlags & GSTNShieldFlags::Raised) != 0; }
	bool IsBreaking() const { return (ShieldFlags & GSTNShieldFlags::Breaking) != 0; }
	bool IsEmpty() const { return (ShieldFlags & GSTNShieldFlags::Empty) != 0; }
	bool IsShieldStunned(int32 Frame) const { return ShieldStunEndFrame != INDEX_NONE && Frame < ShieldStunEndFrame; }
	bool IsInParryRecovery(int32 Frame) const { return ParryRecoveryEndFrame != INDEX_NONE && Frame < ParryRecoveryEndFrame; }
	bool IsPunished(int32 Frame) const { return PunishEndFrame != INDEX_NONE && Frame < PunishEndFrame; }
	bool IsKnockedDown(int32 Frame) const { return KnockdownEndFrame != INDEX_NONE && Frame < KnockdownEndFrame; }
	bool IsTeching(int32 Frame) const { return TechEndFrame != INDEX_NONE && Frame < TechEndFrame; }
	bool IsTechLockedOut(int32 Frame) const { return TechLockoutEndFrame != INDEX_NONE && Frame < TechLockoutEndFrame; }
	bool IsGrabImmune(int32 Frame) const { return GrabImmunityEndFrame != INDEX_NONE && Frame < GrabImmunityEndFrame; }
	bool HasMoveParryWindow() const { return MoveParryStartFrame != INDEX_NONE && MoveParryEndFrame != INDEX_NONE; }
	bool IsMoveParryWindowOpen(int32 Frame) const { return HasMoveParryWindow() && Frame >= MoveParryStartFrame && Frame < MoveParryEndFrame; }

	/** The stamped shield as the math reads it (FGSTN_ShieldView). A fragment never stamped reads as the full value, stamped at frame 0 */
	FGSTN_ShieldView MakeShieldView(const FGSTN_ShieldRates& Rates) const;

	/** Writes a shield view back: value, stamp, Raised, press and release frames */
	void SetShieldView(const FGSTN_ShieldView& View);

	/** The pending pushback as a velocity, cm/s */
	FVector GetPushbackVelocity() const;
	void SetPushback(int32 StartFrame, const FVector& Velocity, int32 Frames);
	void ClearPushback();

	static int16 QuantizeValue(double Value);
	static double DequantizeValue(int16 ValueQ) { return static_cast<double>(ValueQ) / ValueUnitsPerPoint; }

	/** Every field at its default: written as zero bytes */
	bool IsDefault() const;

	bool operator==(const FGSTN_DefenceFragment& Other) const;

	virtual void NetSerialize(FArchive& Ar) override;

	/** Exact integer equality of every field: nothing here is a float, so there is no tolerance */
	virtual bool ShouldReconcile(const FGSTN_SyncFragment& Authority) const override;

	/** Discrete state: the copy of the later sample the registry hands over is already the result */
	virtual void Interpolate(const FGSTN_SyncFragment* From, const FGSTN_SyncFragment* To, float PCT) override {}

	/** "Defence shield 25.50@120 flags 0x1 press 100 release -1 ..." with the groups that are live */
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
};
