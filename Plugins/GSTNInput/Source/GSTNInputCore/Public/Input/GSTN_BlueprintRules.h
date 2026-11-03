// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_InputCoreTypes.h"
#include "Input/GSTN_BufferResetRules.h"
#include "Input/GSTN_InputConditions.h"
#include "Input/GSTN_PriorityCriteria.h"
#include "UObject/Object.h"
#include "GSTN_BlueprintRules.generated.h"

/**
 * The opt-in Blueprint path of the three extensible rule families (input conditions, priority criteria, buffer reset
 * rules). Those families are C++ structs held as TInstancedStruct, which Blueprint cannot subclass; each gets one wrapper
 * struct that holds a Blueprintable class with a const BlueprintNativeEvent instead. The C++ structs stay the default and
 * the fast path: a wrapper calls the Blueprint VM on the class default object, which costs far more than a struct virtual.
 *
 * What a Blueprint rule must obey, because the rules run inside the simulation (rolled back and re-simulated):
 *  - It answers only from the view struct it is given. No world queries, no actors, no random numbers, no time, no reading
 *    variables it wrote earlier: the class default object is shared by every pawn and every machine, so any state in it
 *    would leak between them and would not be rolled back. The functions are const to say so.
 *  - The same view must give the same answer on the server, on a client and after a rollback.
 *
 * Priority criteria are scored once per slot when a slot set or table is built, so a Blueprint criterion is cheap in
 * practice. Reset rules are evaluated every frame and conditions at every candidate frame of every pattern they sit in,
 * so use those two sparingly and prefer a C++ struct for a rule that many commands share.
 */

// ---------------------------------------------------------------------------------------------------------------------
// Conditions
// ---------------------------------------------------------------------------------------------------------------------

/** What a Blueprint condition may look at: the input around the anchor frame, as plain values. */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_BlueprintConditionView
{
	GENERATED_BODY()

	/** The frame the condition is asked about: the frame of the anchor (the press) of its step */
	UPROPERTY(BlueprintReadOnly, Category = "Condition")
	int32 AnchorFrame = 0;

	/** The latest frame of the history: the frame the command would come out */
	UPROPERTY(BlueprintReadOnly, Category = "Condition")
	int32 Now = 0;

	/** Tolerance in frames around the anchor that the step allows */
	UPROPERTY(BlueprintReadOnly, Category = "Condition")
	int32 SimultaneityFrames = 0;

	/** The buttons that are down during the anchor frame */
	UPROPERTY(BlueprintReadOnly, Category = "Condition", meta = (Categories = "Input.Button"))
	FGameplayTagContainer HeldButtons;

	/** How far the stick is pushed at the anchor frame */
	UPROPERTY(BlueprintReadOnly, Category = "Condition")
	EGSTN_StickZone StickZone = EGSTN_StickZone::Neutral;

	/** Stick direction relative to the character at the anchor frame */
	UPROPERTY(BlueprintReadOnly, Category = "Condition")
	EGSTN_StickDirection FacingDirection = EGSTN_StickDirection::Neutral;

	/** Stick direction on the controller at the anchor frame */
	UPROPERTY(BlueprintReadOnly, Category = "Condition")
	EGSTN_StickDirection StickDirection = EGSTN_StickDirection::Neutral;

	/** Stick direction in level axes at the anchor frame */
	UPROPERTY(BlueprintReadOnly, Category = "Condition")
	EGSTN_StickDirection WorldDirection = EGSTN_StickDirection::Neutral;
};

/**
 * Base of Blueprint input conditions. Override Matches. Read the comment of GSTN_BlueprintRules.h first: the answer must
 * depend on the view alone.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class GSTNINPUTCORE_API UGSTN_InputConditionBlueprint : public UObject
{
	GENERATED_BODY()

public:
	/** Does the condition hold around the anchor frame? Deterministic and a function of View only */
	UFUNCTION(BlueprintNativeEvent, Category = "Condition")
	bool Matches(const FGSTN_BlueprintConditionView& View) const;
	virtual bool Matches_Implementation(const FGSTN_BlueprintConditionView& View) const { return false; }
};

/**
 * A condition written in Blueprint, for the rare check the C++ conditions do not cover. Never an anchor: it is checked
 * relative to the anchor of its step like a direction or a hold. It reports no events to consume and no canonical input,
 * so the slot validator cannot use it to compare slots with each other (it treats the condition as always satisfiable).
 * The slower path: see the comment at the top of this file.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Blueprint (slower)"))
struct GSTNINPUTCORE_API FGSTN_InputCondition_Blueprint : public FGSTN_InputCondition
{
	GENERATED_BODY()

	/** The Blueprint class whose Matches decides */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blueprint")
	TSubclassOf<UGSTN_InputConditionBlueprint> Condition;

	/** Weight of the condition in the automatic priority (the pattern's specificity is the sum of its conditions') */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blueprint", meta = (ClampMin = "0"))
	int32 Specificity = 5;

	/** The buttons the Blueprint looks at, so the slot validation can check the input config has them */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blueprint", meta = (Categories = "Input.Button"))
	FGameplayTagContainer ReferencedButtons;

	virtual bool Matches(const FGSTN_ConditionContext& Ctx, int32 AnchorFrame, FGSTN_ConditionResult& Out) const override;
	virtual int32 GetSpecificity() const override { return Specificity; }
	virtual void GetReferencedButtons(FGameplayTagContainer& OutButtons) const override { OutButtons.AppendTags(ReferencedButtons); }
};

// ---------------------------------------------------------------------------------------------------------------------
// Priority criteria
// ---------------------------------------------------------------------------------------------------------------------

/** What a Blueprint priority criterion may look at: the slot's facts, as plain values. */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_BlueprintPriorityView
{
	GENERATED_BODY()

	/** The command of the slot */
	UPROPERTY(BlueprintReadOnly, Category = "Priority", meta = (Categories = "Command"))
	FGameplayTag Command;

	/** Tier of the command from the settings (and the plugins' contributions) */
	UPROPERTY(BlueprintReadOnly, Category = "Priority")
	int32 Tier = 0;

	/** Sum of the specificity of the pattern's conditions */
	UPROPERTY(BlueprintReadOnly, Category = "Priority")
	int32 Specificity = 0;

	/** The slot's manual priority offset */
	UPROPERTY(BlueprintReadOnly, Category = "Priority")
	int32 ManualOffset = 0;
};

/** Base of Blueprint priority criteria. Override Score. Read the comment of GSTN_BlueprintRules.h first. */
UCLASS(Abstract, Blueprintable, BlueprintType)
class GSTNINPUTCORE_API UGSTN_PriorityCriterionBlueprint : public UObject
{
	GENERATED_BODY()

public:
	/** The score of the slot, higher wins. Deterministic and a function of View only */
	UFUNCTION(BlueprintNativeEvent, Category = "Priority")
	int32 Score(const FGSTN_BlueprintPriorityView& View) const;
	virtual int32 Score_Implementation(const FGSTN_BlueprintPriorityView& View) const { return 0; }
};

/** A priority criterion written in Blueprint. The slower path: see the comment at the top of this file. */
USTRUCT(BlueprintType, meta = (DisplayName = "Blueprint (slower)"))
struct GSTNINPUTCORE_API FGSTN_PriorityCriterion_Blueprint : public FGSTN_PriorityCriterion
{
	GENERATED_BODY()

	/** The Blueprint class whose Score ranks the slots */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blueprint")
	TSubclassOf<UGSTN_PriorityCriterionBlueprint> Criterion;

	virtual int32 Score(const FGSTN_PriorityContext& Context) const override;
};

// ---------------------------------------------------------------------------------------------------------------------
// Buffer reset rules
// ---------------------------------------------------------------------------------------------------------------------

/** What a Blueprint reset rule may look at: this frame's events and the history's summary, as plain values. */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_BlueprintResetView
{
	GENERATED_BODY()

	/** Events of the host simulation this frame (Input.BufferEvent.*) */
	UPROPERTY(BlueprintReadOnly, Category = "Reset", meta = (Categories = "Input.BufferEvent"))
	FGameplayTagContainer EventTags;

	/** Gameplay tags that appeared on the owner this frame */
	UPROPERTY(BlueprintReadOnly, Category = "Reset")
	FGameplayTagContainer TagsAdded;

	/** The frame being simulated */
	UPROPERTY(BlueprintReadOnly, Category = "Reset")
	int32 Now = 0;

	/** Frames since the last button or stick event */
	UPROPERTY(BlueprintReadOnly, Category = "Reset")
	int32 FramesSinceLastInput = 0;

	/** Bit i is set while button i of the input config is down */
	UPROPERTY(BlueprintReadOnly, Category = "Reset")
	int32 HeldButtonsMask = 0;
};

/** Base of Blueprint buffer reset rules. Override ShouldFlush. Read the comment of GSTN_BlueprintRules.h first. */
UCLASS(Abstract, Blueprintable, BlueprintType)
class GSTNINPUTCORE_API UGSTN_BufferResetRuleBlueprint : public UObject
{
	GENERATED_BODY()

public:
	/** Should the buffered presses of the rule's buttons be thrown away this frame? Deterministic and a function of View only */
	UFUNCTION(BlueprintNativeEvent, Category = "Reset")
	bool ShouldFlush(const FGSTN_BlueprintResetView& View) const;
	virtual bool ShouldFlush_Implementation(const FGSTN_BlueprintResetView& View) const { return false; }
};

/** A buffer reset rule written in Blueprint. Evaluated every frame: the slower path, see the comment at the top of this file. */
USTRUCT(BlueprintType, meta = (DisplayName = "Blueprint (slower)"))
struct GSTNINPUTCORE_API FGSTN_BufferResetRule_Blueprint : public FGSTN_BufferResetRule
{
	GENERATED_BODY()

	/** The Blueprint class whose ShouldFlush decides */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blueprint")
	TSubclassOf<UGSTN_BufferResetRuleBlueprint> Rule;

	/**
	 * Gameplay tags the Blueprint reads through TagsAdded, so the owner watches them. State tags of any root, hence no
	 * Categories filter
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blueprint")
	FGameplayTagContainer WatchedTags;

	virtual bool ShouldFlush(const FGSTN_BufferFrameEvents& Events, const FGSTN_InputHistory& History, int32 Now) const override;
	virtual void GetWatchedTags(FGameplayTagContainer& OutTags) const override { OutTags.AppendTags(WatchedTags); }
};
