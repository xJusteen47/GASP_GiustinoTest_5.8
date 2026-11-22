// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitVictimView.h"
#include "Sync/GSTN_SyncFragment.h"

/** Bits of FGSTN_DefenceSnapshotExt::Flags */
namespace GSTNDefenceSnapshotFlags
{
	/** The mode's rules enable the shield */
	constexpr uint8 ShieldEnabled = 1 << 0;

	/** The kit names a shield ability */
	constexpr uint8 KitHasShield = 1 << 1;

	/** The shield is up (pressed, not released) at the snapshot frame */
	constexpr uint8 Raised = 1 << 2;

	/** The bubble blocks at the snapshot frame: raised past the startup frames, not breaking (the Shield step's "Raised at the hit's frame") */
	constexpr uint8 Blocks = 1 << 3;

	/** The bubble exists at the snapshot frame (from the press to the release plus ReleaseFrames, or while shield stunned; not in a hit reaction, not broken): the extra hurtbox was contributed */
	constexpr uint8 Bubble = 1 << 4;

	/** The entity carries Combat.Immunity.Parry */
	constexpr uint8 ImmuneParry = 1 << 5;

	/** The entity carries Combat.Immunity.Shield */
	constexpr uint8 ImmuneShield = 1 << 6;

	/** The shield is broken (Breaking) at the snapshot frame */
	constexpr uint8 Breaking = 1 << 7;
}

/**
 * The defence of an entity as the step snapshot carries it (m6 spec 9, "the snapshot now carries, per entity, the rules index... the
 * shield state... the move parry stamps... and the immunity tags"): a 32 byte POD copied into the combat snapshot's extension blob at
 * the end of the previous frame, kept with the lag compensation record, and read by the Parry and Shield steps through the victim view.
 * The attacker's prediction and the authority read the same bytes, so a disagreement means the victim's state really differs.
 *
 * Fields are frames, flags and a quantised value, never a tag or a pointer: the rules come back through FGSTN_DefenceAttach::RulesAt
 * (an index of this machine's table, which a snapshot never sends). ShieldValueQ is the analytic value AT THE SNAPSHOT FRAME, so the
 * steps need no rates: a hit stamped at frame F resolves against the record of F.
 */
struct GSTNDEFENCENP_API FGSTN_DefenceSnapshotExt
{
	/** 'DFNC' in little endian: the word a reader validates before it trusts the bytes (the victim view hands out a void pointer) */
	static constexpr uint32 TypeWord = 0x434E4644u;

	uint32 Type = TypeWord;
	uint8 Flags = 0;

	/** The fragment's MoveParryProfile: profile index in bits 0..6, bit 7 = close on parry */
	uint8 MoveParryProfile = 0;

	/** The victim's IncomingShieldDamageMultiplier in 1/16 (16 = 1) */
	uint8 ShieldDamageMultiplierQ = 16;

	/** FGSTN_DefenceAttach::RulesAt index; 255 = none */
	uint8 RulesIndex = 255;

	/** The shield value at the snapshot frame, quarter points */
	int16 ShieldValueQ = 0;

	/** Bit i = the move window covers hurtbox i (i < 16); 0 = any */
	uint16 MoveParryHurtboxMask = 0;

	int32 ShieldPressFrame = INDEX_NONE;
	int32 ShieldReleaseFrame = INDEX_NONE;
	int32 MoveParryStartFrame = INDEX_NONE;
	int32 MoveParryEndFrame = INDEX_NONE;

	bool Has(uint8 Flag) const { return (Flags & Flag) != 0; }

	/** The shield exists for this entity (mode and kit) */
	bool IsShieldAvailable() const { return Has(GSTNDefenceSnapshotFlags::ShieldEnabled) && Has(GSTNDefenceSnapshotFlags::KitHasShield); }

	double GetShieldValue() const { return static_cast<double>(ShieldValueQ) / 4.0; }
	double GetShieldDamageMultiplier() const { return static_cast<double>(ShieldDamageMultiplierQ) / 16.0; }

	/** The defence extension of a victim view, or null when the view carries none or the bytes are not this type */
	static const FGSTN_DefenceSnapshotExt* FromView(const FGSTN_HitVictimView& Victim);
};

static_assert(TIsTriviallyCopyAssignable<FGSTN_DefenceSnapshotExt>::Value && sizeof(FGSTN_DefenceSnapshotExt) <= 32, "the defence extension is copied into the combat snapshot's 32 byte extension blob");
