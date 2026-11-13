// Backpacks, Network Prediction half (M11 L7c: the dome blocks hits).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "World/GSTN_StepSnapshot.h"

struct FGSTN_HitVictimView;
struct FGSTN_ResolvedHit;

/**
 * The backpack dome of an entity as the step snapshot carries it (M11 L7c): its own dome when one is up at the snapshot frame, and the domes
 * (by owner entity id) its root stands in. Copied into the combat snapshot's extension blob at the end of the previous frame, kept with the
 * lag compensation record, read by the BackpackDome step through the victim view (the victim's) and the resolved hit (the attacker's).
 *
 * The side of a hit is decided from these lists: both ends read the same snapshot, so the attacker's prediction, the victim and a rewind give
 * one verdict per hit. Fixed size, no pointer: 40 bytes, the blob's whole payload.
 */
struct GSTNBACKPACKNP_API FGSTN_BackpackSnapshotExt
{
	/** 'BKPK' in little endian: the word a reader validates before it trusts the bytes (the views hand out a void pointer) */
	static constexpr uint32 TypeWord = 0x4B504B42u;

	/** Domes one entity can stand in at once; one more is left out, the same cut on every machine (domes are rare) */
	static constexpr int32 MaxInsideDomes = 4;

	uint32 Type = TypeWord;

	/** World space, from the fragment's whole centimetres; meaningful only with bHasDome */
	float DomeCenter[3] = { 0.f, 0.f, 0.f };

	float DomeRadiusCm = 0.f;

	/** The entity holds a dome up at the snapshot frame */
	uint8 bHasDome = 0;

	uint8 NumInside = 0;

	/** Owner entity ids of the domes the root is in, ascending (the snapshot order); the first NumInside are set */
	int32 InsideDomes[MaxInsideDomes] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };

	bool HasDome() const { return bHasDome != 0; }
	FVector GetDomeCenter() const { return FVector(DomeCenter[0], DomeCenter[1], DomeCenter[2]); }

	bool IsInside(int32 DomeOwnerId) const;

	/** Adds a dome to the inside list (a duplicate or one past MaxInsideDomes is ignored) */
	void AddInside(int32 DomeOwnerId);

	void ClearInside();

	/** The victim's extension, or null (no blob, or bytes that are not this type) */
	static const FGSTN_BackpackSnapshotExt* FromView(const FGSTN_HitVictimView& Victim);

	/** The attacker's extension of the hit, or null: an attacker without one stands in no dome */
	static const FGSTN_BackpackSnapshotExt* FromAttacker(const FGSTN_ResolvedHit& Incoming);
};

static_assert(sizeof(FGSTN_BackpackSnapshotExt) <= FGSTN_SnapshotExtensionBlob::MaxBytes, "the backpack snapshot extension must fit a blob");

/**
 * Adds the backpack dome to the combat step snapshot (M11 L7c), feature id GSTNFeatureIds::Backpack:
 *
 *  - Contribute: the entity's own dome from FGSTN_BackpackFragment when it is up at the snapshot frame (end of the previous frame). At a rebuild
 *    in the victim drain the inside list of the earlier blob is kept (it depends on the other entries, which a rebuild does not see).
 *  - AppendExtraHurtboxes: the dome as an outside-only sphere (MakeDomeHurtbox): world-fixed, so made root-relative from the entry's transform,
 *    priority 255 (above any body hurtbox), no ignored hit type (grabs too), Source = Backpack.
 *  - FinalizeSnapshot: for every entry, the domes whose sphere holds its root (entries without a blob get one). Early out when no entry has a
 *    dome, so a match without domes pays one blob lookup per entry and frame.
 *
 * Reads the context and the entries only: no net mode, the same answer on the host, the clients and a re-simulation.
 */
class GSTNBACKPACKNP_API FGSTN_BackpackSnapshotContributor : public IGSTN_SnapshotContributor
{
public:
	virtual uint8 GetTypeId() const override;
	virtual void Contribute(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotExtensionBlob& Blob) const override;
	virtual void AppendExtraHurtboxes(const FGSTN_SnapshotContributorContext& Context, FGSTN_ExtraHurtboxList& Out) const override;
	virtual void FinalizeSnapshot(int32 Frame, TArrayView<FGSTN_SnapshotEntry> Entries) const override;

	/** The entity's own dome at the context's frame (world centre, radius): false without a fragment or with no dome up then */
	static bool ReadDome(const FGSTN_SnapshotContributorContext& Context, FVector& OutCenter, double& OutRadius);

	/** The dome as an extra hurtbox of the entity whose snapshot transform is EntityTransform */
	static FGSTN_ExtraHurtbox MakeDomeHurtbox(const FVector& WorldCenter, double Radius, const FTransform& EntityTransform);
};
