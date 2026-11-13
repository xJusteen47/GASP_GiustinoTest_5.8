// Destructible parts, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitExtensions.h"

/**
 * The attacker's answer to "does this hit bounce?" for parts (combat-parts.md §3.4, §4.4): Ricochet when the part the attacker's overlap
 * chose falls in a penetration band that ricochets, Normal otherwise.
 *
 * The attacker can predict this exactly because everything the band depends on is either static (the hit's penetration, the part's
 * hardness, the table) or replicated (the part set and the broken mask in the snapshot extension). It reads the verdict the part resolve
 * step already wrote into the pipeline run's outcome entries (FGSTN_PartHitOutcome with the Ricochet flag) instead of deciding a second
 * time, so the step and the seam can never disagree about which band a hit fell in. A hit the step did not reach (blocked, parried,
 * intangible, a victim with no part set) has no entry and is Normal.
 *
 * Consequences, all owned by the combat core: the attacker recoils with the attack's ricochet response, no hitstop opens on either side,
 * no hit confirm is raised and the inbox entry of the victim is flagged, so the victim only plays the cue and gets the hit event. Pure and
 * const like every hook of the pipeline: no world, no state, no net mode.
 */
class GSTNPARTSNP_API FGSTN_PartsAttackerOutcome : public IGSTN_AttackerOutcomeExtension
{
public:
	virtual FName GetName() const override;
	virtual EGSTN_AttackerHitOutcome Evaluate(const FGSTN_HitContextView& View) const override;
};
