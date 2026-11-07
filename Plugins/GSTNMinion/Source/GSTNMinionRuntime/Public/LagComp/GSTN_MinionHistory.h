// Minion runtime: lag compensation history of the authority batch (M8 G5, spike 5).

#pragma once

#include "CoreMinimal.h"

/**
 * One minion at the end of one batch frame, as the history keeps it for a rewound attack query (combat-m8-minions.md 9.2).
 * Position is the feet (the store's), the hurtboxes hang from it like in the snapshot entry.
 */
struct FGSTN_MinionHistoryRecord
{
	/** The batch frame whose end this is; INDEX_NONE for an empty record */
	int32 Frame = INDEX_NONE;

	FVector3f Position = FVector3f::ZeroVector;
	float YawDegrees = 0.f;

	/**
	 * The swept bound of the frame: the hurt sphere at this record and at the slot's previous record, boxed. A query rejects a candidate
	 * with it before sampling (one box test, no interpolation), so a minion that moved fast during the rewound frame is still found
	 */
	FBox3f SweptBound = FBox3f(ForceInit);

	/** GSTNVictimReactionFlags of the record (intangible, hitstop...) */
	uint8 ReactionFlags = 0;
};

/**
 * The minion side of lag compensation (M8 X4, spike 5): a fixed ring of records per store slot, written by stage 5 for frame F (end of F),
 * and a uniform hash of the current positions on the ground plane for the candidate search. The batch writes it; nothing in the simulation
 * reads it back (it only answers rewound queries), so it holds no state the batch depends on.
 *
 * Why a ring of its own next to the combat world subsystem's (which the minions feed anyway through the provider's snapshot entries):
 * the subsystem's ring is sampled entity by entity for the whole snapshot copy of GSTNLagComp::BuildOverlay, so its cost per rewinding
 * attacker grows with N. This one answers "which minions could the attacker's hitbox have touched at frame R": candidates from the current
 * hash inflated by MaxMinionSpeed x rewind age, rejected by the swept bound of frame R, and only the survivors sampled. The cost then
 * follows the hits, not N.
 *
 * Records are stamped with their frame; a slot that the LOD skipped on a frame has no record for it, and Find walks back to the latest
 * record at or before the asked frame (inside the ring's depth). Plain data, game thread only. Deterministic: every query returns slots in
 * ascending order.
 */
class GSTNMINIONRUNTIME_API FGSTN_MinionHistory
{
public:
	/** Frames kept per slot: 32 at 62.5 Hz is 512 ms, above the 400 ms MaxRewindMs of the standard rules */
	static constexpr int32 DefaultDepth = 32;

	/** Cell edge of the position hash (cm) */
	static constexpr float DefaultCellSize = 400.f;

	/** Sizes the ring for Capacity slots of Depth frames each and empties everything */
	void Reset(int32 Capacity, int32 Depth = DefaultDepth);

	int32 GetDepth() const { return Depth; }
	int32 GetCapacity() const { return Depth > 0 ? Records.Num() / Depth : 0; }

	/** Forgets a slot's records (it was freed or activated: the next occupant must not inherit the old one's past) */
	void ClearSlot(int32 Slot);

	/** Writes the record of Frame for a slot. The swept bound covers the hurt sphere (feet-centred, BoundRadius) here and at the previous record */
	void Write(int32 Slot, int32 Frame, const FVector3f& Position, float YawDegrees, float BoundRadius, uint8 ReactionFlags);

	/** The latest record of the slot at or before Frame, inside the depth; null when there is none */
	const FGSTN_MinionHistoryRecord* Find(int32 Slot, int32 Frame) const;

	/** The oldest record the ring still holds for the slot (SamplePose clamps a frame older than the ring to it); null when none */
	const FGSTN_MinionHistoryRecord* FindOldest(int32 Slot) const;

	/** Rebuilds the position hash from the given slots (ascending) and their current feet positions (indexed by slot) */
	void RebuildHash(TConstArrayView<int32> Slots, TConstArrayView<FVector3f> PositionBySlot, float CellSize = DefaultCellSize);

	/** Slots whose current position lies in a hash cell that Box touches on the ground plane, ascending, no duplicates */
	void QueryHash(const FBox& Box, TArray<int32>& OutSlots) const;

private:
	int32 IndexOf(int32 Slot, int32 Frame) const
	{
		const int32 Wrapped = ((Frame % Depth) + Depth) % Depth;
		return Slot * Depth + Wrapped;
	}

	FIntPoint CellOf(double X, double Y) const
	{
		return FIntPoint(FMath::FloorToInt32(X / HashCellSize), FMath::FloorToInt32(Y / HashCellSize));
	}

	int32 Depth = 0;
	/** [Slot * Depth + Frame % Depth] */
	TArray<FGSTN_MinionHistoryRecord> Records;
	/** Last frame written per slot, INDEX_NONE when none (the previous record of a swept bound) */
	TArray<int32> LastFrame;

	float HashCellSize = DefaultCellSize;
	/** Looked up by key only, never iterated, so its order cannot reach a result */
	TMap<FIntPoint, TArray<int32>> Cells;
};
