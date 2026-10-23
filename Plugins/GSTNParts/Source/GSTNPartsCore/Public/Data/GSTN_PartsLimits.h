// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"

/**
 * Fixed bounds of the part data (combat-parts.md §3.1, §5.1). They are not tuning: the broken mask is a uint16, the damage bytes
 * are 16 wide in the sync state and the snapshot, and the penetration rule is an integer comparison on a small tier scale, so
 * these are part of the wire and rollback contract and change only with it.
 */
namespace GSTN_PartsLimits
{
	/** Parts per entity: the width of the broken mask (uint16) and of the per-part damage array */
	constexpr int32 MaxParts = 16;

	/** Parts per minion (a uint8 mask, combat-parts.md §5.5): the minion bridge validates against this */
	constexpr int32 MaxMinionParts = 8;

	/** Hardness and penetration tiers are integers 0..MaxHardness (owner: Helldivers armour levels), so no float decides a band */
	constexpr int32 MaxHardness = 15;

	/** Lowest penetration-minus-hardness delta a hit can produce (penetration 0 against the hardest part); a band at or below it catches every delta */
	constexpr int32 MinPossibleDelta = -MaxHardness;

	/** Entries of a baked curve (combat-parts.md §4.3) */
	constexpr int32 CurveEntries = 64;

	/** Quantised part damage: 0 = pristine, 255 = at the cap (combat-parts.md §5.1) */
	constexpr int32 DamageAtCap = 255;
}
