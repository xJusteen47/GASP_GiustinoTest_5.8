// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Data/GSTN_PartDefinition.h"
#include "Data/GSTN_PenetrationTable.h"

/** One finding of a validation pass */
struct FGSTN_PartsIssue
{
	bool bError = false;
	FString Message;
};

/** What a validation pass found. Pure data, so tests and the editor module read it without an asset or a validation context */
struct GSTNPARTSCORE_API FGSTN_PartsValidationReport
{
	TArray<FGSTN_PartsIssue> Issues;

	void AddError(const FString& Message);
	void AddWarning(const FString& Message);

	int32 NumErrors() const;
	int32 NumWarnings() const { return Issues.Num() - NumErrors(); }
	bool HasErrors() const { return NumErrors() > 0; }

	/** Whether some issue of the given severity contains Text (case-insensitive) */
	bool Contains(bool bError, const TCHAR* Text) const;
};

/**
 * The rules a part set and a penetration table must obey (combat-parts.md §3.1, plan P2), as pure functions over plain arrays.
 * The assets' IsDataValid call these and forward the findings; the editor module and the minion bridge planned for M8 (not built) call them with their
 * own limits.
 */
struct GSTNPARTSCORE_API FGSTN_PartsValidation
{
	/**
	 * Checks the parts of one set. KnownBones, when given, are the bones of the preview mesh: a bone that is not in it is a
	 * warning (presentation only). MaxPartsAllowed is GSTN_PartsLimits::MaxParts, or MaxMinionParts for a minion set
	 */
	static void ValidateParts(TConstArrayView<FGSTN_PartDefinition> Parts, const FGSTN_GrabGate& GrabGate,
		const FGSTN_PartReaction& BodyReaction, const TSet<FName>* KnownBones, int32 MaxPartsAllowed,
		FGSTN_PartsValidationReport& Report);

	/**
	 * The extra rules of a part set flagged for minions (combat-parts.md §5.5, plan P15), on top of ValidateParts: more than
	 * GSTN_PartsLimits::MaxMinionParts parts is an error (MinionMaskOver8: the store keeps a uint8 mask and 8 damage bytes), and so is
	 * a Stats break effect (a minion has no gameplay effects; the conversion rule is "rejected", see FGSTN_PartsMinionRules)
	 */
	static void ValidateForMinions(TConstArrayView<FGSTN_PartDefinition> Parts, FGSTN_PartsValidationReport& Report);

	/** Checks the bands of one penetration table as authored (before Normalize): empty, unsorted, duplicate MinDelta, scales, gaps */
	static void ValidateBands(TConstArrayView<FGSTN_PenetrationBand> Bands, FGSTN_PartsValidationReport& Report);
};
