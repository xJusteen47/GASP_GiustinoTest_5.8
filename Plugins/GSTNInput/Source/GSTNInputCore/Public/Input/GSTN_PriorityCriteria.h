// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_InputCoreTypes.h"
#include "GSTN_PriorityCriteria.generated.h"

struct FGSTN_InputPattern;
struct FGSTN_InputSlotSpec;
class UGSTN_InputSettings;

namespace GSTNPriority
{
	/** Most criteria a command is ranked by: the inline size of FGSTN_CommandPriority, and the cap on the settings' list */
	constexpr int32 MaxCriteria = 8;
}

/**
 * Everything a priority criterion may look at. Criteria are scored once per slot, when a slot set or a table is built or
 * ranked, never per frame: they see the slot, its pattern and the settings, not the history, the clock or the owner, so
 * the same slot gets the same score on every machine and after a rollback.
 */
struct FGSTN_PriorityContext
{
	const FGSTN_InputSlotSpec& Slot;
	const FGSTN_InputPattern& Pattern;
	const UGSTN_InputSettings& Settings;
};

/**
 * One way of ranking commands that match at once (input.md section 4.1). The criteria listed in
 * UGSTN_InputSettings::PriorityCriteria are compared in order: a command that scores higher on the first criterion wins
 * whatever it scores on the next ones, which only break ties. A new rule ("chords first", "this button first") is a small
 * struct deriving from this one with a Score override; the resolver does not change.
 */
USTRUCT(BlueprintType, meta = (Hidden))
struct GSTNINPUTCORE_API FGSTN_PriorityCriterion
{
	GENERATED_BODY()

	virtual ~FGSTN_PriorityCriterion() = default;

	/** The score of the slot. Higher wins. Must be pure: same context, same score */
	virtual int32 Score(const FGSTN_PriorityContext& Context) const
	{
		return 0;
	}
};

/** The tier of the command tag root, from UGSTN_InputSettings::CommandTiers: a special above an attack whatever the pattern. */
USTRUCT(BlueprintType, meta = (DisplayName = "Tier"))
struct GSTNINPUTCORE_API FGSTN_PriorityCriterion_Tier : public FGSTN_PriorityCriterion
{
	GENERATED_BODY()

	virtual int32 Score(const FGSTN_PriorityContext& Context) const override;
};

/** The sum of the pattern's conditions: crouch + direction above direction above a bare button. */
USTRUCT(BlueprintType, meta = (DisplayName = "Specificity"))
struct GSTNINPUTCORE_API FGSTN_PriorityCriterion_Specificity : public FGSTN_PriorityCriterion
{
	GENERATED_BODY()

	virtual int32 Score(const FGSTN_PriorityContext& Context) const override;
};

/** The slot's PriorityOffset: the designer's manual tie breaker. */
USTRUCT(BlueprintType, meta = (DisplayName = "Manual Offset"))
struct GSTNINPUTCORE_API FGSTN_PriorityCriterion_ManualOffset : public FGSTN_PriorityCriterion
{
	GENERATED_BODY()

	virtual int32 Score(const FGSTN_PriorityContext& Context) const override;
};
