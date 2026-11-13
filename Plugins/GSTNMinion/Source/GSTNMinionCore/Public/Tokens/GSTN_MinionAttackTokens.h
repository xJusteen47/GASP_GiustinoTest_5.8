// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"

/**
 * Attack tokens: at most MaxPerTarget minions hold a token to START an attack against a given target at a time (combat-m8-minions.md
 * 9, 9b); the others circle or wait. This bounds how many minion hits can reach one player in a frame, and with it the
 * corrections that player's client takes (plan risk: many hits cause many corrections).
 *
 * Requests are collected during the AI stage and resolved once per tick by Resolve(), which grants in ascending slot index
 * order. The grant order therefore depends only on the set of requesters, never on the order they asked in or on which
 * machine runs it. A holder keeps its token until it releases (attack finished, or it died or was hit: the batch calls
 * ReleaseSlot); a later requester with a lower index does not preempt a holder.
 *
 * Targets are the pipeline entity ids (GSTNEntityKinds); holders are store slot indices, so the batch must release a slot
 * before it is recycled. Lowering the cap does not revoke tokens already held: they drain by release. Plain data, no world, no
 * clock; sized for a few dozen entries so entries live in one array kept sorted by (target, slot).
 */
class GSTNMINIONCORE_API FGSTN_MinionAttackTokens
{
public:
	/** Default of MaxSimultaneousAttackersPerPlayer (owner question 5, hypothesis 4) */
	static constexpr int32 DefaultMaxPerTarget = 4;

	/** Changes the cap (at least 1). Tokens already held above a lowered cap stay until released */
	void SetMaxPerTarget(int32 InMaxPerTarget);
	int32 GetMaxPerTarget() const { return MaxPerTarget; }

	/**
	 * Registers a request of a slot for a target. Idempotent: asking again while waiting or holding changes nothing. A slot may
	 * wait on several targets; the batch decides which request it keeps (normally one, its current target).
	 */
	void Request(int32 TargetId, int32 SlotIndex);

	/** Grants waiting requests up to the cap of each target, lowest slot index first. Returns how many tokens were granted now */
	int32 Resolve();

	/** True when the slot holds a token for the target (granted by Resolve, not merely requested) */
	bool Holds(int32 TargetId, int32 SlotIndex) const;

	/** True when the slot holds a granted token for any target (the G11 T0 cap never demotes an attacker). Linear in the entries, which are few */
	bool HoldsAny(int32 SlotIndex) const;

	/** True when the slot asked for the target and has not been granted yet */
	bool IsWaiting(int32 TargetId, int32 SlotIndex) const;

	/** Gives back the token or cancels the wait of one (target, slot) pair. Returns false if there was none */
	bool Release(int32 TargetId, int32 SlotIndex);

	/** Gives back every token and wait of a slot (it died, was hit or was despawned). Returns how many entries were removed */
	int32 ReleaseSlot(int32 SlotIndex);

	/** Drops every entry of a target (it died or left). Returns how many entries were removed */
	int32 ReleaseTarget(int32 TargetId);

	/** Tokens currently held against a target */
	int32 NumHolders(int32 TargetId) const;

	/** Requests of a target still waiting */
	int32 NumWaiting(int32 TargetId) const;

	int32 NumEntries() const { return Entries.Num(); }

	void Reset() { Entries.Reset(); }

private:
	struct FEntry
	{
		int32 TargetId = INDEX_NONE;
		int32 SlotIndex = INDEX_NONE;
		bool bGranted = false;
	};

	int32 FindEntry(int32 TargetId, int32 SlotIndex) const;

	/** Sorted by (TargetId, SlotIndex): the grant order is the array order */
	TArray<FEntry> Entries;
	int32 MaxPerTarget = DefaultMaxPerTarget;
};
