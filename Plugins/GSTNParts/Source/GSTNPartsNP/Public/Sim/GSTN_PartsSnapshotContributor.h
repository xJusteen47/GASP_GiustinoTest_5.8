// Destructible parts, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "World/GSTN_StepSnapshot.h"

/**
 * Adds the parts of an entity to the combat step snapshot (combat-parts.md §5.2): a FGSTN_PartsSnapshotExt, the state of the parts
 * fragment at the END OF F-1 (the snapshot is built from the end-of-previous-frame sync state, so no attacker's order within the step
 * changes what another reads), plus the derived GrabLocked bit and the part set. The combat snapshot keeps it in a 32 byte blob, the
 * lag compensation history copies the blob with the record, and a rewound attacker therefore hits the parts as they were then.
 *
 * It is the one snapshot extension type the combat snapshot supports (type id GSTNFeatureIds::Parts). An entity without a part set
 * contributes nothing; an entity with a set and no fragment yet contributes a pristine state, which is what lets the first hit on a
 * fresh enemy resolve against its parts.
 *
 * ModifyEntry applies the hurtbox policy of broken parts (combat-parts.md §3.3): the hurtboxes of a broken part whose policy hides them
 * become Intangible in the entry, so an attacker's overlap passes through them. A Keep part keeps its boxes and a hit on it lands on the
 * broken part. The hurtbox states are derived from the mask here and never stored.
 *
 * Reads the context only: no world, no ability system, no net mode, the same answer on the host, the clients and a re-simulation.
 */
class GSTNPARTSNP_API FGSTN_PartsSnapshotContributor : public IGSTN_SnapshotContributor
{
public:
	virtual uint8 GetTypeId() const override;
	virtual void Contribute(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotExtensionBlob& Blob) const override;
	virtual void ModifyEntry(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotEntry& Entry) const override;
};
