// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PartsLimits.h"

/**
 * The per-entity state of the parts (combat-parts.md §5.1), plain integers only: this is what the sync fragment carries and
 * what the snapshot copies, so it is a trivially copyable POD and nothing in it is a float, a tag or a pointer.
 *
 * PartDamage[i] is part i's damage over its cap quantised to 8 bits (0 pristine, 255 at the cap). BrokenMask bit i is set once
 * part i has broken and only an explicit restore clears it, so the mask is monotonic within a hit. BreakSerial counts the
 * events that changed the mask (a break, a restore) in 4 bits and wraps, so presentation and events can be deduplicated by
 * (entity, part index, serial) across a rollback.
 *
 * Not a reflected struct: the arrays are fixed, the layout is the wire contract, and Blueprint reaches the state through
 * UGSTN_PartMath.
 */
struct GSTNPARTSCORE_API FGSTN_PartsState
{
	/** BreakSerial uses this many low bits */
	static constexpr uint8 SerialMask = 15;

	uint16 BrokenMask = 0;
	uint8 PartDamage[GSTN_PartsLimits::MaxParts] = {};
	uint8 BreakSerial = 0;

	bool IsBroken(int32 PartIndex) const
	{
		return PartIndex >= 0 && PartIndex < GSTN_PartsLimits::MaxParts && (BrokenMask & (1u << PartIndex)) != 0;
	}

	/** Moves the serial on by one, wrapping inside its 4 bits */
	void AdvanceSerial() { BreakSerial = static_cast<uint8>((BreakSerial + 1) & SerialMask); }

	/** Bitwise equality, the comparison a reconcile makes */
	bool operator==(const FGSTN_PartsState& Other) const
	{
		return BrokenMask == Other.BrokenMask && BreakSerial == Other.BreakSerial
			&& FMemory::Memcmp(PartDamage, Other.PartDamage, sizeof(PartDamage)) == 0;
	}
	bool operator!=(const FGSTN_PartsState& Other) const { return !(*this == Other); }
};

static_assert(TIsTriviallyCopyAssignable<FGSTN_PartsState>::Value, "the parts state is copied by value into snapshots and fragments");
