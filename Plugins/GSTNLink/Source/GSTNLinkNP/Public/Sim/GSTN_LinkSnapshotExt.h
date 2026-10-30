// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "State/GSTN_LinkState.h"

struct FGSTN_HitVictimView;
struct FGSTN_ResolvedHit;
struct FGSTN_LinkFragment;

/** Bits of FGSTN_LinkSnapshotExt::Bits */
namespace GSTNLinkSnapshotBits
{
	/** The entity holds the parked role of its Body link: its snapshot transform was substituted from its partner's (FinalizeSnapshot) */
	constexpr uint8 Parked = 1 << 0;
	/** The parked role does not act while linked (the grabbed): no candidates, every command vetoed */
	constexpr uint8 BlocksActions = 1 << 1;
}

/**
 * The link snapshot extension of one entity (spec seam D9, m6 spec 9 "the snapshot now carries Link.Role, GrabImmunityEnd and the immunity
 * tags"): the Body and Hand link summaries, the Body partner, the grab immunity stamp, the request tags as a mask over FGSTN_LinkRequestTags,
 * and the entity's body content index (baked sockets, kit link extension). Trivially copyable and at most 32 bytes, so the lag compensation
 * history keeps it with the record; integers only, no tag and no pointer.
 *
 * The LinkRequest step reads the target's from the victim view and the initiator's from FGSTN_ResolvedHit::AttackerExtensions, both of the
 * end of the previous frame, so the attacker's prediction and the victim's resolution decide on the same bytes.
 */
struct GSTNLINKNP_API FGSTN_LinkSnapshotExt
{
	/** The type word a reader validates ("LK") */
	static constexpr uint16 Magic = 0x4C4B;

	uint16 TypeWord = Magic;

	/** FGSTN_LinkAttach table index of the entity's body content; 0 = none (its sockets read as its root) */
	uint16 BodyIndex = 0;

	/** The Body link's partner entity id (a fighter's combat entity id), INDEX_NONE when not linked */
	int32 PartnerId = INDEX_NONE;

	/** The Body link's formation frame */
	int32 LinkFrame = 0;

	/** The entity cannot be grabbed before this frame */
	int32 GrabImmunityEndFrame = 0;

	/** Bit i = the entity carried FGSTN_LinkRequestTags::Get()[i] at the end of the previous frame */
	uint32 RequestTagMask = 0;

	uint8 BodyTypeIndex = 0;
	uint8 BodyRole = 0;
	uint8 BodyPhase = 0;
	uint8 BodySerial = 0;
	uint8 BodyFlags = 0;
	uint8 HandTypeIndex = 0;
	uint8 HandRole = 0;

	/** GSTNLinkSnapshotBits */
	uint8 Bits = 0;

	bool IsValid() const { return TypeWord == Magic; }
	bool IsParked() const { return (Bits & GSTNLinkSnapshotBits::Parked) != 0; }

	/** The two channels as link states (the summary: type, role, phase, partner, serial, frame, flags) */
	FGSTN_LinkStates ToLinkStates() const;

	/** Fills the summary from a fragment (Body, Hand, immunity); keeps BodyIndex, RequestTagMask and Bits */
	void SetFromFragment(const FGSTN_LinkFragment& Fragment);

	/** The extension of the victim view, or null */
	static const FGSTN_LinkSnapshotExt* FromVictim(const FGSTN_HitVictimView& Victim);

	/** The initiator's extension of a hit (its attacker), or null */
	static const FGSTN_LinkSnapshotExt* FromAttacker(const FGSTN_ResolvedHit& Incoming);
};

static_assert(sizeof(FGSTN_LinkSnapshotExt) <= 32, "the link snapshot extension must fit the snapshot blob");

/**
 * The tags a link request may read on the target (and the initiator) side, in one fixed table, so the snapshot carries them as a 32-bit mask
 * (no tag in a snapshot POD): the generic link tags (Combat.Immunity.Grab / .Latch, Combat.Lock.Link, Combat.Part.GrabLocked by name), then
 * every tag the registered definitions' RequestRequirements name and every tag the default rules of their grab and latch rules name
 * (Requirements, and IgnoreTagNames resolved by name), sorted by tag name, at most 32.
 *
 * Content, not state: built from the link registry (identical on every machine) and rebuilt when the registry's signature changes. A tag a
 * kit's own rules name that no definition names is not in the table and reads as absent (F10 deviation).
 */
struct GSTNLINKNP_API FGSTN_LinkRequestTags
{
	static constexpr int32 MaxTags = 32;

	/** The table, built on first use */
	static const TArray<FGameplayTag>& Get();

	/** The mask of the table's tags that Tags holds */
	static uint32 MakeMask(const FGameplayTagContainer& Tags);

	/** Adds the table's tags of Mask to Out */
	static void AppendTags(uint32 Mask, FGameplayTagContainer& Out);

	/** Forgets the table (tests that change the registry) */
	static void Invalidate();
};
