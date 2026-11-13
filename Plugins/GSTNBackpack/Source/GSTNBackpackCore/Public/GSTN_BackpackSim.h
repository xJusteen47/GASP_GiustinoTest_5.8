// Backpacks, pure simulation data (M11 L7b): the backpack's memory and dome as combat sync state, the timeline tracks that drive them and
// the pure step. No ability system, no Network Prediction, no world: the NP module (GSTNBackpackNP) runs the step inside the combat tick.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Misc/StringBuilder.h"
#include "Sync/GSTN_SyncFragment.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_BackpackSim.generated.h"

/** How a memory slot of a backpack gets its charges back (owner 2026-11-12: "a backpack that gives more EX charges must know how many remain") */
UENUM(BlueprintType)
enum class EGSTN_BackpackRefill : uint8
{
	/** Never in the mission: spent is spent (a limited supply) */
	Never,

	/** Every charge comes back when the carrier lands (a jet burst per airtime) */
	OnLanding,

	/** One charge comes back every RefillFrames frames (a cooldown) */
	OverTime
};

/** How long a dome stays up */
UENUM(BlueprintType)
enum class EGSTN_DomeLifetime : uint8
{
	/** While the move that raised it runs: its button held (the move ends on release), at most the timeline's length. Recommended default */
	WhileHeld,

	/** DurationFrames from the raise, whatever the carrier does next (deployed and left behind) */
	Deploy
};

/** Number of memory slots a backpack has: a fixed small array so the state is plain integers on the wire */
namespace GSTNBackpackMemory
{
	constexpr int32 NumSlots = 4;
}

/**
 * Track of a backpack move (one of the backpack's move slots): when its timeline starts, the combat tick spends Cost charges of memory slot
 * Slot. The slot's capacity and refill rule are data of the track (the same values on every move that shares the slot: the validator does not
 * check it yet), copied into the sync state on the first spend, so the simulation never reads the backpack asset. The binding's gate is its
 * IgnoreTags on Backpack.State.Empty.Slot<N> (projected from the state), like the air spin's.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Backpack Charge"))
struct GSTNBACKPACKCORE_API FGSTN_BackpackChargeTrack : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** Memory slot, 0 to GSTNBackpackMemory::NumSlots - 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backpack", meta = (ClampMin = "0", ClampMax = "3"))
	int32 Slot = 0;

	/** Charges the move spends */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backpack", meta = (ClampMin = "1", ClampMax = "255"))
	int32 Cost = 1;

	/** Capacity of the slot */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backpack", meta = (ClampMin = "1", ClampMax = "255"))
	int32 Charges = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backpack")
	EGSTN_BackpackRefill Refill = EGSTN_BackpackRefill::OverTime;

	/** OverTime: frames per charge back (simulation frames, 62.5 per second) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Backpack", meta = (ClampMin = "1", ClampMax = "65535", ForceUnits = "Frames", EditCondition = "Refill == EGSTN_BackpackRefill::OverTime"))
	int32 RefillFrames = 625;

	static const FGSTN_BackpackChargeTrack* Find(const FGSTN_CombatTimelineData& Data);
};

/**
 * Track of a dome move (the shield-dome backpack, owner 2026-11-12: an area dome, not a personal shield). When its timeline starts the combat
 * tick raises a dome of RadiusCm centred where the carrier stood at the end of the previous frame; it holds by Lifetime.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Backpack Dome"))
struct GSTNBACKPACKCORE_API FGSTN_BackpackDomeTrack : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dome", meta = (ClampMin = "50", ClampMax = "5000", ForceUnits = "cm"))
	int32 RadiusCm = 400;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dome")
	EGSTN_DomeLifetime Lifetime = EGSTN_DomeLifetime::WhileHeld;

	/** Deploy: frames the dome stays up. WhileHeld: the timeline's length is the cap */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dome", meta = (ClampMin = "1", ForceUnits = "Frames", EditCondition = "Lifetime == EGSTN_DomeLifetime::Deploy"))
	int32 DurationFrames = 500;

	static const FGSTN_BackpackDomeTrack* Find(const FGSTN_CombatTimelineData& Data);
};

/** One memory slot as sync state */
struct FGSTN_BackpackMemorySlot
{
	/** Charges spent; 0 = full, which is also the state of a slot never used */
	uint8 Spent = 0;

	/** Capacity, copied from the track of the first spend (0 = the slot was never used) */
	uint8 Charges = 0;

	EGSTN_BackpackRefill Refill = EGSTN_BackpackRefill::Never;

	uint16 RefillFrames = 0;

	/** OverTime: the frame the running refill counts from; INDEX_NONE when full */
	int32 RefillFromFrame = INDEX_NONE;

	bool IsDefault() const { return Spent == 0 && Charges == 0 && Refill == EGSTN_BackpackRefill::Never && RefillFrames == 0 && RefillFromFrame == INDEX_NONE; }
	bool IsEmpty() const { return Charges > 0 && Spent >= Charges; }
	bool operator==(const FGSTN_BackpackMemorySlot& Other) const
	{
		return Spent == Other.Spent && Charges == Other.Charges && Refill == Other.Refill && RefillFrames == Other.RefillFrames && RefillFromFrame == Other.RefillFromFrame;
	}
};

/**
 * The backpack state of one entity as combat sync state (M11 L7b): the memory slots (charges left of each backpack move slot) and the one dome
 * it holds up. Rides in the combat sync state like the other fragments, compared, reconciled and rolled back with it; an entity that never
 * used a backpack move has none, which costs nothing on the wire.
 *
 * Integers and frames only, written by FGSTN_BackpackStep (from the NP tick extension) and nowhere else. Timers are frames against stamps.
 * Wire format: nothing at the defaults; else a 5 bit mask (bit i = memory slot i not default, bit 4 = the dome) and the groups present:
 * a slot is spent 1, charges 1, refill 1, refill frames packed, refill from + 1 packed; the dome is start + 1 packed, until + 1 packed,
 * timeline packed, radius packed, lifetime 1, centre 3 x 4 (whole cm).
 */
USTRUCT()
struct GSTNBACKPACKCORE_API FGSTN_BackpackFragment : public FGSTN_SyncFragment
{
	GENERATED_BODY()

	FGSTN_BackpackMemorySlot Memory[GSTNBackpackMemory::NumSlots];

	// ---- Dome

	/** Frame the dome went up; INDEX_NONE = no dome */
	int32 DomeStartFrame = INDEX_NONE;

	/** Deploy: the dome holds before this frame (exclusive). WhileHeld: the cap (the end of the timeline) */
	int32 DomeUntilFrame = INDEX_NONE;

	/** WhileHeld: the combat state's ActiveTimeline that raised it; the dome falls when another one runs */
	uint16 DomeTimeline = 0;

	uint16 DomeRadiusCm = 0;

	EGSTN_DomeLifetime DomeLifetime = EGSTN_DomeLifetime::WhileHeld;

	/** Whole centimetres, world space */
	FIntVector DomeCenter = FIntVector::ZeroValue;

	bool HasDome() const { return DomeStartFrame != INDEX_NONE; }

	/** The dome covers Point (world space): inside the sphere. False without a dome */
	bool DomeCovers(const FVector& Point) const;

	bool IsDefault() const;
	bool operator==(const FGSTN_BackpackFragment& Other) const;

	virtual void NetSerialize(FArchive& Ar) override;
	virtual bool ShouldReconcile(const FGSTN_SyncFragment& Authority) const override;
	virtual void Interpolate(const FGSTN_SyncFragment* From, const FGSTN_SyncFragment* To, float PCT) override {}
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
};

/** Registers FGSTN_BackpackFragment under GSTNFeatureIds::Backpack (the NP module at startup; exposed for the tests) */
struct GSTNBACKPACKCORE_API FGSTN_BackpackFragmentRegistration
{
	static bool Register();
	static void Unregister();
};

/**
 * The backpack's pure step (M11 L7b), deterministic: the same fragment and inputs give the same fragment on every machine and in every
 * re-simulation. The NP tick extension gathers the inputs from the combat state; the tests drive it directly.
 */
struct GSTNBACKPACKCORE_API FGSTN_BackpackStep
{
	/**
	 * Start of the frame: OverTime slots get a charge back each RefillFrames; a Deploy dome past its frames falls; a WhileHeld dome falls when
	 * bDomeTimelineRunning is false (the move that raised it ended or was replaced) or its cap passed
	 */
	static void AfterClocks(FGSTN_BackpackFragment& Fragment, int32 Frame, bool bDomeTimelineRunning);

	/** The carrier landed this frame: every OnLanding slot is full again */
	static void Landed(FGSTN_BackpackFragment& Fragment);

	/** A move with a charge track started: spends its cost (clamped at the capacity). False when the slot was already empty (the gate missed) */
	static bool Spend(FGSTN_BackpackFragment& Fragment, int32 Frame, const FGSTN_BackpackChargeTrack& Track);

	/**
	 * A move with a dome track started: the dome goes up at Center, replacing one already up. TimelineIndex and TimelineFrames are the
	 * running timeline's (the WhileHeld identity and cap)
	 */
	static void RaiseDome(FGSTN_BackpackFragment& Fragment, int32 Frame, const FGSTN_BackpackDomeTrack& Track, const FVector& Center, int32 TimelineIndex, int32 TimelineFrames);

	/** Backpack.State.Empty.Slot<N> for each empty slot, Backpack.State.Dome while a dome is up (null fragment: none) */
	static void GatherStateTags(const FGSTN_BackpackFragment* Fragment, FGameplayTagContainer& OutTags);

	/** Every tag GatherStateTags can return: what the tick extension sets or clears */
	static const FGameplayTagContainer& GetProjectableTags();

	/** Charges left in Slot; INDEX_NONE for a slot never used (full, its capacity is only known from the track of a first spend) */
	static int32 ChargesLeft(const FGSTN_BackpackFragment& Fragment, int32 Slot);
};
