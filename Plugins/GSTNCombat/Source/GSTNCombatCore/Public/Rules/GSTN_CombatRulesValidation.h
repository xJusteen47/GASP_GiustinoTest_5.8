// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_HitResolution.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "Hits/GSTN_ReactionValidation.h"

class UGSTN_CombatRules;
class UGSTN_HurtboxSetDefinition;

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
	/**
	 * Hit columns every matrix printer shares, so two printouts of two assets can be compared: a light (damage 4, 450 cm/s), a medium (9, 900)
	 * and a heavy hit (16, 1500 with growth). Fixed numbers; they are a ruler, not content
	 */
	static TArray<FGSTN_ReactionMatrixHit> MakeStandardMatrixHits();

	/** The rules' matrix against three victim weights (light 60, medium 100, heavy 160) and the standard hit columns. IsDataValid of the rules prints it as an info message */
	static FString PrintStandardMatrix(const UGSTN_CombatRules& Rules);

	/**
	 * The matrix of a hurtbox set: one row per hurtbox at the weight of a medium combatant (100) times the hurtbox's WeightScale, with the hurtbox's own
	 * stagger threshold and reaction cap when it sets them and the set's otherwise (the other scales of a hurtbox are not applied: only the dials the tiers read).
	 * IsDataValid of the set prints it as an info message, which is how a designer sees "this head launches, this foot never staggers" before a PIE
	 */
	static FString PrintHurtboxSetMatrix(const UGSTN_HurtboxSetDefinition& Set, const UGSTN_CombatRules& Rules);

	/**
	 * A hurtbox set:
	 *  - no hurtbox, more than 255 (the index of a hit is a byte), an empty or duplicated name (errors)
	 *  - a shape without volume (error), a hurtbox with no Combat.Hurtbox tag (warning: no modifier track can address it by tag)
	 *  - a negative scale in a reaction scale (error)
	 *  - a weight curve whose sample count is neither 0 nor 64 (error)
	 *  - the pending "hurtbox + part" rule needs the part type of GSTNParts, which this branch does not have; the hook is this function
	 */
	static void ValidateHurtboxSet(const UGSTN_HurtboxSetDefinition& Set, TArray<FGSTN_ReactionMessage>& OutMessages);
};
