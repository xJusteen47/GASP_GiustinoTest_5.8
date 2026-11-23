// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitResolution.h"
#include "State/GSTN_LinkState.h"
#include "GSTN_HitStep_LinkBreakOnHit.generated.h"

class UGSTN_LinkTypeDefinition;

/**
 * The LinkBreakOnHit step of the hit resolution pipeline (m6 spec 9 step 8, 7.5 "Hit"): a hit that landed on a victim holding a live Body link
 * marks that link PendingBreak with reason Hit when the role rules of the link type say so (FGSTN_LinkBreakRules::HitBreaks: the victim's role
 * has an entry, the knockback the hit computed reaches MinKnockbackToBreak, and a hit from the linked partner counts only with
 * bPartnerHitsBreak, so a pummel keeps the grab). It never stops the hit and changes nothing else: the mark travels as an outcome extension
 * (FGSTN_LinkBreakOutcome) to the victim's link fragment, and the watchdog breaks the link two frames later through its one break path.
 *
 * Pure: the victim's link snapshot (role, serial, partner, type) and the hit's outcome in, the result out; the same bytes on the attacker's
 * prediction and the victim's resolution. After MeterGain (the knockback is known), so last in the standard pipeline.
 */
UCLASS(meta = (DisplayName = "Link Break On Hit"))
class GSTNLINKNP_API UGSTN_HitStep_LinkBreakOnHit : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;

	/** The rule as a function: whether a hit with this knockback, from the partner or not, breaks the link for the holder of VictimRole (1 or 2) */
	static bool QualifiesForBreak(const UGSTN_LinkTypeDefinition& Definition, uint8 VictimRole, bool bFromPartner, float Knockback);
};
