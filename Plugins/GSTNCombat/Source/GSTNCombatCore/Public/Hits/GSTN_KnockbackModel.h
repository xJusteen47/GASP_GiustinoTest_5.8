// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "GSTN_KnockbackModel.generated.h"

/**
 * Turns one hit on one victim into a reaction: knockback, tier, hitstun, hitstop (combat-m4-hits 3). One pure const function
 * of the input struct: no world, no actor, no component, no random. The victim-side modifiers are written by pipeline steps
 * into the input and read here exactly once, in the order of "Weight, stagger and juggle":
 *
 *   Weight x WeightScale -> knockback K -> x KnockbackScale -> stagger test (force x StaggerScale against the threshold)
 *   -> tier from K (capped by MaxReaction) -> hitstun x HitstunScale -> hitstop x HitstopScale.
 *
 * A model decides only the first step (what K is); the rest is shared, in protected helpers, so every model evaluates the
 * rules once and the same way. Blueprint classes may override Compute to replace the whole thing (the hook is pure: nothing in
 * it may read the world).
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class GSTNCOMBATCORE_API UGSTN_KnockbackModel : public UObject
{
	GENERATED_BODY()

public:
	/** The constants of the stagger, hitstop, tier and hitstun rules, shared by every model */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	FGSTN_ReactionTuning Tuning;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GSTN|Hits")
	FGSTN_KnockbackResult Compute(const FGSTN_KnockbackInput& Input) const;
	virtual FGSTN_KnockbackResult Compute_Implementation(const FGSTN_KnockbackInput& Input) const;

	/** True when the model counts the victim's damage percent itself: it must not meet a non flat health weight curve (damage counted twice) */
	virtual bool UsesPercentTerm() const { return false; }

protected:
	/**
	 * The knockback magnitude before the hurtbox's KnockbackScale, in cm/s. The weight is already in the input; models that use
	 * it read GetEffectiveWeight.
	 */
	virtual double ComputeKnockback(const FGSTN_KnockbackInput& Input) const PURE_VIRTUAL(UGSTN_KnockbackModel::ComputeKnockback, return 0.0;);

	/** Rule 1 and 2, then 3 and 4 around the model's K: scale, stagger test, tier, velocity, hitstun, hitstop */
	FGSTN_KnockbackResult ResolveRules(const FGSTN_KnockbackInput& Input, double Knockback) const;

	/** max(Weight x WeightScale, MinWeight) */
	double GetEffectiveWeight(const FGSTN_KnockbackInput& Input) const;

	/** FixedKnockback when it is used, else BaseKnockback: the authored knockback that has no victim in it */
	static double GetNominalKnockback(const FGSTN_HitData& Hit);

	/** FixedKnockback when used, else BaseKnockback + KnockbackGrowth x Damage */
	static double GetAuthoredBracket(const FGSTN_HitData& Hit);

	/** The stagger force of the attack: belongs to the attack, never to the victim */
	double ComputeStaggerForce(const FGSTN_HitData& Hit) const;
};
