// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Loadout/GSTN_CompiledBindingTable.h"

/**
 * Where a compiled entry comes from, as one bit of a 32-bit mask. The input resolver (GSTNInputNP) never learns the source
 * vocabulary: it only sees a mask per binding and the mask of the sources that are active this frame, and a binding applies when
 * the two intersect. Mask 0 on a binding means "always active".
 *
 * Layout: bit 0 the kit, bits 1..8 the super slots, bits 9..31 the weapons (so up to MaxWeapons loadout weapons).
 */
namespace GSTNEntrySourceMask
{
	constexpr int32 KitBit = 0;
	constexpr int32 FirstSuperBit = 1;
	constexpr int32 FirstWeaponBit = FirstSuperBit + FGSTN_BindingTableCompiler::MaxSuperSlots;
	constexpr int32 MaxWeapons = 32 - FirstWeaponBit;

	/** The bit of a source. Zero (never active through the mask) for an index past the layout, so a malformed row is inert, not aliased */
	inline uint32 Bit(EGSTN_EntrySource Source, int32 SourceIndex)
	{
		switch (Source)
		{
		case EGSTN_EntrySource::Kit:
			return 1u << KitBit;
		case EGSTN_EntrySource::Super:
			return (SourceIndex >= 0 && SourceIndex < FGSTN_BindingTableCompiler::MaxSuperSlots) ? (1u << (FirstSuperBit + SourceIndex)) : 0u;
		case EGSTN_EntrySource::Weapon:
			return (SourceIndex >= 0 && SourceIndex < MaxWeapons) ? (1u << (FirstWeaponBit + SourceIndex)) : 0u;
		}
		return 0u;
	}

	/** The sources that are active: the kit, every super slot (a slot without an entry has no row) and the active weapon */
	inline uint32 ActiveMask(int32 ActiveWeaponIndex)
	{
		uint32 Mask = 1u << KitBit;
		for (int32 Super = 0; Super < FGSTN_BindingTableCompiler::MaxSuperSlots; ++Super)
		{
			Mask |= 1u << (FirstSuperBit + Super);
		}
		return Mask | Bit(EGSTN_EntrySource::Weapon, ActiveWeaponIndex);
	}
}
