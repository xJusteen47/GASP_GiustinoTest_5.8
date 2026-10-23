// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PartDefinition.h"
#include "Data/GSTN_PartHitData.h"
#include "Data/GSTN_PartsBakedCurve.h"
#include "Data/GSTN_PenetrationTable.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Math/GSTN_PartsProjection.h"
#include "Math/GSTN_PartsState.h"
#include "GSTN_PartMath.generated.h"

class UGSTN_PartSetDefinition;

/**
 * One hit as the part math sees it: the damage already multiplied by the attacker's outgoing and the victim's incoming
 * multipliers, the part the attacker's overlap picked, and the part data of the hit (combat-parts.md §3.2, §4.2).
 */
struct GSTNPARTSCORE_API FGSTN_PartHitInput
{
	/** D: the hit's damage. Non-finite or negative counts as 0 */
	float Damage = 0.f;

	/** The part the hit landed on, chosen by the attacker; INDEX_NONE (or out of range) = the body, no part */
	int32 PartIndex = INDEX_NONE;

	/** Penetration tier compared with the part's hardness */
	int32 Penetration = 0;

	/** The hit's own multiplier of the damage that goes to the part */
	float PartDamageScale = 1.f;

	/** Take the top band whatever the hardness */
	bool bIgnoreHardness = false;

	/** Damage the main pool only, leave the part alone */
	bool bMainOnly = false;

	/** From the hit's damage, the part index and its authored part data (null = penetration 0, scales of 1) */
	static FGSTN_PartHitInput Make(float InDamage, int32 InPartIndex, const FGSTN_PartHitData* Data);
};

/**
 * What one hit did to the parts and what the hit pipeline needs from it (combat-parts.md §4.2, §4.4). Plain data: the step
 * turns it into the hit's damage, its reaction modifiers and its outcome entry, and the outcome applier writes the new state
 * at the victim's tick. The state the math was given already holds the new values.
 */
struct GSTNPARTSCORE_API FGSTN_PartHitResult
{
	/** The part the hit resolved on, INDEX_NONE for the body (also when a disabled stump passed the hit through) */
	int32 PartIndex = INDEX_NONE;

	/** Index of the penetration band, INDEX_NONE for a body hit or an empty table */
	int32 BandIndex = INDEX_NONE;

	/** The band's multiplier of the hit's stagger force before the part's threshold test; 1 for the body */
	float StaggerScale = 1.f;

	/** The band's multiplier of the automatic hitstop; 1 for the body */
	float HitstopScale = 1.f;

	/** Damage that went into the part's own state; 0 for a ricochet, a broken part or an unbreakable one */
	float PartDelta = 0.f;

	/** The main pool's share of the hit itself: D x band main scale x the part's transfer */
	float HitMainDelta = 0.f;

	/** Bonus main damage of the break effects of the parts this hit broke, once each */
	float BonusMainDamage = 0.f;

	/** What the main pool takes: HitMainDelta + BonusMainDamage. This is what the step writes as the hit's damage */
	float MainDelta = 0.f;

	/** Parts this hit broke, cascaded children included */
	uint16 NewlyBrokenMask = 0;

	/** The part's damage byte after the hit (0 for the body) */
	uint8 NewPartDamage = 0;

	/** The band ricochets: no damage, no state change */
	bool bRicochet = false;

	/** The hit landed on a broken part whose policy is Disable and went to the body */
	bool bPassedToBody = false;

	/** The hit landed on a part that was already broken (a stump) */
	bool bHitBrokenPart = false;

	/** The hit moved the part's damage byte */
	bool bPartDamaged = false;

	/** The part is a weak point and the hit did not ricochet */
	bool bWeakPointHit = false;

	/** A part this hit broke asks to kill the entity */
	bool bKill = false;

	/** Every breakable part is broken after this hit and was not before */
	bool bAllBrokenReached = false;
};

/**
 * The deterministic part math (combat-parts.md §4.2, §4.3, §5.1): the penetration band, how a hit changes a part's state and
 * the main pool, the cascade of children, the effective weight, the derived projection and the restore. Static, pure and
 * allocation free on the hit path, with no tag, string, world or component access, so it is identical on every machine and
 * safe inside the simulation and a resimulation. It reads authored data through views and writes only the state it is given.
 *
 * Determinism: the band comes from an integer comparison, the stored damage is an integer, and the only floats are products of
 * authored values in a fixed order, so two machines running the same build agree bit for bit. The hot functions (ApplyHit)
 * stay C++; the Blueprint functions below are the stateless helpers a hook may call.
 *
 * Part indices are addressed up to GSTN_PartsLimits::MaxParts; entries beyond are ignored.
 */
UCLASS()
class GSTNPARTSCORE_API UGSTN_PartMath : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// --- Quantisation (C++) ---

	/**
	 * The stored byte for Damage over Cap: min(255, ceil(Damage / Cap x 255)). A nonzero damage never stores 0. No cap (zero or
	 * less) or no damage stores 0
	 */
	static uint8 Quantise(float Damage, float Cap);

	/** The damage a stored byte stands for: byte / 255 x Cap. The next hit starts from this value, so the rounding is part of the rule */
	static float Dequantise(uint8 Quantised, float Cap);

	/**
	 * The byte after adding Delta damage to a part holding Current: min(255, ceil(Current + Delta / Cap x 255)). A positive
	 * Delta always moves the byte up by at least one until the cap. Worked in byte units, so no dequantise round trip drifts
	 */
	static uint8 AddDamage(uint8 Current, float Delta, float Cap);

	// --- Bands (C++) ---

	/** The band for a hit: Delta = Penetration - Hardness, IgnoreHardness = the top band. See UGSTN_PenetrationTable::LookupBand */
	static int32 LookupBand(TConstArrayView<FGSTN_PenetrationBand> SortedBands, int32 Penetration, int32 Hardness, bool bIgnoreHardness);

	// --- Hits (C++) ---

	/**
	 * Applies one hit to the state. PartDelta = D x band part scale x hit scale x part scale, MainDelta = D x band main scale x
	 * the part's transfer (its broken transfer once broken). The part breaks when its stored byte reaches 255; damage beyond the
	 * cap is not carried to the main pool. Children of a part that broke break in the same hit, in index order, and each newly
	 * broken part adds its MainDamage effects once. A ricochet band changes nothing. A hit on the body is the full damage.
	 * Allocation free. Two hits in one frame must be applied in the inbox's sorted order: each sees the state of the one before
	 */
	static FGSTN_PartHitResult ApplyHit(TConstArrayView<FGSTN_PartDefinition> Parts, TConstArrayView<FGSTN_PenetrationBand> SortedBands,
		FGSTN_PartsState& State, const FGSTN_PartHitInput& Hit);

	/** The mask closed under children: every part whose parent chain reaches a part in BrokenMask is added. Terminates on a cycle */
	static uint16 ComputeCascade(TConstArrayView<FGSTN_PartDefinition> Parts, uint16 BrokenMask);

	/** Bit i set for every part with a cap above zero */
	static uint16 GetBreakableMask(TConstArrayView<FGSTN_PartDefinition> Parts);

	/** Every breakable part is broken, and there is at least one */
	static bool IsAllBroken(TConstArrayView<FGSTN_PartDefinition> Parts, uint16 BrokenMask);

	// --- Weight (C++) ---

	/**
	 * The multiplier of the base weight: the curve at Progress times (1 - LostMassFraction) of every broken part, never below
	 * MinWeightScale. Progress is the entity's damage over its cap (0..1)
	 */
	static float EffectiveWeightScale(TConstArrayView<FGSTN_PartDefinition> Parts, const FGSTN_PartsBakedCurve& Curve, float Progress,
		uint16 BrokenMask, float MinWeightScale);

	/** BaseWeight x EffectiveWeightScale. Derived on every resolution, never stored */
	static float EffectiveWeight(float BaseWeight, TConstArrayView<FGSTN_PartDefinition> Parts, const FGSTN_PartsBakedCurve& Curve,
		float Progress, uint16 BrokenMask, float MinWeightScale);

	// --- Derived projection (C++) ---

	/** True while a non-empty grab gate is unmet: the entity cannot be grabbed yet. An empty gate never locks */
	static bool IsGrabLocked(const FGSTN_GrabGate& Gate, uint16 BrokenMask);

	/**
	 * Fills Out from the state: the flags, the hurtboxes of broken parts whose policy is Disable, and what every derived break
	 * effect of the broken parts projects (tags, disabled hurtboxes, hit groups), in part index order. Out is reset first
	 */
	static void ProjectDerived(TConstArrayView<FGSTN_PartDefinition> Parts, const FGSTN_PartsState& State, const FGSTN_GrabGate& Gate,
		FGSTN_PartsProjection& Out);

	// --- Restore (C++) ---

	/**
	 * Restores parts (a healer, an explicit reset; parts never regenerate by themselves). Mask 0 = every part. A broken part in
	 * the mask unbreaks; the damage of every part in the mask becomes DamageFraction of its cap (0 = pristine) but never more than
	 * it held, so a restore never hurts. The fraction is capped just below the cap so a restored part is not broken. The serial
	 * advances once if any part unbroke, so restore then break again is a new event key. The main pool is not touched: giving
	 * percent back is the healer's own effect. Returns the parts that unbroke
	 */
	static uint16 RestoreParts(FGSTN_PartsState& State, uint16 Mask, float DamageFraction);

	// --- Blueprint helpers: stateless, pure, safe inside a simulation hook ---

	/** Index of the band a hit falls in; the table null = the built-in three bands. -1 only for an empty table */
	UFUNCTION(BlueprintPure, Category = "GSTN|Parts")
	static int32 GetPenetrationBandIndex(const UGSTN_PenetrationTable* Table, int32 Penetration, int32 Hardness, bool bIgnoreHardness);

	/** The stored byte (0..255) for a damage over a cap */
	UFUNCTION(BlueprintPure, Category = "GSTN|Parts")
	static int32 QuantisePartDamage(float Damage, float Cap);

	/** The damage a stored byte stands for */
	UFUNCTION(BlueprintPure, Category = "GSTN|Parts")
	static float DequantisePartDamage(int32 Quantised, float Cap);

	/** Whether bit PartIndex of a broken mask is set */
	UFUNCTION(BlueprintPure, Category = "GSTN|Parts")
	static bool IsPartBroken(int32 BrokenMask, int32 PartIndex);

	/** Number of broken parts in a mask */
	UFUNCTION(BlueprintPure, Category = "GSTN|Parts")
	static int32 CountBrokenParts(int32 BrokenMask);

	/** BaseWeight with the set's weight curve at Progress and the lost mass of the broken parts, never below the settings' floor */
	UFUNCTION(BlueprintPure, Category = "GSTN|Parts")
	static float GetEffectiveWeight(const UGSTN_PartSetDefinition* Set, float BaseWeight, float Progress, int32 BrokenMask);
};
