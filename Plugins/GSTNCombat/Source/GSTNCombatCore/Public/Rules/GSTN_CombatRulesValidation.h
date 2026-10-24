// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_HitResolution.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "Hits/GSTN_ReactionValidation.h"

class UGSTN_CombatRules;

/** A victim row of the reaction matrix: the dials that decide which hits stagger it and how far */
struct FGSTN_ReactionMatrixVictim
{
	FString Name;
	float Weight = 100.f;
	FGSTN_StaggerThreshold StaggerThreshold;
	EGSTN_HitReaction MaxReaction = EGSTN_HitReaction::Launch;
};

/** A hit column of the reaction matrix */
struct FGSTN_ReactionMatrixHit
{
	FString Name;
	FGSTN_HitData Hit;
};

/**
 * Pure checks of a rules asset (combat-m4-hits 3, 10). Errors make the asset invalid; warnings are for the designer. The
 * messages are the reaction validation's, so one list serves both.
 */
struct GSTNCOMBATCORE_API FGSTN_CombatRulesValidation
{
	/** Names of the documented placeholder steps a plugin may provide: Shield, Parry, PartResolve */
	static bool IsKnownPlaceholderName(FName StepName);

	/** The step's name as the validator and the docs use it: the class name without the U prefix (GSTN_HitStep_TeamFilter), the asset name of a path that does not load */
	static FName GetStepName(const TSoftClassPtr<UGSTN_HitResolutionStep>& Entry);

	/**
	 * The hit pipeline and the tuning:
	 *  - an entry with no class, a class that is not found, abstract or not a hit step (error), or a placeholder whose plugin is not
	 *    loaded (warning); either way it is skipped at run time
	 *  - a step that runs after ComputeKnockback but writes what the model reads: HurtState, HurtboxReaction, HealthWeight,
	 *    Shield, Parry, PartResolve (error); Hitstop before it (warning, no frames yet)
	 *  - HealthWeight and PartResolve both present (warning: two weights)
	 *  - a step listed twice (warning)
	 *  - the model's tuning (threshold order, weights) and a percent term model next to a non flat health curve
	 *  - MaxHealth not positive, a non positive MaxRewindMs with lag compensation on (error)
	 */
	static void Validate(const UGSTN_CombatRules& Rules, TArray<FGSTN_ReactionMessage>& OutMessages);

	/** A hitstop override above the model's MaxHitstop is clamped: say so (warning) */
	static void ValidateHitstopOverride(const FGSTN_HitData& Hit, const FGSTN_ReactionTuning& Tuning, TArray<FGSTN_ReactionMessage>& OutMessages);

	/**
	 * The reaction matrix: for every victim weight, which hits stagger and which tier they reach (combat-m4-hits 10), at full health
	 * with the rules' model. One header line and one line per victim, cells separated by " | ": "-" = no stagger, "In" =
	 * stagger in place, "Push" = push back, "Launch" = launch, with a "T" suffix when it tumbles. Plain text so a test compares it
	 * with a golden string and a designer reads it in the log.
	 */
	static FString PrintReactionMatrix(const UGSTN_CombatRules& Rules, const TArray<FGSTN_ReactionMatrixVictim>& Victims, const TArray<FGSTN_ReactionMatrixHit>& Hits);
};
