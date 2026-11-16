// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitResolution.h"
#include "GSTN_HitStep_PartResolve.generated.h"

/**
 * The part resolve step of the hit resolution pipeline (combat-parts.md §4.2, §4.4, §5.3). It runs after the steps that stop a
 * hit (TeamFilter, HurtState, Shield, Parry) and before ComputeKnockback, so a blocked, parried, grabbed or intangible hit never
 * damages a part.
 *
 * What it does for an entity that has a part set (the parts snapshot extension of the victim view):
 *  - finds the part from the hurtbox the attacker's overlap chose (HurtboxIndex, never re-derived here) and the band from the
 *    hit's penetration against the part's hardness;
 *  - applies the hit to a copy of the part state through UGSTN_PartMath::ApplyHit (damage, break, cascade);
 *  - writes the reaction modifiers: the effective weight (falls with the damage taken and the mass lost by broken parts) times
 *    the juggle factors, the knockback and hitstop scales, the band's stagger scale, the part's stagger threshold (else the
 *    body's, else unset so the rules' global value applies) and the lower of the part's and the body's reaction cap;
 *  - writes the main pool damage (MainDelta) as the hit's damage and the damage request, and one FGSTN_PartHitOutcome for the
 *    outcome applier (the new state, the events). A ricochet band stops the pipeline: nothing lands, nothing is frozen, the
 *    entry only tells the applier and the attacker's seam what happened.
 *
 * It REPLACES two M4 steps in a rules asset that uses parts: HealthWeight (both write the weight scale; this one adds the lost
 * mass) and ApplyDamage (that step requests the hit's own damage, which would ignore the part's share). For an entity without a
 * part set the step writes exactly what ApplyDamage writes, so one pipeline serves both kinds of entity. The rules validator
 * (FGSTN_PartsPipelineValidator) reports the ordering and the replacement.
 *
 * Like every step it is pure: the view in, the result out, no world, actor or component, no state of its own. The attacker's
 * hitstop uses only static data (the band and the part's scales), never the broken mask or the damage, so the attacker's prediction
 * agrees with the authority (combat-parts.md §4.4).
 */
UCLASS(meta = (DisplayName = "Part Resolve"))
class GSTNPARTSCORE_API UGSTN_HitStep_PartResolve : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;
};
