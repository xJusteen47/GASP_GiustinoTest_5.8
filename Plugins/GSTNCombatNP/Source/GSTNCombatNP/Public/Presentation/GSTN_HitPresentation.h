// Combat simulation (GAS-NPP fork): the pure parts of the attacker's own hit presentation (combat-m4-hits 9b, M4 H10).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "Simulation/GSTN_CombatHitState.h"

/**
 * What the attacker's tick predicted about one hit, kept next to the simulation for the presentation component of the same machine. Local
 * data written by every tick and re-simulation, never part of the state and never read back by the simulation.
 */
struct GSTNCOMBATNP_API FGSTN_PredictedHitView
{
	/** The frame of the attacker's tick that made the hit */
	int32 Frame = INDEX_NONE;
	int32 VictimId = INDEX_NONE;
	uint8 HitGroup = 0;

	/** Timeline slot and hitbox index: with the two actors they identify the hit for the cue dedupe */
	int32 Timeline = INDEX_NONE;
	int32 HitboxIndex = INDEX_NONE;

	bool bRicochet = false;
	int32 AttackerHitstopFrames = 0;

	/** The knockback of the attacker's pipeline: tier, hitstun frames, hitstop and launch velocity without the victim's DI */
	bool bHasKnockback = false;
	FGSTN_KnockbackResult Knockback;

	FGameplayTag CueTag;
	FVector HitLocation = FVector::ZeroVector;
};

/** One victim the running move has hit with a hit group: the identity of a hit in the sync state's target ring */
struct GSTNCOMBATNP_API FGSTN_HitPresentationKey
{
	int32 VictimId = INDEX_NONE;
	int32 TimelineStartFrame = INDEX_NONE;
	uint8 HitGroup = 0;

	bool operator==(const FGSTN_HitPresentationKey& Other) const
	{
		return VictimId == Other.VictimId && TimelineStartFrame == Other.TimelineStartFrame && HitGroup == Other.HitGroup;
	}
};

/**
 * Follows the target ring of the attacker's state frame by frame and says which hits are new and which vanished. A new entry is a predicted
 * hit to show; an entry that vanishes while the same move still runs is a hit the authority did not confirm (a correction), whose effects are
 * cancelled. A move that ended or was replaced takes its targets with it: that is the end of the hit, not a correction.
 */
class GSTNCOMBATNP_API FGSTN_HitPresentationTracker
{
public:
	struct FDelta
	{
		TArray<FGSTN_HitPresentationKey> Started;
		TArray<FGSTN_HitPresentationKey> Cancelled;
	};

	/** bRunLive: a timeline runs in the state; RunStartFrame: its start frame (the targets of other runs are dead) */
	void Update(const FGSTN_HitSyncState& Hit, bool bRunLive, int32 RunStartFrame, FDelta& OutDelta);

	const TArray<FGSTN_HitPresentationKey>& GetActive() const { return Active; }
	void Reset();

private:
	TArray<FGSTN_HitPresentationKey> Active;
	int32 RunStart = INDEX_NONE;
};

/**
 * Remembers the cues the attacker's machine played for a predicted hit, so the real cue of the same hit (executed by the authority, or by
 * the simulation of the victim) is played once, not twice. Keyed by the hit id; entries are forgotten after MaxAgeFrames.
 */
class GSTNCOMBATNP_API FGSTN_HitCueDedup
{
public:
	static constexpr int32 MaxAgeFrames = 180;

	void NoteLocal(uint32 HitId, int32 Frame);

	/** The prediction of this hit never stood: the real cue is the only one */
	void Forget(uint32 HitId);

	/** True when a local cue was played for HitId (the real one must be skipped); the record is used up */
	bool ConsumeLocal(uint32 HitId);

	void Prune(int32 Frame);
	int32 Num() const { return Played.Num(); }

private:
	TArray<TPair<uint32, int32>> Played;
};

namespace GSTNHitPresentation
{
	/** Stable for one hit on one machine: the attacker and victim object ids, the attacker's timeline slot and the hitbox */
	GSTNCOMBATNP_API uint32 MakeHitId(uint32 AttackerUniqueId, uint32 VictimUniqueId, int32 Timeline, int32 HitboxIndex);
}
