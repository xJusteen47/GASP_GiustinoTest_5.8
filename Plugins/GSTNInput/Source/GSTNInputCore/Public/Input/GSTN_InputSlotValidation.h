// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Input/GSTN_InputSlot.h"
#include "GSTN_InputSlotValidation.generated.h"

class UGSTN_InputSettings;

/** What is wrong between slots of one set. */
UENUM(BlueprintType)
enum class EGSTN_SlotConflictKind : uint8
{
	/** Two slots can be triggered by the same input at the same rank: which one fires depends on the slot order. An error */
	Ambiguous,

	/** A slot of higher rank always matches this slot's own input: this slot can never fire from it. A warning */
	Shadowed,

	/** Two slots carry the same command tag. An error */
	DuplicateCommand,

	/** A slot has no command tag, or a malformed pattern (no step, a step without anchor), or its canonical input could not be built. An error */
	InvalidSlot,
};

/** One finding of the validator. */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_SlotConflict
{
	GENERATED_BODY()

	/** The slot the finding is about (the shadowed one, for Shadowed) */
	UPROPERTY(BlueprintReadOnly, Category = "Conflict")
	int32 SlotA = INDEX_NONE;

	/** The other slot (the shadowing one), INDEX_NONE for findings about a single slot */
	UPROPERTY(BlueprintReadOnly, Category = "Conflict")
	int32 SlotB = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Conflict")
	EGSTN_SlotConflictKind Kind = EGSTN_SlotConflictKind::Ambiguous;

	/** For the designer, naming both commands */
	UPROPERTY(BlueprintReadOnly, Category = "Conflict")
	FString Message;

	bool IsError() const { return Kind != EGSTN_SlotConflictKind::Shadowed; }
};

/**
 * Checks the slots of a set against each other (combat.md section 9). Stateless.
 *
 * Every slot's canonical input (the minimal input that satisfies its pattern, built by the conditions themselves) is
 * played into a history and matched against every other slot. If another slot matches it too with the same rank the pair
 * is ambiguous; if it matches with a higher rank, the first slot is shadowed. Slots have no context (bindings do: GSTN_CommandBindings.h). Buttons get arbitrary bits for the check (the set's button tags
 * sorted by name), so no input config is needed: sets are reusable across configs.
 *
 * It looks at canonical inputs only: slots that conflict on an input no canonical input reaches are not found.
 */
UCLASS()
class GSTNINPUTCORE_API UGSTN_InputSlotValidation : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** All findings for the slots, in slot order */
	static TArray<FGSTN_SlotConflict> FindConflicts(TConstArrayView<const FGSTN_InputSlotSpec*> Slots, const UGSTN_InputSettings& Settings);

	/** Convenience for a set asset */
	static TArray<FGSTN_SlotConflict> FindConflicts(const UGSTN_InputSlotSet& Set, const UGSTN_InputSettings& Settings);
};
