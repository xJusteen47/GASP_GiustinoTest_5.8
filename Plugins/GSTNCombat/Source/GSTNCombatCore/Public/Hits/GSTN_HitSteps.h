// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitResolution.h"
#include "GSTN_HitSteps.generated.h"

/**
 * The standard hit resolution steps (combat-m4-hits 3). The standard rules list them in this order:
 * TeamFilter, HurtState, HurtboxReaction, HealthWeight, ComputeKnockback, ApplyDamage, Hitstop, MeterGain.
 *
 * Placeholders, documented and known to the rules validator by name but not classes (a plugin that provides one adds the class):
 *   Shield       after HurtboxReaction, before PartResolve: blocks, writes bBlocked and the shield damage (M6)
 *   Parry        before Shield: writes bParried (M6)
 *   PartResolve  after Shield, before ComputeKnockback: the GSTNParts step that resolves the part, replaces HealthWeight (M4b)
 * A name in the pipeline with no class behind it is reported by the validator and skipped at run time.
 *
 * Every class here is stateless and reads only its view, like any step.
 */

/** Lets the hit through only on a hittable relationship: the rules' HittableRelationships, plus Ally when friendly fire is on */
UCLASS(meta = (DisplayName = "Team Filter"))
class GSTNCOMBATCORE_API UGSTN_HitStep_TeamFilter : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;
};

/**
 * Acts on the state of the hurtbox that was hit: Intangible lets the hit pass (it did not land), Invincible keeps the hit
 * (hitstop, confirm) but removes damage, knockback and hitstun. An Armor window is read by HurtboxReaction, which raises the
 * threshold; this step has nothing to write for it.
 */
UCLASS(meta = (DisplayName = "Hurt State"))
class GSTNCOMBATCORE_API UGSTN_HitStep_HurtState : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;
};

/**
 * The "where" of a hit. Combines the hurtbox that was hit (its reaction scales, threshold, cap) with the entity defaults of its
 * set and the rules: the threshold is the first set among hurtbox, set, rules; the cap is the lowest of hurtbox and set; an Armor
 * window raises the threshold to the larger of the two.
 */
UCLASS(meta = (DisplayName = "Hurtbox Reaction"))
class GSTNCOMBATCORE_API UGSTN_HitStep_HurtboxReaction : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;
};

/**
 * Weight that falls with the damage taken. Writes WeightScale = Curve(Progress), where the curve is the hurtbox set's if authored,
 * else the rules' default, and Progress comes from the victim as it stood BEFORE this hit (UGSTN_CombatRules::GetDamageProgress).
 * With no authored curve it writes nothing (a flat curve is the same thing). The standard pipeline holds it; a game replaces
 * or removes it, and GSTNParts replaces it with PartResolve.
 */
UCLASS(meta = (DisplayName = "Health Weight"))
class GSTNCOMBATCORE_API UGSTN_HitStep_HealthWeight : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;
};

/**
 * Calls the rules' knockback model. The model needs the launch direction and Sakurai3D needs the knockback to pick the angle, so
 * with Sakurai3D the step calls Compute twice (once with no direction to read K, once with the direction); the model is one pure
 * function and K does not depend on the direction, so the second result is the whole answer. Without Sakurai3D it calls once.
 * A victim already in tumble has its knockback scaled by the rules' AirborneJuggleScale. No model in the rules writes nothing.
 */
UCLASS(meta = (DisplayName = "Compute Knockback"))
class GSTNCOMBATCORE_API UGSTN_HitStep_ComputeKnockback : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;
};

/**
 * Writes the hit's damage and the damage request (the effect class comes from the rules' DamageEffect), and passes the hit's extra
 * effects on. Nothing when the hit did not land or an Invincible window removed the damage. The damage is the attack's own: the
 * victim's accumulated damage is not changed here, it counts from the next hit on.
 */
UCLASS(meta = (DisplayName = "Apply Damage"))
class GSTNCOMBATCORE_API UGSTN_HitStep_ApplyDamage : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;
};

/**
 * Turns the model's hitstop frames into freeze requests for the victim and the attacker (the rules' FreezeEffect). Hitstop
 * applies whether or not the victim staggered; a hurtbox that must not pause sets HitstopScale to 0.
 */
UCLASS(meta = (DisplayName = "Hitstop"))
class GSTNCOMBATCORE_API UGSTN_HitStep_Hitstop : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;
};

/** The attacker's meter gain: the rules' MeterGainPerDamage times the damage, as a meter gain and a request with the MeterGainEffect. Nothing at the default 0 */
UCLASS(meta = (DisplayName = "Meter Gain"))
class GSTNCOMBATCORE_API UGSTN_HitStep_MeterGain : public UGSTN_HitResolutionStep
{
	GENERATED_BODY()

public:
	virtual EGSTN_StepResult Resolve_Implementation(const FGSTN_HitContextView& View, FGSTN_StepResult& Out) const override;
};
