// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GSTN_MinionLOD.generated.h"

/**
 * Simulation level of detail of a minion (combat-m8-minions.md 7). The tier decides how often the batch steps the minion and what
 * it skips; it is chosen from the distance to the nearest living fighter. The numbers are the wire-free names T0..T3.
 */
UENUM(BlueprintType)
enum class EGSTN_MinionLODTier : uint8
{
	/** T0: everything every frame (AI 30 Hz, timeline, queries, movement, grid, history, replication 15 Hz) */
	Combat = 0,
	/** T1: stepped every second frame, queries only while an attack is active */
	Near,
	/** T2: steering and ground snap only, not in the grid (cannot be hit) */
	Far,
	/** T3: no movement, no AI, wake check only */
	Dormant,
	Count UMETA(Hidden)
};

/**
 * Tunables of the LOD tiers. Distances are to the nearest living fighter on the ground plane, in cm; each is the OUTER border of
 * its tier (Combat up to CombatDistance, Near up to NearDistance, Far up to FarDistance, Dormant beyond). They must increase.
 *
 * Hysteresis and dwell stop a minion at a border from flipping tier every frame: a tier change needs the distance to be beyond
 * the border by HysteresisFraction (outward: border x (1 + h); inward: border x (1 - h)) AND the minion to have spent at least
 * MinDwellFrames in its current tier. Hits, spawning and death bypass both and force Combat (see FGSTN_MinionLODMath::Step).
 */
USTRUCT(BlueprintType)
struct GSTNMINIONCORE_API FGSTN_MinionLODParams
{
	GENERATED_BODY()

	/** Outer border of T0 Combat */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|LOD|Distance", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float CombatDistance = 1500.f;

	/** Outer border of T1 Near. Must be above CombatDistance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|LOD|Distance", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float NearDistance = 3500.f;

	/** Outer border of T2 Far; beyond it the minion is Dormant. Must be above NearDistance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|LOD|Distance", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float FarDistance = 8000.f;

	/** Fraction of a border the distance must cross before the tier changes (0.1 = 10 %) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|LOD|Hysteresis", meta = (ClampMin = "0.0", ClampMax = "0.45"))
	float HysteresisFraction = 0.1f;

	/** Frames a minion stays in a tier before a distance-driven change is allowed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|LOD|Hysteresis", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 MinDwellFrames = 30;

	/** Batch interval of T0 in frames: the minion runs when (Frame + Index) % Interval == 0 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|LOD|Interval", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 CombatInterval = 1;

	/** Batch interval of T1 in frames */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|LOD|Interval", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 NearInterval = 2;

	/** Batch interval of T2 in frames */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|LOD|Interval", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 FarInterval = 4;

	/** Batch interval of T3 in frames (the wake check) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|LOD|Interval", meta = (ClampMin = "1", ForceUnits = "Frames"))
	int32 DormantInterval = 16;

	/** Most minions kept in T0 at once; the farthest are demoted to T1 (attackers never are) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|LOD|Budget", meta = (ClampMin = "1"))
	int32 MaxActiveCombat = 48;

	/** Outer border of a tier; Dormant has none and returns infinity */
	float GetBorder(EGSTN_MinionLODTier Tier) const;

	/** Batch interval of a tier in frames, at least 1 */
	int32 GetInterval(EGSTN_MinionLODTier Tier) const;

	/** True when the borders strictly increase, the hysteresis is below half a tier and every interval is at least 1 */
	bool IsConsistent() const;
};

/**
 * Project-wide LOD settings asset (distances and rates are cvar-overridable at runtime in a later checkpoint). A minion type
 * may carry its own FGSTN_MinionLODParams override in its definition; this asset is the default for every other type.
 */
UCLASS(BlueprintType)
class GSTNMINIONCORE_API UGSTN_MinionLODSettings : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion|LOD")
	FGSTN_MinionLODParams Params;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};

/** Where a minion stands in the LOD: its tier and how long it has been there. Kept in the store's LODTier and FramesInTier columns */
struct GSTNMINIONCORE_API FGSTN_MinionLODState
{
	EGSTN_MinionLODTier Tier = EGSTN_MinionLODTier::Combat;
	int32 FramesInTier = 0;
};

/**
 * The LOD decision as pure functions: no world, no store, no clock. The same inputs give the same tier on every machine, so the
 * server and a replay agree.
 */
struct GSTNMINIONCORE_API FGSTN_MinionLODMath
{
	/**
	 * The tier a distance maps to with no memory (no hysteresis, no dwell). Used for spawning far minions and as the reference of tests.
	 * A NaN or negative distance maps to Combat, the safe choice: a minion with an unknown distance is simulated fully.
	 */
	static EGSTN_MinionLODTier TierForDistance(const FGSTN_MinionLODParams& Params, float Distance);

	/**
	 * The next tier from the current one. Moving to a farther tier needs Distance > border x (1 + h) of every border crossed;
	 * moving to a nearer tier needs Distance < border x (1 - h). FramesInTier below MinDwellFrames keeps the current tier unless
	 * bForceCombat is set, which returns Combat at once (hit, attacking, launched, dying, spawning, wake).
	 */
	static EGSTN_MinionLODTier Evaluate(const FGSTN_MinionLODParams& Params, EGSTN_MinionLODTier Current, float Distance, int32 FramesInTier, bool bForceCombat);

	/**
	 * Advances State by DeltaFrames frames in the tier and applies Evaluate. On a change the tier is replaced and FramesInTier
	 * restarts at 0. Returns true when the tier changed.
	 */
	static bool Step(const FGSTN_MinionLODParams& Params, FGSTN_MinionLODState& State, float Distance, bool bForceCombat, int32 DeltaFrames = 1);
};
