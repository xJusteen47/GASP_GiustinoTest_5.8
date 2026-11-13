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
 * The grace rule for a predicted hit whose key vanishes from the attacker's target ring. An NP correction can resimulate the move and drop a
 * key for a frame or two before the resim adds it back with the same key (G7b: it vanished 16 ms after the prediction and returned 40 ms
 * later), which is not a missed hit. So a vanished key is a cancel only after it stayed away for the grace; a key that comes back is the
 * same hit (no cancel, no second prediction); keys not seen for the memory time are forgotten (their move is long over).
 * Shared by the pawn component and the minion presentation subsystem. Pure; the caller passes the times. Presentation only.
 */
class GSTNCOMBATNP_API FGSTN_HitPredictionGrace
{
public:
	static constexpr double DefaultCancelGraceSeconds = 0.25;
	static constexpr double DefaultMemorySeconds = 2.0;

	enum class EStart : uint8
	{
		/** Not predicted before: show the hit */
		New,
		/** Already predicted and kept (its key came back): no second prediction */
		Returned
	};

	/** A key appeared in the ring. A key pending its cancel is rescued */
	EStart NoteStarted(const FGSTN_HitPresentationKey& Key, double NowSeconds);

	/** A key vanished from the ring. True when a grace started now (false for an unknown key or one already vanished) */
	bool NoteVanished(const FGSTN_HitPresentationKey& Key, double NowSeconds, double GraceSeconds);

	/** Call every presented frame, before reading the ring: appends the keys whose grace ran out (real cancels, forgotten here) to OutCancelled */
	void Update(double NowSeconds, double MemorySeconds, TArray<FGSTN_HitPresentationKey>& OutCancelled);

	bool IsKnown(const FGSTN_HitPresentationKey& Key) const { return Find(Key) != INDEX_NONE; }
	int32 Num() const { return Entries.Num(); }
	void Reset() { Entries.Reset(); }

private:
	struct FEntry
	{
		FGSTN_HitPresentationKey Key;
		double SeenSeconds = 0.0;
		/** >= 0: the key vanished and is cancelled at this time unless it comes back */
		double CancelAtSeconds = -1.0;
	};

	int32 Find(const FGSTN_HitPresentationKey& Key) const;

	TArray<FEntry> Entries;
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
