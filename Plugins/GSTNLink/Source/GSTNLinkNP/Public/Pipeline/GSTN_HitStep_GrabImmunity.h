// Links, Network Prediction half.

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitResolution.h"
#include "GSTN_HitStep_GrabImmunity.generated.h"

/**
 * The GrabImmunity step of the hit resolution pipeline (m6 spec 9 step 3): a Grab hit (Combat.HitType.Grab or below) on a victim that is
 * immune to grabs Stops with no registration, so it whiffs on both sides (no hitstop, no damage, no link request). Immune = the victim's link
 * snapshot says GrabImmunityEndFrame is after the hit's stamped frame (the stamp set when its last grab ended), or it carries
 * Combat.Immunity.Grab (a boss, a granted immunity). Every other hit Continues. Pure: the victim view and the hit in, the result out; the
 * initiator's prediction and the victim's resolution read the same snapshot bytes. After HurtState, before LinkRequest.
 */
UCLASS(meta = (DisplayName = "Grab Immunity"))
class GSTNLINKNP_API UGSTN_HitStep_GrabImmunity : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;
};
