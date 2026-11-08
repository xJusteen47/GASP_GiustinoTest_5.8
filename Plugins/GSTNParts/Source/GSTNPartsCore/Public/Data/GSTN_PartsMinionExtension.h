// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PartDefinition.h"
#include "Data/GSTN_PartsLimits.h"
#include "GSTN_PartsMinionExtension.generated.h"

class UGSTN_PartSetDefinition;
struct FGSTN_PartsValidationReport;

/**
 * What a minion type adds to its definition to lose parts (combat-parts.md §5.5, plan P15). Declared here, pure and free of any
 * GSTNMinion type, so the data and its rules are testable without the minion plugin; the bridge plugin GSTNPartsMinion (P16) hangs it on
 * UGSTN_MinionDefinition. It carries the set and the one decision a minion needs that a fighter does not: which part losses kill.
 *
 * Helldivers-style default (owner, 2026-11-08): LethalParts is empty, so losing any part, the head included, never kills by itself;
 * the minion keeps fighting and dies only when its Health runs out. A designer lists the parts whose loss is fatal (a torso, a core).
 * The Kill flag of an FGSTN_PartBreakEffect_MainDamage is NOT read for minions: lethality is this list and nothing else (validation warns about a
 * bKill on a part that is not listed).
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartsMinionExtension
{
	GENERATED_BODY()

	/** The minion's part set. Must have bForMinions set (validation) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Minion")
	TSoftObjectPtr<UGSTN_PartSetDefinition> PartSet;

	/** Names of the parts whose loss kills the minion. Empty = every part loss keeps the minion fighting (the default, head included) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parts|Minion")
	TArray<FName> LethalParts;

	/** Bit i set for every LethalParts name found in Parts at index i (below MaxMinionParts). Unknown names are skipped; validation reports them */
	uint8 ResolveLethalMask(TConstArrayView<FGSTN_PartDefinition> Parts) const;

	/** Whether a minion with this broken mask must die for the lethal mask. The only lethality rule: a pure integer test */
	static bool IsLethal(uint8 BrokenMask, uint8 LethalMask) { return (BrokenMask & LethalMask) != 0; }

	/** Checks the lethal names against the set (unknown name: error; bKill on a part that is not lethal: warning) */
	void Validate(TConstArrayView<FGSTN_PartDefinition> Parts, FGSTN_PartsValidationReport& Report) const;
};

/**
 * The rules that make a part set minion-ready. Conversion rule for break effects a minion cannot run: a Stats effect (an infinite
 * gameplay effect on the ability system) is REJECTED, not converted. Rejecting is the simpler choice: a minion has no ability system and
 * a "minion stats struct" would be a second stat model to keep in step with M8's. The effects a minion can run are the derived ones
 * (Tags, DisableHurtbox, DisableHitGroups) and the one-shots MainDamage, Launch and Stagger; the bridge applies those from the outcome
 * directly. An unknown or empty entry is left to ValidateParts, which already warns about it.
 */
struct GSTNPARTSCORE_API FGSTN_PartsMinionRules
{
	/** Whether a minion can run this break effect: false only for a Stats effect */
	static bool CanMinionRunBreakEffect(const FInstancedStruct& Effect);
};
