// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitResolution.h"
#include "GSTN_HitStep_Shield.generated.h"

/**
 * The Shield step of the hit resolution pipeline (m6 spec 5.3, 9 step 6), after Parry and before ComputeKnockback. It acts only where the
 * shield is available (mode and kit: skipped entirely where the mode disables it) and the victim's shield blocks at the hit's stamped
 * frame (raised past its startup, not breaking: the snapshot's Blocks flag, never the resolution frame), on an Enemy or Neutral hit.
 *
 *  - A hit whose hurtbox is the bubble (an extra hurtbox, index >= 0x80, which the attacker's overlap picks first and which ignores the
 *    non-blockable and grab-like hit types) is blocked: bBlocked, no damage, no knockback, no hitstun; shield damage = ShieldDamageOf(the
 *    hit's ShieldDamage or Damage x ShieldDamagePerDamage, the victim's IncomingShieldDamageMultiplier, the relationship scale); hitstop on
 *    both sides = the knockback model's x ShieldHitstopMultiplier; a FGSTN_DefenceHitOutcome carries the new value (re-stamped lower, the
 *    bubble keeps its size), the shield stun (ShieldStunFrames(Damage) after the hitstop) and the push back along the hit (no tumble, over
 *    PushbackFrames). Combat.Event.Blocked. Stops.
 *  - The new value at or below zero breaks the shield: the outcome says Broken (the applier sets Breaking, BreakEndFrame, and asks the
 *    victim step for BreakLaunch along the victim's local up as a launch that never tumbles). Combat.Event.ShieldBroken. Stops.
 *  - A hit on a BODY hurtbox while the shield is up (only possible with a kit radius that leaves one uncovered) Continues as an ordinary
 *    hit: no event, the shield value untouched (the hitstun it causes ends the shield).
 *
 * The attacker's meter for a blocked hit follows the combat rules' per-damage meter; the defender's small gain is not applied (F7 deviation).
 * Pure: the view in, the result out; the attacker's prediction runs the same function on the same snapshot.
 */
UCLASS(meta = (DisplayName = "Shield"))
class GSTNDEFENCENP_API UGSTN_HitStep_Shield : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;
};
