// Combat abilities (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_MoveEvents.generated.h"

struct FGSTN_CombatTimelineData;

/** Why a move's timeline ended, as On Move Ended reports it (CP1 of the Blueprint seams) */
UENUM(BlueprintType)
enum class EGSTN_MoveEndReason : uint8
{
	/** The timeline played to its last frame */
	Finished,

	/** Cut inside its cancellable tail (a stick move, a jump, a command): only recovery was left */
	TailCancelled,

	/** The move met a hard face and played its ricochet recoil (cut short or not) */
	AfterRecoil,

	/** Anything earlier: a hit taken, a move that replaced it through a cancel window, a forced end */
	Interrupted,
};

/**
 * The rules UGSTN_GA_BaseCombat raises its Blueprint move events with (CP1 of the Blueprint seams), split out so they run in a test
 * without a world or an ability system. The ability keeps the cursors (last evaluated frame, confirm groups already reported) in sync
 * variables, so a rollback restores them with everything else and the events of the re-simulated frames are raised again exactly once.
 */
struct GSTNCOMBATNP_API FGSTN_MoveEventRules
{
	using FKeyList = TArray<int32, TInlineAllocator<4>>;

	/**
	 * Indices in Data.Tracks of the Event tracks whose StartFrame lies in (LastFrame, CurrentFrame], in track order: the same crossing rule
	 * as the timeline player's cues and outbox events, so a key raises once whether frames stand still (hitstop, a hold) or are skipped.
	 * LastFrame INDEX_NONE before the first frame (a key on frame 0 is crossed by the first evaluation). Allocation free for up to 4 keys a frame
	 */
	static void CollectKeys(const FGSTN_CombatTimelineData& Data, int32 LastFrame, int32 CurrentFrame, FKeyList& OutTracks);

	/** The hit groups that confirmed since the last report: bits of Now that Seen did not have */
	static uint8 NewConfirmGroups(uint8 Seen, uint8 Now) { return static_cast<uint8>(Now & ~Seen); }

	/**
	 * Why the move ended. LastFrame is the last timeline frame the ability saw running (the frame before the combat state cleared it on a
	 * natural end); TailStartFrame is INDEX_NONE without a tail
	 */
	static EGSTN_MoveEndReason ResolveEndReason(bool bWasCancelled, bool bRecoilPlayed, int32 LastFrame, int32 TotalFrames, int32 TailStartFrame);
};
