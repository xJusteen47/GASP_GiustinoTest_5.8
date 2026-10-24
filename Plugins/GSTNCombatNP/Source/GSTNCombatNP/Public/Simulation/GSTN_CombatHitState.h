// Combat simulation (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "Misc/StringBuilder.h"

class FArchive;

/**
 * What the hit part of the sync state needs to know about the frame it belongs to, to tell a live field from an expired one
 * (combat-m4-hits 11, "end-frame stamps"). Taken from the core of the same state: see FGSTN_CombatSyncCore::GetHitContext.
 */
struct FGSTN_HitLiveContext
{
	/** The frame the state is the end of (the latest frame of the input history) */
	int32 Frame = INDEX_NONE;

	/** The state says a timeline is running (EGSTN_ActionState::InTimeline) */
	bool bInTimeline = false;

	int32 TimelineStartFrame = INDEX_NONE;

	/** The core's hitstop window end: the launch and the reaction chosen by a hit are pending until it */
	int32 HitstopEndFrame = INDEX_NONE;
};

/** One victim the running timeline has already hit with a hit group (the multi-hit rule of the attacker side, M4 H8) */
struct FGSTN_HitTarget
{
	int32 EntityId = INDEX_NONE;

	/** The timeline run the hit belongs to: a target of an earlier run is dead the moment a new timeline starts */
	int32 TimelineStartFrame = INDEX_NONE;

	uint8 HitGroup = 0;

	bool operator==(const FGSTN_HitTarget& Other) const
	{
		return EntityId == Other.EntityId && TimelineStartFrame == Other.TimelineStartFrame && HitGroup == Other.HitGroup;
	}
};

/**
 * The world transform a move's root motion and hitbox keys are relative to, quantised so equal states are equal bits: position in
 * 1/10 cm, each Euler angle in 1/65536 of a turn. Compared as integers, never as floats.
 */
struct FGSTN_MoveOrigin
{
	static constexpr double PositionUnitsPerCm = 10.0;
	static constexpr double RotationUnitsPerTurn = 65536.0;

	bool bSet = false;
	int32 PositionQ[3] = { 0, 0, 0 };

	/** Pitch, yaw, roll */
	uint16 RotationQ[3] = { 0, 0, 0 };

	static FGSTN_MoveOrigin Make(const FTransform& Transform);
	FTransform ToTransform() const;

	bool operator==(const FGSTN_MoveOrigin& Other) const;
};

/**
 * The hit part of the combat sync state (combat-m4-hits 11): the attacker's bookkeeping of the move it runs and the victim's
 * reaction windows. The hitstop window itself stays in the core (it existed already, now written by hits).
 *
 * Idle cost is nothing. Every field carries or implies an end frame, and Canonical drops the fields whose frame is past in the
 * state's own frame: that is the form that goes on the wire and that ShouldReconcile compares, so a combatant that was hit a
 * second ago equals one that never was, and the combat tick never has to clear a window that ran out. The hit part is
 * serialized inside the action part of the core, which makes a combatant with nothing live pay no bit for it at all.
 *
 * Wire format (inside the action part, after the requests): 1 bit "hit part live"; then 7 bits of presence (move origin, targets,
 * confirm flags, hitstun end, impact end, pending launch, SDI), 2 bits of pending reaction, and the blocks that are present.
 */
struct FGSTN_HitSyncState
{
	static constexpr int32 MaxHitTargets = 8;

	/** Written by the attacker side when a move starts; live while a timeline runs */
	FGSTN_MoveOrigin MoveOrigin;

	/** Ring of the victims the running timeline has hit, filled in order; overflow replaces the oldest. Use the accessors */
	FGSTN_HitTarget HitTargets[MaxHitTargets];
	uint8 HitTargetsHead = 0;
	uint8 HitTargetsNum = 0;

	/** One bit per hit group the running timeline has confirmed a hit with (the gate of cancel routes that need a hit) */
	uint8 HitConfirmFlags = 0;

	/** Hitstun lasts until this frame (exclusive); live while it is in the future */
	int32 HitstunEndFrame = INDEX_NONE;

	/** The wall splat or other surface impact reaction lasts until this frame (exclusive) */
	int32 ImpactEndFrame = INDEX_NONE;

	/** The launch velocity chosen by the hit, in cm/s divided by 4, applied when the hitstop ends. State at F + 1 like the reaction */
	int16 PendingLaunchQ[3] = { 0, 0, 0 };

	/** 0 = none, otherwise EGSTN_HitReaction + 1. Two bits on the wire */
	uint8 PendingReaction = 0;

	/** Reserved for the SDI of M6; nothing writes it in M4 */
	uint8 SDICount = 0;

	// ---- Accessors

	bool HasPendingReaction() const { return PendingReaction != 0; }
	EGSTN_HitReaction GetPendingReaction() const { return static_cast<EGSTN_HitReaction>(FMath::Max<int32>(static_cast<int32>(PendingReaction) - 1, 0)); }
	void SetPendingReaction(EGSTN_HitReaction Reaction) { PendingReaction = static_cast<uint8>(static_cast<uint8>(Reaction) + 1); }
	void ClearPendingReaction() { PendingReaction = 0; PendingLaunchQ[0] = PendingLaunchQ[1] = PendingLaunchQ[2] = 0; }

	/** Quantises to 4 cm/s and clamps to the int16 range (about 1310 m/s) */
	void SetPendingLaunch(const FVector& Velocity);
	FVector GetPendingLaunch() const;

	/** Adds a target at the head of the ring; with the ring full the oldest is replaced. @return true when one was replaced */
	bool AddHitTarget(int32 EntityId, int32 TimelineStartFrame, uint8 HitGroup);
	bool HasHitTarget(int32 EntityId, int32 TimelineStartFrame, uint8 HitGroup) const;
	int32 NumHitTargets() const { return HitTargetsNum; }

	/** Target number AgeIndex, 0 = the oldest */
	const FGSTN_HitTarget& GetHitTarget(int32 AgeIndex) const;

	// ---- Wire, reconcile, blend

	/** The same state with every field that is dead in Context reset and the ring packed oldest first from slot 0 */
	FGSTN_HitSyncState Canonical(const FGSTN_HitLiveContext& Context) const;

	bool IsDefault(const FGSTN_HitLiveContext& Context) const;

	/** Logical equality: the ring compares oldest first, whatever the slot the head is on */
	bool operator==(const FGSTN_HitSyncState& Other) const;

	/** Saving writes the canonical form; loading reads it (Context is the one of the loaded core, which already holds the timeline start the targets share) */
	void NetSerialize(FArchive& Ar, const FGSTN_HitLiveContext& Context);

	/**
	 * The state an interpolated proxy shows: To, with the move origin blended between From and To when both hold one (position
	 * and angles blended on the quantised values, the angles by the shortest way). Everything else is discrete
	 */
	static FGSTN_HitSyncState Interpolated(const FGSTN_HitSyncState* From, const FGSTN_HitSyncState& To, float PCT);

	void ToString(FAnsiStringBuilderBase& Out) const;
};
