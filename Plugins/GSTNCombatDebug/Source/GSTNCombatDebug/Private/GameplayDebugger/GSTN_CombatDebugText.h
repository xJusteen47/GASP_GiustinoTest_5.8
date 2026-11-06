// Text and hit ring of the combat Gameplay Debugger category (combat-m5-editor.md 10.2, plan E15). Pure: no world, no simulation.
#pragma once

#include "CoreMinimal.h"

struct FGSTN_CombatSyncCore;
struct FGSTN_SimEvent;

/** One hit dealt or received, as the category prints it. Built from an outbox event on the authority, never from the sync state */
struct FGSTN_CombatHitRecord
{
	int32 Frame = INDEX_NONE;
	bool bDealt = false;
	/** The other side of the hit: the victim of a hit dealt, the attacker of a hit received ("?" when the actor is gone) */
	FString Other;
	float Damage = 0.f;
	/** The event's tags (hit type, relationship, reaction), leaf names only, to keep the line short */
	FString Tags;

	static FGSTN_CombatHitRecord FromEvent(const FGSTN_SimEvent& Event, bool bDealt);
	FString ToLine() const;
};

/**
 * The last hits of one pawn, newest first. Fixed capacity: a host with many fighters keeps a few hundred bytes per pawn whatever
 * the length of the match
 */
struct FGSTN_CombatHitRing
{
	static constexpr int32 Capacity = 8;

	void Add(FGSTN_CombatHitRecord&& Record);
	int32 Num() const { return Records.Num(); }
	/** Index 0 is the newest */
	const FGSTN_CombatHitRecord& operator[](int32 Index) const { return Records[(Head - 1 - Index + Capacity) % Capacity]; }

private:
	TArray<FGSTN_CombatHitRecord, TInlineAllocator<Capacity>> Records;
	int32 Head = 0;
};

namespace GSTNCombatDebugText
{
	/** Display name of an action state value (the enum has no reflection) */
	FString ActionStateName(uint8 ActionState);

	/**
	 * The combat state lines of the category for the state of Frame: action state, timeline and its frame, hitstop and hitstun remaining,
	 * weapon, loadout and table revision. Frame is the state's own latest input frame, so a proxy shows its own numbers
	 */
	void DescribeCore(const FGSTN_CombatSyncCore& Core, TArray<FString>& OutLines);
}
