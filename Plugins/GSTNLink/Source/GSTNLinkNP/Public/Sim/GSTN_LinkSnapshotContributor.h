// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "State/GSTN_LinkState.h"
#include "World/GSTN_StepSnapshot.h"

struct FGSTN_LinkSnapshotExt;
class UGSTN_LinkTypeDefinition;

/**
 * Adds the link state of every attached or linked entity to the step snapshot (m6 spec 9: the snapshot carries Link.Role, GrabImmunityEnd and
 * the immunity tags; spec seam D9), and places a parked entity on its partner.
 *
 *  - Contribute: FGSTN_LinkSnapshotExt from the link fragment of the end of the previous frame (both channels' summaries, the Body partner,
 *    the grab immunity stamp), the entity's body content index (FGSTN_LinkAttach), and the request tags it carried at the end of the previous
 *    frame as a mask (its ability system's owned tags against FGSTN_LinkRequestTags, read at the snapshot build like the defence's immunity
 *    tags; a rebuild in the victim drain keeps the previous mask). Parked / BlocksActions bits from the definition's role data. An entity
 *    with neither a fragment nor attached content contributes nothing.
 *  - FinalizeSnapshot (the ANCHOR SUBSTITUTION of m6 spec 7.3): an entity whose Body role is the definition's parked role gets its entry's
 *    transform replaced by the parked transform: partner's snapshot transform x the partner's BAKED socket of the role (FGSTN_LinkBodyData,
 *    reference pose) x the role's offset (UGSTN_LinkMath::TryGetParkedTransform). So its hurtboxes, its broadphase cell and its history
 *    record follow the partner on every machine from the same bytes, whatever its capsule (parked at its last real position) or its mesh
 *    (attached to the live socket for presentation) do. It depends on the partner's transform and the definition only, never on the
 *    partner's role, so the grabbed is placed at F+1 while the grabber has no role yet, and a latch (role 1 parked) uses the same code.
 *
 * Deterministic: the fragments, the registry and the attach table (content), the snapshot entries; no live mesh, no Mover query.
 */
class GSTNLINKNP_API FGSTN_LinkSnapshotContributor : public IGSTN_SnapshotContributor
{
public:
	virtual uint8 GetTypeId() const override;
	virtual void Contribute(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotExtensionBlob& Blob) const override;
	virtual void FinalizeSnapshot(int32 Frame, TArrayView<FGSTN_SnapshotEntry> Entries) const override;

	/** The parked placement of an entity from its partner's entry: false when Ext is not parked or its definition is unknown */
	static bool ComputeParkedTransform(const FGSTN_LinkSnapshotExt& Ext, const FGSTN_SnapshotEntry& Partner, FTransform& OutTransform);

	/** Parked and BlocksActions bits of a Body link summary: the role is the definition's parked one and the link is live (Active or Exiting) */
	static uint8 ComputeBits(const UGSTN_LinkTypeDefinition* Definition, uint8 Role, EGSTN_LinkPhase Phase);
};
