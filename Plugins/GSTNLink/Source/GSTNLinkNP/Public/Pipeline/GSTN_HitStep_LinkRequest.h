// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitResolution.h"
#include "GSTN_HitStep_LinkRequest.generated.h"

/**
 * The LinkRequest step of the hit resolution pipeline (m6 spec 7.2, 9 step 4): the target side of link formation, data-driven per link type.
 *
 *  - A hit that names a link type the registry knows (FGSTN_HitData::LinkType; a grab, a latch, a later type such as the drill's latch
 *    variant) is a REQUEST: the definition's rule chain decides (UGSTN_LinkRequirementLibrary::EvaluateRequest: the shared checks, busy
 *    channels, coexistence, Combat.Lock.Link, the definition's RequestRequirements, then its rules in order with the initiator kit's rules).
 *    Accepted -> a FGSTN_LinkHitOutcome for the applier (the target's role 2 from F+1, serial one after the newer of both sides') and an
 *    outgoing LinkResponse{accepted} to the initiator (consumed at F+2). Refused -> LinkResponse{refused} and the hit does not land (bHit
 *    false: the grab whiffs, no hitstop on either side). Either way the step Stops: a link request does no damage and no knockback.
 *  - A Throw or Pummel hit (the plugin's hit types) is valid only from the victim's Body partner on the link of the same serial; anything
 *    else Stops without landing. A valid one Continues to the damage steps.
 *  - Every other hit Continues.
 *
 * What the parked side does with the role (no candidates, the parked mode, the anchor of its hurtboxes) follows from the role data of the
 * definition, never from a branch here, so a latch (initiator parked) and a grab (target parked) are the same request. Simultaneous requests
 * on one target: the first in drain order wins, the next sees the target busy (the victim drain rebuilds the link extension between
 * entries) and is refused. Pure: the view, the hit's attacker extension, the registry and the attach table (content) in, the result out.
 */
UCLASS(meta = (DisplayName = "Link Request"))
class GSTNLINKNP_API UGSTN_HitStep_LinkRequest : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;
};
