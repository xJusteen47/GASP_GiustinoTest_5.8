// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PartsLimits.h"
#include "Math/GSTN_PartsState.h"

class UGSTN_PartSetDefinition;
struct FGSTN_HitVictimView;

/**
 * What the hit pipeline reads of a victim's parts (combat-parts.md §5.2): the state at the end of the previous frame, copied into
 * the combat snapshot and held by the lag compensation history. Plain data, no tag, no container; the combat core hands it to the
 * steps as FGSTN_HitVictimView::SnapshotExtension (a const void*), which is why it starts with a type word the step checks
 * before it trusts the pointer.
 *
 * The part set is a pointer here, not the id of the spec (PartSetId): the pure step cannot look an asset up by id without a
 * global registry. The set is a data asset kept alive by the attach registry of GSTNPartsNP while any entity uses it, which is why
 * the lag compensation history may keep a copy of this struct (a 32 byte blob in the combat snapshot) without owning the set. Never
 * serialised and never sent. It is a trivially copyable struct of at most 32 bytes (static_assert below), the size of that blob.
 *
 * The NP adapter (GSTNPartsNP) fills it from the parts fragment; the minion bridge fills it from the minion store. An entity
 * without a part set simply has no extension, and the step does nothing for it.
 */
struct GSTNPARTSCORE_API FGSTN_PartsSnapshotExt
{
	/** 'PRTS': the word every instance starts with */
	static constexpr uint32 TypeWord = 0x53545250u;

	uint32 Type = TypeWord;
	uint16 BrokenMask = 0;
	uint8 PartDamage[GSTN_PartsLimits::MaxParts] = {};
	uint8 BreakSerial = 0;

	/** Derived bits (FlagGrabLocked), set by the snapshot contributor from the set and the mask; the step does not read them */
	uint8 Flags = 0;

	/** The grab gate of the set is unmet by the broken parts (Combat.Part.GrabLocked) */
	static constexpr uint8 FlagGrabLocked = 1 << 0;

	bool IsGrabLocked() const { return (Flags & FlagGrabLocked) != 0; }

	/** The entity's part set, or null (the step then treats the entity as having none) */
	const UGSTN_PartSetDefinition* PartSet = nullptr;

	/** The state the extension holds, as the part math takes it */
	FGSTN_PartsState ToState() const;

	/** Fills the state fields from a part state */
	void SetState(const FGSTN_PartsState& State);

	/** The extension a victim view carries, or null when it has none or the pointer is not a parts extension */
	static const FGSTN_PartsSnapshotExt* FromView(const FGSTN_HitVictimView& Victim);
};

static_assert(TIsTriviallyCopyAssignable<FGSTN_PartsSnapshotExt>::Value && sizeof(FGSTN_PartsSnapshotExt) <= 32, "the parts extension is copied into the combat snapshot's 32 byte extension blob");
