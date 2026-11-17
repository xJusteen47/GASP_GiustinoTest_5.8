// Presentation events and their effects (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_PresentationTypes.h"
#include "GSTN_PresentationLogic.generated.h"

/** The run skid's phase as the presentation sees it (mirrors the simulation's enum, so this plugin never names the movement code) */
UENUM(BlueprintType)
enum class EGSTN_PresentationSkidPhase : uint8
{
	None,
	Enter,
	Loop,
	Exit
};

/**
 * What the derivation reads of one presented frame of a character. An adapter fills it on the game thread from the synchronized
 * state (Mover sync state, combat state, ability system tags) of whatever machine shows the character: the owner, the server, a proxy.
 * Every field is a copy: the derivation never touches the simulation.
 */
USTRUCT(BlueprintType)
struct GSTNPRESENTATIONCORE_API FGSTN_PresentationSnapshot
{
	GENERATED_BODY()

	/** False = no state yet (nothing is derived, the next valid frame only becomes the reference) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	bool bValid = false;

	/** Simulation frame of the state (INDEX_NONE = unknown) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	int32 Frame = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	FVector Location = FVector::ZeroVector;

	/** cm/s, world */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	FVector Velocity = FVector::ZeroVector;

	/** Up axis of the character (gravity's opposite) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	FVector Up = FVector::UpVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	bool bGrounded = true;

	/** Air jumps spent since the last landing (a rise of it is an air jump) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	int32 AirJumpsUsed = 0;

	/** 0 = not running, 1.. = run stage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	int32 RunStage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	EGSTN_PresentationSkidPhase SkidPhase = EGSTN_PresentationSkidPhase::None;

	/** Reacting to a hit (hitstun or tumble): landings become bounces, wall contacts become wall bounces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	bool bReacting = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	bool bSplat = false;

	/** Frame of the last hit received (stamped by the simulation), INDEX_NONE before the first */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	int32 LastHitFrame = INDEX_NONE;

	/** Its strength (hitstun frames) and push direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	float LastHitStrength = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	FVector LastHitDirection = FVector::ZeroVector;

	/** The tags of the running move (its ability's asset tags) and the frame it started */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	FGameplayTagContainer MoveTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	int32 MoveStartFrame = INDEX_NONE;

	/** State tags the character owns this frame (ability system owned tags: predicted, restored by a resimulation) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Snapshot")
	FGameplayTagContainer StateTags;
};

/** Emit an event when a tag appears or goes away (state tags, or the running move's tags) */
USTRUCT(BlueprintType)
struct GSTNPRESENTATIONCORE_API FGSTN_PresentationTagRule
{
	GENERATED_BODY()

	/** Watched tag (exact or any child): e.g. Combat.State.Shielding */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FGameplayTag WatchedTag;

	/** Look in the running move's tags (a move started: frame = its start frame, stable across resimulations) instead of the state tags */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	bool bMoveTag = false;

	/** Emitted when the tag appears (none = nothing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule", meta = (Categories = "Presentation.Event"))
	FGameplayTag OnAdded;

	/** Emitted when it goes away (none = nothing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule", meta = (Categories = "Presentation.Event"))
	FGameplayTag OnRemoved;

	/**
	 * The watched tag counts only while this state tag is absent (none = always counts). The tumble trail waits out the hitstop: the
	 * tumble tag is granted on the hit frame, but the victim stays frozen until the hitstop ends, so the effect starts at the launch.
	 * A hitstop inside the state (a second hit) ends the effect and starts it again after the freeze
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rule")
	FGameplayTag HeldWhileTag;
};

/** Thresholds of the derivation (component data; the defaults fit the 62.5 Hz fighters of this project) */
USTRUCT(BlueprintType)
struct GSTNPRESENTATIONCORE_API FGSTN_PresentationDeriveRules
{
	GENERATED_BODY()

	/** Leaving the ground with at least this upward speed is a jump (below: walking off a ledge, nothing) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Derive", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float JumpMinUpSpeed = 100.f;

	/** A landing slower than this makes no Land event (stepping down a stair) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Derive", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float LandMinFallSpeed = 150.f;

	/** A reacting victim moving into a surface faster than this and rebounding is a bounce */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Derive", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float BounceMinSpeed = 200.f;

	/** Running again within this many frames after a skid, against the skid's direction (dot below -0.5), is a Turn180 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Derive", meta = (ClampMin = "1"))
	int32 TurnWindowFrames = 30;

	/**
	 * No jump or landing while the character owns one of these state tags: the downed landing's small bounce is not a jump (review r2: a
	 * jump puff on every knockdown). Filled by MakeDefaultQuietTags
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Derive")
	FGameplayTagContainer QuietAirTags;

	/** This project's quiet tags (Combat.State.Knockdown, by name) */
	static FGameplayTagContainer MakeDefaultQuietTags();

	/** Tag rules (shield, knockdown, dodge... whatever state or move tags the game's abilities carry) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Derive")
	TArray<FGSTN_PresentationTagRule> TagRules;

	/** The rules for this project's tags (Combat.State.*, by name: a missing plugin leaves its rule inert) */
	static TArray<FGSTN_PresentationTagRule> MakeDefaultTagRules();
};

/**
 * Turns two consecutive presented snapshots into events. Pure but stateful (it remembers the skid's direction for Turn180 and the
 * previous snapshot). Edge detection on the presented state: a correction can show an edge twice, which the dedupe absorbs.
 */
struct GSTNPRESENTATIONCORE_API FGSTN_PresentationDeriver
{
	/** Appends this frame's events to Out. The first valid snapshot only becomes the reference */
	void Step(const FGSTN_PresentationSnapshot& Current, const FGSTN_PresentationDeriveRules& Rules, TArray<FGSTN_PresentationEvent>& Out);

	void Reset() { *this = FGSTN_PresentationDeriver(); }

	const FGSTN_PresentationSnapshot& GetPrevious() const { return Previous; }

private:
	FGSTN_PresentationSnapshot Previous;
	FVector SkidDirection = FVector::ZeroVector;
	int32 SkidExitFrame = INDEX_NONE;
	int32 FramesSinceSkid = -1;
};

/**
 * The rollback guard: an event is accepted once per (tag, frame). A resimulation that shows the same edge again, at the same frame or a
 * frame or two away (FrameWindow) because the correction moved it, is dropped. Events without a frame are always accepted.
 */
struct GSTNPRESENTATIONCORE_API FGSTN_PresentationDedupe
{
	/** Frames either side that still count as the same event (0 = exact frame only) */
	int32 FrameWindow = 2;

	/** Entries older than this many frames behind the newest are forgotten */
	int32 MemoryFrames = 120;

	/** True = play it (and remember it). bExactFrame: the frame is the simulation's own stamp, so only that frame matches */
	bool Accept(const FGameplayTag& Tag, int32 Frame, bool bExactFrame = false);

	void Reset() { Seen.Reset(); NewestFrame = INDEX_NONE; }

	int32 Num() const { return Seen.Num(); }

private:
	struct FEntry
	{
		FGameplayTag Tag;
		int32 Frame = INDEX_NONE;
	};
	TArray<FEntry> Seen;
	int32 NewestFrame = INDEX_NONE;
};
