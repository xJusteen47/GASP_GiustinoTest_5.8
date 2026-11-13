// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_ReactionValidation.h"

class UGSTN_CombatRules;
class UGSTN_PartSetDefinition;

/**
 * Checks of a rules asset's hit pipeline that only make sense with the parts plugin (combat-parts.md §5.3). The combat rules
 * validator already knows PartResolve by name (it must not run after ComputeKnockback, and HealthWeight next to it is reported);
 * these are the rest. Steps are compared by their short name, the class name without the GSTN_HitStep_ prefix, so a Blueprint
 * subclass of a step is not recognised (it is reported as unknown by the combat validator anyway).
 *
 * Errors make the data invalid, warnings are for the designer. Pure: nothing is loaded.
 */
struct GSTNPARTSCORE_API FGSTN_PartsPipelineValidator
{
	/**
	 *  - PartResolve before Parry or Shield (error): a blocked or parried hit would damage a part
	 *  - PartResolve together with ApplyDamage (error): the damage would be requested twice, once whole and once as the part's share
	 */
	static void Validate(const UGSTN_CombatRules& Rules, TArray<FGSTN_ReactionMessage>& OutMessages);

	/**
	 * A kit's part set under rules whose pipeline has no PartResolve step (error): the parts would never take a hit and every
	 * hit would go to the body.
	 */
	static void ValidateKit(const UGSTN_PartSetDefinition& PartSet, const UGSTN_CombatRules& Rules, TArray<FGSTN_ReactionMessage>& OutMessages);

	/** Whether the pipeline of the rules lists the PartResolve step */
	static bool HasPartResolve(const UGSTN_CombatRules& Rules);
};
