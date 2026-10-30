// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Sim/GSTN_DefenceSnapshotExt.h"
#include "World/GSTN_StepSnapshot.h"

struct FGSTN_DefenceEntityData;
struct FGSTN_DefenceFragment;

/**
 * Adds the defence of an entity to the combat step snapshot (m6 spec 5.3 "Contact", 9): a FGSTN_DefenceSnapshotExt built from the
 * defence fragment at the END OF F-1 and the rules attached to the entity (FGSTN_DefenceAttach), and, while the bubble exists, the
 * shield bubble as an extra hurtbox (UGSTN_DefenceMath::MakeShieldBubbleHurtbox: a root-relative sphere of the rules' RadiusCm, the
 * highest priority, ignoring the non-blockable hit types). The combat snapshot keeps the extension in its blob set and the lag
 * compensation history copies it with the record, so a rewound attacker hits the bubble and reads the shield as they were at the hit's
 * frame; the bubble itself depends only on the rules and the entity transform, which the record carries (the fixed-bubble decision).
 *
 * The bubble exists from the press frame to the release frame plus the rules' ReleaseFrames (or while shield stunned), and never while the
 * shield is broken, nor while the entity is in a hit reaction (hitstun, tumble, splat) or knocked out (the context's action state).
 * The extension is contributed whenever the entity has defence data, bubble or not: the Parry step reads the move window of an entity
 * whose mode has no shield at all.
 *
 * The shield value is evaluated analytically at the snapshot frame (the attribute set's rates at that frame, else the rules'), so the
 * steps need no rates and the attacker's machine reads the same number the victim's does for the same frame. At a rebuild inside the victim
 * drain (no owner) the fragment the appliers just wrote is read as it stands: its stamp IS the frame.
 *
 * Reads the context and the attached content only: no net mode, the same answer on the host, the clients and a re-simulation.
 */
class GSTNDEFENCENP_API FGSTN_DefenceSnapshotContributor : public IGSTN_SnapshotContributor
{
public:
	virtual uint8 GetTypeId() const override;
	virtual void Contribute(const FGSTN_SnapshotContributorContext& Context, FGSTN_SnapshotExtensionBlob& Blob) const override;
	virtual void AppendExtraHurtboxes(const FGSTN_SnapshotContributorContext& Context, FGSTN_ExtraHurtboxList& Out) const override;

	/**
	 * The extension for the context: false when the entity has no defence data (nothing to contribute). The one function both hooks
	 * use, so the flags and the bubble agree. OutData is the attached data the extension was built from (null at a rebuild)
	 */
	static bool BuildExtension(const FGSTN_SnapshotContributorContext& Context, FGSTN_DefenceSnapshotExt& OutExt, const FGSTN_DefenceEntityData*& OutData);

	/** Whether the bubble exists at Frame for a fragment under these rules, outside a hit reaction (the rule of m6 5.3, shared with the tick extension) */
	static bool BubbleExistsAt(const FGSTN_DefenceFragment& Fragment, int32 Frame, int32 ReleaseFrames);
};
