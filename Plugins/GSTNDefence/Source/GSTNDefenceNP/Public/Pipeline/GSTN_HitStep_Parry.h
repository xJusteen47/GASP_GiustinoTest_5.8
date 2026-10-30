// Defence, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitResolution.h"
#include "GSTN_HitStep_Parry.generated.h"

/**
 * The Parry step of the hit resolution pipeline (m6 spec 5.4, 9 step 5): the one parry resolution of both sources, the shield's perfect
 * shield (the release- or press-anchored window, on a hit that landed on the bubble) and a move's ParryWindow track (the stamped window,
 * on a hurtbox the track covers). Both go through UGSTN_DefenceMath::ResolveParry on the victim's defence snapshot at the hit's stamped
 * frame, so the attacker's prediction and the victim's resolution read the same bytes and decide the same thing.
 *
 * On a parry: the hit is nullified (bParried, no damage, no knockback, no hitstun, the shield value untouched) and Stops; both sides get the
 * profile's hitstop (a knockback result that does not stagger); a Punish entry {Parried, AttackerStunFrames of the hit class} goes back to
 * the attacker's inbox (none when the profile gives no stun: the "advantage only" variant); a FGSTN_DefenceHitOutcome tells the applier
 * the defender side by source (Shield: ParryRecoveryEndFrame; Move: the window closes after the hit with bCloseOnParry) and raises
 * Combat.Event.Parried with the source as its magnitude. The defender's parry meter is not applied (deviation of F7: the victim has no
 * meter request path yet).
 *
 * Runs in every mode: with the shield off the shield source is never available, a move window still parries. After HurtState (an
 * Intangible window needs no parry) and before Shield and ComputeKnockback. Pure: the view in, the result out.
 */
UCLASS(meta = (DisplayName = "Parry"))
class GSTNDEFENCENP_API UGSTN_HitStep_Parry : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;
};
