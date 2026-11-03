// Interaction focus: pure data types (interaction.md §9). No GAS, no Network Prediction, no Mover.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_InteractTypes.generated.h"

/** How a candidate is checked for line of sight (§2.4) */
UENUM(BlueprintType)
enum class EGSTN_InteractLosPolicy : uint8
{
	/** Default for ground pickups and entities: short reach, arc and height band already exclude other floors */
	None,
	/** One segment to the anchor, blocked by WorldStatic simple collision only (terminals, anything behind walls) */
	StaticGeometry
};

/** What the player points with (§2.1) */
UENUM(BlueprintType)
enum class EGSTN_InteractPointingMode : uint8
{
	/** The stick's world direction when past StickThreshold, else the facing (owner answer INT.1: the character points) */
	StickThenFacing,
	/** The facing only, for kits that should not steer by stick */
	Facing,
	/** The 3D aim direction while CameraAimWhen holds and aim values exist; otherwise as StickThenFacing. Off by default */
	CameraAim
};

/** Whether priority biases the cost or beats it (§2.3) */
UENUM(BlueprintType)
enum class EGSTN_InteractPriorityMode : uint8
{
	/** Priority is one weighted term of the cost */
	Biased,
	/** Priority wins first, cost only orders equal priorities */
	Lexicographic
};

/** Why the focus is what it is after a probe (debug draw, tests) */
UENUM(BlueprintType)
enum class EGSTN_InteractFocusReason : uint8
{
	/** No candidate and no previous focus */
	None,
	/** First focus from nothing */
	Acquired,
	/** The incumbent stays */
	Kept,
	/** A challenger replaced the incumbent (margin and dwell, or the incumbent was gone) */
	Switched,
	/** The incumbent stays because Interact is held */
	Locked,
	/** The previous focus ended and nothing replaced it */
	Lost
};

/** Where the pointing direction came from */
UENUM(BlueprintType)
enum class EGSTN_InteractPointingSource : uint8
{
	Stick,
	Facing,
	Aim
};

/** Providers of candidates. 0 is "none". Ids are small and stable because they are part of a candidate id; kept in this one file (the central id file of modularity §5b) */
namespace GSTNInteractProviders
{
	/** Candidates registered by interactable components (and any code that adds a record to the registry directly) */
	constexpr uint8 Registry = 1;
	/** First id free for a snapshot provider of a consumer plugin */
	constexpr uint8 FirstConsumer = 16;
	/** GSTNLink's grabbable entities (FGSTN_LinkInteractProvider, GSTNLinkGAS). The next consumer takes FirstConsumer + 1 */
	constexpr uint8 LinkEntities = FirstConsumer;
}

/**
 * The opaque identity of a candidate: {Provider, Handle}, 5 bytes, both zero = none. No entity reference, no pointer, no tag
 * name, so it can sit in a sync fragment. Total order: Provider, then Handle.
 */
USTRUCT(BlueprintType)
struct GSTNINTERACTCORE_API FGSTN_InteractCandidateId
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	uint8 Provider = 0;

	/** The provider's own value (a uint32 bit pattern) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	int32 Handle = 0;

	FGSTN_InteractCandidateId() = default;
	FGSTN_InteractCandidateId(uint8 InProvider, uint32 InHandle) : Provider(InProvider), Handle(static_cast<int32>(InHandle)) {}

	bool IsValid() const { return Provider != 0 || Handle != 0; }
	void Reset() { Provider = 0; Handle = 0; }

	/** Provider<<32 | Handle: the opaque key combat stamps into an activation context (§3.3) */
	uint64 ToKey() const { return (static_cast<uint64>(Provider) << 32) | static_cast<uint32>(Handle); }
	static FGSTN_InteractCandidateId FromKey(uint64 Key) { return FGSTN_InteractCandidateId(static_cast<uint8>(Key >> 32), static_cast<uint32>(Key)); }

	bool operator==(const FGSTN_InteractCandidateId& Other) const { return Provider == Other.Provider && Handle == Other.Handle; }
	bool operator!=(const FGSTN_InteractCandidateId& Other) const { return !(*this == Other); }
	bool operator<(const FGSTN_InteractCandidateId& Other) const { return ToKey() < Other.ToKey(); }
	friend uint32 GetTypeHash(const FGSTN_InteractCandidateId& Id) { return ::GetTypeHash(Id.ToKey()); }

	FString ToString() const { return FString::Printf(TEXT("%u:%u"), Provider, static_cast<uint32>(Handle)); }
};

/**
 * One candidate as numbers (§3.2, §9): immutable once registered (a moved item is a removal plus a new record). Positions are
 * whole centimetres, which is the quantisation of §2.3, so two machines hold the same integers. Stamps are server frame numbers.
 */
USTRUCT(BlueprintType)
struct GSTNINTERACTCORE_API FGSTN_InteractCandidate
{
	GENERATED_BODY()

	static constexpr int32 DefaultPriority = -1;
	static constexpr int32 FlagFocusWhenUnactionable = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	FGSTN_InteractCandidateId Id;

	/** Anchor in whole centimetres */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	FIntVector PositionCm = FIntVector::ZeroValue;

	/** Reach and distance are measured to the surface: max(0, distance - Radius) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact", meta = (ClampMin = 0, ClampMax = 65535, ForceUnits = "cm"))
	int32 RadiusCm = 0;

	/** Index into the compiled meaning table (1-based; 0 = none) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact", meta = (ClampMin = 0, ClampMax = 255))
	int32 MeaningIndex = 0;

	/** 0..255; -1 takes the meaning's default priority */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact", meta = (ClampMin = -1, ClampMax = 255))
	int32 Priority = DefaultPriority;

	/** 0 = the rules' ReachCm; clamped to HardMaxReachCm */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact", meta = (ClampMin = 0, ClampMax = 65535, ForceUnits = "cm"))
	int32 ReachOverrideCm = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	EGSTN_InteractLosPolicy LosPolicy = EGSTN_InteractLosPolicy::None;

	/** Index of the requirement query in the query's Requirements; -1 = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact", meta = (ClampMin = -1))
	int32 QueryIndex = -1;

	/** Focusable from this frame (inclusive) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	int32 FromFrame = 0;

	/** The meaning tag is projected from this frame (>= FromFrame); before it the candidate can be focused but not acted on */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	int32 ActionableFrom = 0;

	/** Exclusive end. Tombstones keep the record after this so a late reader sees a removal by stamp, not by absence */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	int32 ToFrame = MAX_int32;

	/** FlagFocusWhenUnactionable: a locked terminal shows "needs key": focused, no tag projected (INT.6) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	int32 Flags = 0;

	bool IsLiveAt(int32 Frame) const { return Frame >= FromFrame && Frame < ToFrame; }
	bool FocusesWhenUnactionable() const { return (Flags & FlagFocusWhenUnactionable) != 0; }
	static FIntVector QuantiseCm(const FVector& Location);
	/** The same rounding as a vector of whole centimetres */
	static FVector SnapCm(const FVector& Location) { const FIntVector Cm = QuantiseCm(Location); return FVector(Cm.X, Cm.Y, Cm.Z); }
	FVector GetPosition() const { return FVector(PositionCm.X, PositionCm.Y, PositionCm.Z); }
};

/**
 * Tuning of the focus (§9). Proposal defaults. The game thread interactor and a simulation bridge read the same values so
 * both compute the same focus; presentation never reads this struct.
 */
USTRUCT(BlueprintType)
struct GSTNINTERACTCORE_API FGSTN_InteractRules
{
	GENERATED_BODY()

	// Reach

	/** Default reach to a candidate's surface */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Reach", meta = (ClampMin = 1, ForceUnits = "cm"))
	float ReachCm = 200.f;

	/** The largest reach any per-candidate override can have. Also sizes the registry's grid cells */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Reach", meta = (ClampMin = 1, ForceUnits = "cm"))
	float HardMaxReachCm = 400.f;

	/** Inside this surface distance the angle is ignored: an item under the feet is always eligible */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Reach", meta = (ClampMin = 0, ForceUnits = "cm"))
	float NearRadiusCm = 60.f;

	/** Height difference along Up beyond which a candidate is another floor (planar modes) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Reach", meta = (ClampMin = 0, ForceUnits = "cm"))
	float MaxHeightDeltaCm = 150.f;

	/** Extra reach the server allows when it validates a press (lag slack) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Reach", meta = (ClampMin = 0, ForceUnits = "cm"))
	float ServerReachSlackCm = 60.f;

	// Pointing

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Pointing")
	EGSTN_InteractPointingMode PointingMode = EGSTN_InteractPointingMode::StickThenFacing;

	/** CameraAim mode only: while this query holds on the interactor's tags the aim direction points. Empty = always */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Pointing")
	FGameplayTagQuery CameraAimWhen;

	/** Stick magnitude (0..1) from which the stick direction points. Added to the spec's list (the spec names the classifier's Aim zone) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Pointing", meta = (ClampMin = 0, ClampMax = 1))
	float StickThreshold = 0.3f;

	/** Half-angle at full reach, at the stick or facing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Pointing", meta = (ClampMin = 1, ClampMax = 180, ForceUnits = "deg"))
	float ArcFarDeg = 50.f;

	/** Half-angle just outside NearRadiusCm, widening towards it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Pointing", meta = (ClampMin = 1, ClampMax = 180, ForceUnits = "deg"))
	float ArcNearDeg = 110.f;

	/** Half-angle of the narrower 3D arc in CameraAim mode */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Pointing", meta = (ClampMin = 1, ClampMax = 180, ForceUnits = "deg"))
	float AimArcDeg = 20.f;

	/** The shared Input.Button tag the interact binding uses (data for validators; the probe takes the pressed state as input) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Pointing", meta = (Categories = "Input.Button"))
	FGameplayTag InteractButton;

	/** While this query holds on the interactor's tags (hitstun, linked) there is no focus */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Pointing")
	FGameplayTagQuery BlockedWhen;

	// Scoring

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Scoring", meta = (ClampMin = 0))
	float AngleWeight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Scoring", meta = (ClampMin = 0))
	float DistanceWeight = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Scoring", meta = (ClampMin = 0))
	float PriorityWeight = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Scoring", meta = (ClampMin = 0.1))
	float AnglePower = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Scoring")
	EGSTN_InteractPriorityMode PriorityMode = EGSTN_InteractPriorityMode::Biased;

	/** Costs within this many cost units fall in the same tie bucket, then higher priority, then the lower id decides */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Scoring", meta = (ClampMin = 0.0002))
	float TieEpsilon = 1.f / 256.f;

	// Focus

	/** The incumbent stays a candidate up to this much beyond the reach where it could be newly acquired */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Focus", meta = (ClampMin = 1))
	float StickyReachScale = 1.15f;

	/** The incumbent stays a candidate up to this much beyond the arc where it could be newly acquired */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Focus", meta = (ClampMin = 1))
	float StickyArcScale = 1.25f;

	/** A challenger can replace the incumbent only after the incumbent held the focus for this many frames */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Focus", meta = (ClampMin = 0, ForceUnits = "frames"))
	int32 MinDwellFrames = 10;

	/** The challenger must be cheaper by max(SwitchMarginAbs, SwitchMarginRatio * incumbent cost) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Focus", meta = (ClampMin = 0))
	float SwitchMarginRatio = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Focus", meta = (ClampMin = 0))
	float SwitchMarginAbs = 0.05f;

	/** While Interact is pressed or held a valid incumbent is kept, so a hold bar is never reset by a neighbour */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Focus")
	bool bHoldLocksFocus = true;

	/** The optional cycle command skips the incumbent for this long; 0 = the cycle command is off (INT.2) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Focus", meta = (ClampMin = 0, ForceUnits = "frames"))
	int32 CycleSkipFrames = 0;

	// Lifetime

	/** Server stamps a spawned record FromFrame = now + this (640 ms at 62.5 Hz) so a client receives it before it simulates that frame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Lifetime", meta = (ClampMin = 0, ForceUnits = "frames"))
	int32 MinAgeFrames = 40;

	/** Removed records stay readable this long so a late reader sees the removal by stamp */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Lifetime", meta = (ClampMin = 0, ForceUnits = "frames"))
	int32 TombstoneFrames = 150;

	// Budget

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Budget", meta = (ClampMin = 1))
	int32 MaxCandidatesPerProbe = 64;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Budget", meta = (ClampMin = 0))
	int32 MaxLosTraces = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Budget", meta = (ClampMin = 0))
	int32 MaxFilterCalls = 8;

	/** The caller probes on frames divisible by this (it halves the cost at one frame of staleness); the maths itself runs whenever called */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Budget", meta = (ClampMin = 1))
	int32 ProbeEveryNFrames = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interact|Budget", meta = (ClampMin = 1))
	int32 MaxRecords = 4096;

	/** Fixes values that would break an invariant (reach order, arc order). Returns true when something changed */
	bool Sanitise();
	/** True when the invariants hold; OutError names the first that does not */
	bool IsValid(FString* OutError = nullptr) const;
};

/** The persistent part of a probe (§2.5): what the incumbent is and since when. Plain data, copied into a sync fragment by a bridge */
USTRUCT(BlueprintType)
struct GSTNINTERACTCORE_API FGSTN_InteractFocusState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	FGSTN_InteractCandidateId FocusId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	int32 FocusSinceFrame = 0;

	/** The id came from authority and no known candidate has it (a late record, §3.2): kept as a ghost, no tag, no highlight */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	bool bGhost = false;

	/** The candidate the cycle command skips, until the given frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	FGSTN_InteractCandidateId CycleSkipId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	int32 CycleSkipUntilFrame = 0;

	void Reset() { *this = FGSTN_InteractFocusState(); }

	/** Takes an id decided elsewhere (the server's, at a reconcile): possibly unknown to this machine, so a ghost until a record arrives */
	void AdoptAuthoritative(const FGSTN_InteractCandidateId& Id, int32 SinceFrame, bool bIsGhost)
	{
		FocusId = Id;
		FocusSinceFrame = SinceFrame;
		bGhost = bIsGhost && Id.IsValid();
	}
};

/** Output of one probe */
USTRUCT(BlueprintType)
struct GSTNINTERACTCORE_API FGSTN_InteractFocusResult
{
	GENERATED_BODY()

	static constexpr int32 FlagGhost = 1;
	static constexpr int32 FlagUnactionable = 2;

	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	FGSTN_InteractCandidateId FocusId;

	/** Compiled meaning index to project as the one Interact.Target tag; 0 = focus with no actionable meaning */
	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	int32 MeaningIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	int32 FocusSinceFrame = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	EGSTN_InteractFocusReason Reason = EGSTN_InteractFocusReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	int32 Flags = 0;

	/** Cost of the focus (debug) */
	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	float Cost = 0.f;

	/** Candidates scored and line-of-sight traces spent (debug, budget tests) */
	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	int32 CandidatesScored = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	int32 TracesUsed = 0;

	bool HasFocus() const { return FocusId.IsValid(); }
	bool IsGhost() const { return (Flags & FlagGhost) != 0; }
	bool IsUnactionable() const { return (Flags & FlagUnactionable) != 0; }
};

/** What a Blueprint candidate filter sees of a survivor (§6 item 3) */
USTRUCT(BlueprintType)
struct GSTNINTERACTCORE_API FGSTN_InteractCandidateView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	FGSTN_InteractCandidateId Id;

	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	int32 MeaningIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	float Cost = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	float DistanceCm = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Interact")
	float AngleDeg = 0.f;
};

/** One designer-authored meaning (§5 prompt, §2.3 priority) */
USTRUCT(BlueprintType)
struct GSTNINTERACTCORE_API FGSTN_InteractMeaning
{
	GENERATED_BODY()

	/** The Interact.Target leaf projected while this meaning is the focus */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact", meta = (Categories = "Interact.Target"))
	FGameplayTag Tag;

	/** Used when a candidate sets none. Proposal order (INT.5): Grab > Pickup > Objective > Equipment > Mount */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact", meta = (ClampMin = 0, ClampMax = 255))
	int32 DefaultPriority = 100;

	/** Prompt verb of a tap; empty = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	FText TapVerb;

	/** Prompt verb of a hold; empty = none (a pickup shows only the hold) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact")
	FText HoldVerb;

	/** Frames the hold takes; the prompt's progress bar divides by it. A validator compares it with the binding */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interact", meta = (ClampMin = 0, ForceUnits = "frames"))
	int32 HoldFrames = 0;
};

/**
 * The compiled meaning table (§3.1): entries sorted by tag name so every machine derives the same index (modularity §4.7).
 * Index 0 is reserved for "no actionable meaning", so the first entry is index 1. Not a UObject: owned by a subsystem or a test.
 */
struct GSTNINTERACTCORE_API FGSTN_InteractMeaningTable
{
	/** Replaces the table. Entries with an invalid or duplicate tag are dropped (reported in OutErrors when given) */
	void Build(TConstArrayView<FGSTN_InteractMeaning> InEntries, TArray<FString>* OutErrors = nullptr);

	int32 Num() const { return Entries.Num(); }
	/** 1-based compiled index of a tag; 0 when unknown */
	int32 IndexOf(const FGameplayTag& Tag) const;
	/** The entry of a compiled index, or null for 0 and out of range */
	const FGSTN_InteractMeaning* Get(int32 Index) const;
	FGameplayTag TagOf(int32 Index) const;
	int32 DefaultPriorityOf(int32 Index) const;
	/** Hash of the sorted set, for the content signature (modularity §3.9): two machines with different meanings differ */
	uint32 Signature() const { return SignatureValue; }

private:
	TArray<FGSTN_InteractMeaning> Entries;
	uint32 SignatureValue = 0;
};
