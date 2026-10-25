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
 * global registry, and the snapshot is rebuilt every frame by code that already holds the loaded set (the fragment's owner). The
 * set is valid for the frame of the snapshot only; a history that outlives a frame must hold the id and rebuild this struct
 * (P7 decides). Never serialised and never sent.
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

	/** The entity's part set, or null (the step then treats the entity as having none) */
	const UGSTN_PartSetDefinition* PartSet = nullptr;

	/** The state the extension holds, as the part math takes it */
	FGSTN_PartsState ToState() const;

	/** Fills the state fields from a part state */
	void SetState(const FGSTN_PartsState& State);

	/** The extension a victim view carries, or null when it has none or the pointer is not a parts extension */
	static const FGSTN_PartsSnapshotExt* FromView(const FGSTN_HitVictimView& Victim);
};
