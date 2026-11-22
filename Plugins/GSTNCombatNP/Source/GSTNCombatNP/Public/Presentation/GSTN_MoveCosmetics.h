// Combat presentation (CP4 of the Blueprint seams): the cosmetic windows of a move (trails, whooshes, charge glows) and the pure rule that
// turns the presented combat state into "window began / window ended".

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_MoveCosmetics.generated.h"

class UNiagaraSystem;
class USoundBase;

/**
 * One cosmetic window of a move, in timeline frames (the frames of the move's timeline, which stand still in hitstop like the keys do).
 * Pure presentation data: the simulation never reads it, so changing it never changes how a move plays, on any machine.
 *
 * The window is live from StartFrame to EndFrame plus TailFrames (both inclusive). EndFrame -1 keeps it live until the move ends. A window
 * with StartFrame == EndFrame and no tail is a one-shot (a whoosh sound, a flash): it begins once and ends on the next frame.
 *
 * Checklist (owner-approved VFX rules, 2026-11-15): a trail is on for the active frames plus 2-3 frames of tail; it should never hide the
 * striking limb in the frozen hitstop frame; size follows the importance of the move.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATNP_API FGSTN_MoveCosmeticWindow
{
	GENERATED_BODY()

	/** What this window is, for the Blueprint events (On Move Cosmetic Begin/End) and the debugger. Free tag, e.g. Cosmetic.Trail.Fist */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Window", meta = (Categories = "Cosmetic"))
	FGameplayTag CosmeticTag;

	/** First timeline frame of the window */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Window", meta = (ClampMin = "0"))
	int32 StartFrame = 0;

	/** Last timeline frame of the window (inclusive); -1 = until the move ends */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Window", meta = (ClampMin = "-1"))
	int32 EndFrame = 0;

	/** Frames the window stays live after EndFrame (a trail's tail). Ignored with EndFrame -1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Window", meta = (ClampMin = "0"))
	int32 TailFrames = 0;

	/** Niagara system spawned when the window begins and deactivated when it ends (its particles finish). None = no effect */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	TSoftObjectPtr<UNiagaraSystem> Effect;

	/** Sound played once when the window begins (a whoosh, a charge). None = no sound */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	TSoftObjectPtr<USoundBase> Sound;

	/** Socket (or bone) of the character mesh the effect and the sound follow; None = the mesh root */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FName Socket;

	/** Attach to the socket (a trail follows the fist). Off = spawned where the socket is and left in the world (a dust puff) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	bool bAttach = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FVector Scale = FVector::OneVector;

	/** Pause the effect while the character is in hitstop, so the frozen contact frame shows the trail where it was */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	bool bFreezeInHitstop = true;

	/** When the move is cut short (cancelled, replaced, a ricochet, a correction) the effect vanishes at once instead of fading out */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	bool bKillWhenInterrupted = true;

	/** True when timeline frame Frame is inside the window (tail included). A window with EndFrame -1 is live from StartFrame on */
	bool IsLiveAt(int32 Frame) const
	{
		if (Frame < StartFrame)
		{
			return false;
		}
		return EndFrame < 0 || Frame <= FMath::Max(EndFrame, StartFrame) + FMath::Max(TailFrames, 0);
	}
};

/** One window of one run of a move: the registry entry, the simulation frame the run started on and the window index */
struct GSTNCOMBATNP_API FGSTN_MoveCosmeticKey
{
	int32 Entry = INDEX_NONE;
	int32 RunStartFrame = INDEX_NONE;
	int32 Window = INDEX_NONE;

	bool operator==(const FGSTN_MoveCosmeticKey& Other) const
	{
		return Entry == Other.Entry && RunStartFrame == Other.RunStartFrame && Window == Other.Window;
	}
};

/**
 * Follows the presented combat state frame by frame and says which cosmetic windows began and which ended. It is a diff of the presented
 * state, never an event of the simulation, so a rollback or a correction cannot fire anything twice or leave anything on: the frame after a
 * correction simply has other live windows. Two rules make it safe for one-shots:
 *  - a key that began is never begun again while it is remembered (MemorySeconds): a correction that drops a run for a frame and brings it back
 *    does not replay its whoosh (a trail cut by such a flicker stays off; rare, and better than a double sound);
 *  - an end is natural when the window's frames ran out: the same run went past it, or the run ended or was replaced while only the tail
 *    was left (or the window lasts until the move ends). A window cut during its frames (a cancel, a recoil, a correction that took the
 *    frames back) is interrupted.
 * A window that began and ended between two drawn frames (a one-shot when the game draws slower than the simulation) is reported as both
 * began and ended (natural) in the same update: the caller processes OutDelta.Began first, then OutDelta.Ended.
 * Pure: the caller passes the windows of the running move and the time. Presentation only.
 */
class GSTNCOMBATNP_API FGSTN_MoveCosmeticTracker
{
public:
	static constexpr double DefaultMemorySeconds = 2.0;

	struct FEnded
	{
		FGSTN_MoveCosmeticKey Key;
		bool bInterrupted = false;
	};

	struct FDelta
	{
		TArray<FGSTN_MoveCosmeticKey> Began;
		TArray<FEnded> Ended;
	};

	/**
	 * Entry: the timeline registry entry the state runs (INDEX_NONE: no move). RunStartFrame: the simulation frame it started on.
	 * TimelineFrame: the evaluated timeline frame (INDEX_NONE outside a timeline and while a ricochet recoil plays). Windows: the running move's
	 */
	void Update(int32 Entry, int32 RunStartFrame, int32 TimelineFrame, TConstArrayView<FGSTN_MoveCosmeticWindow> Windows, double NowSeconds,
		double MemorySeconds, FDelta& OutDelta);

	/** Ends everything live as interrupted (the component stops, the owner is destroyed) */
	void EndAll(FDelta& OutDelta);

	int32 NumActive() const { return Active.Num(); }
	bool IsActive(const FGSTN_MoveCosmeticKey& Key) const { return Active.ContainsByPredicate([&Key](const FActive& Item) { return Item.Key == Key; }); }
	int32 NumRemembered() const { return Begun.Num(); }
	void Reset() { Active.Reset(); Begun.Reset(); LastEntry = LastRunStartFrame = LastFrame = INDEX_NONE; }

private:
	/** The run and timeline frame of the previous update, to catch the windows crossed between two drawn frames */
	int32 LastEntry = INDEX_NONE;
	int32 LastRunStartFrame = INDEX_NONE;
	int32 LastFrame = INDEX_NONE;

	struct FActive
	{
		FGSTN_MoveCosmeticKey Key;
		int32 StartFrame = 0;
		int32 EndFrame = 0;
	};

	TArray<FActive> Active;
	/** Keys that began, with the time they were last live: never begun again while remembered */
	TArray<TPair<FGSTN_MoveCosmeticKey, double>> Begun;
};
