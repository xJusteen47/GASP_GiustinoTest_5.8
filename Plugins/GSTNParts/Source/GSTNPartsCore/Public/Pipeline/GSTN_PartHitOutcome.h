// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PartsLimits.h"
#include "Hits/GSTN_HitExtensions.h"
#include "Math/GSTN_PartsState.h"
#include "GSTN_PartHitOutcome.generated.h"

/** Bit flags of FGSTN_PartHitOutcome::Flags */
namespace GSTNPartOutcomeFlags
{
	/** The band ricochets: nothing changed, the attacker recoils */
	constexpr uint8 Ricochet = 1 << 0;
	/** The hit moved the part's damage byte */
	constexpr uint8 PartDamaged = 1 << 1;
	/** The part is a weak point and the hit did not ricochet */
	constexpr uint8 WeakPointHit = 1 << 2;
	/** A part this hit broke asks to kill the entity */
	constexpr uint8 Kill = 1 << 3;
	/** The last breakable part broke with this hit */
	constexpr uint8 AllBroken = 1 << 4;
	/** The hit landed on a broken part that hides its hurtboxes and went to the body */
	constexpr uint8 PassedToBody = 1 << 5;
}

/**
 * What the part resolve step tells the outcome applier (combat-parts.md §5.3): one entry of the context's OutcomeExtensions per
 * hit on an entity that has a part set. The applier of GSTNPartsNP writes the result into the sync fragment at the victim's
 * frame F+1 and raises the events; this struct is plain data and is never a source of truth on its own.
 *
 * It carries the whole state the hit produced (ResultBrokenMask, ResultPartDamage, ResultBreakSerial) as well as the part and the
 * newly broken mask, so the applier writes with one assignment and a re-simulated hit lands on the same bytes. Two entries of
 * one drain are applied in the inbox order, each built from the state the earlier one left (combat-parts.md §4.2).
 *
 * All members are reflected, because an FInstancedStruct copies a struct property by property.
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartHitOutcome : public FGSTN_HitExtension
{
	GENERATED_BODY()

	/** The part the hit resolved on, INDEX_NONE for the body */
	UPROPERTY(BlueprintReadOnly, Category = "Part Outcome")
	int32 PartIndex = INDEX_NONE;

	/** The penetration band the hit fell in, INDEX_NONE for a body hit */
	UPROPERTY(BlueprintReadOnly, Category = "Part Outcome")
	int32 BandIndex = INDEX_NONE;

	/** The part's damage byte after the hit (0 for the body) */
	UPROPERTY(BlueprintReadOnly, Category = "Part Outcome")
	uint8 NewPartDamage = 0;

	/** Parts this hit broke, cascaded children included */
	UPROPERTY()
	uint16 NewlyBrokenMask = 0;

	/** The state after the hit */
	UPROPERTY()
	uint16 ResultBrokenMask = 0;

	UPROPERTY()
	uint8 ResultPartDamage[GSTN_PartsLimits::MaxParts] = {};

	UPROPERTY()
	uint8 ResultBreakSerial = 0;

	/** What the main pool takes (the damage the pipeline applies); 0 for a ricochet */
	UPROPERTY(BlueprintReadOnly, Category = "Part Outcome")
	float MainDelta = 0.f;

	/** GSTNPartOutcomeFlags */
	UPROPERTY(BlueprintReadOnly, Category = "Part Outcome")
	uint8 Flags = 0;

	bool HasFlag(uint8 Flag) const { return (Flags & Flag) != 0; }

	/** The state the hit left, as the part math and the fragment take it */
	FGSTN_PartsState GetResultState() const;
};
