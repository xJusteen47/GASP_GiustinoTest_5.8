// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_CommandBindings.generated.h"

/**
 * Tags a character must have and must not have for a binding to apply. The engine's FGameplayTagRequirements lives in the
 * ability system module, which the Core must not depend on; this is the same two lists.
 *
 * A command is the same input in every context (on the ground, in the air, on a wall...): what it starts depends on the
 * context, so the requirements belong to the binding of a command to a move or ability, not to the command's slot.
 */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_TagRequirements
{
	GENERATED_BODY()

	/** Every one of these must be present */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirements")
	FGameplayTagContainer RequireTags;

	/** None of these may be present */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirements")
	FGameplayTagContainer IgnoreTags;

	bool IsEmpty() const { return RequireTags.IsEmpty() && IgnoreTags.IsEmpty(); }

	/** All required tags are in Context and none of the ignored ones is */
	bool IsSatisfiedBy(const FGameplayTagContainer& Context) const
	{
		return Context.HasAll(RequireTags) && !Context.HasAny(IgnoreTags);
	}

	/** How demanding the requirements are: the binding with the most required tags is the most specific */
	int32 GetRequiredCount() const { return RequireTags.Num(); }

	/**
	 * False when no context can satisfy both: one requires a tag the other ignores. Two bindings of a command that can hold
	 * together must differ in how many tags they require, or the choice between them is arbitrary.
	 */
	bool CanHoldTogether(const FGSTN_TagRequirements& Other) const
	{
		return !RequireTags.HasAny(Other.IgnoreTags) && !Other.RequireTags.HasAny(IgnoreTags);
	}
};

/** A binding as the validator sees it: which command, under which requirements. */
struct FGSTN_BindingKey
{
	FGameplayTag Command;
	const FGSTN_TagRequirements* Requirements = nullptr;
};

/** Two bindings of one command that cannot be told apart by their context. */
struct FGSTN_BindingConflict
{
	/** Indices in the list given to FindBindingConflicts, BindingA < BindingB */
	int32 BindingA = INDEX_NONE;
	int32 BindingB = INDEX_NONE;

	FString Message;
};

/**
 * Pure helpers over command bindings (combat.md section 9.2), shared by every binding set: the combat component's own,
 * the kit and weapon move sets of M3. Stateless, deterministic, no allocation in the selection.
 */
UCLASS()
class GSTNINPUTCORE_API UGSTN_CommandBindingUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Picks the binding of a command for the current context among its candidates: of those whose requirements hold, the
	 * one with the most required tags wins (an on-wall binding requiring Airborne + OnWall beats an airborne one), the
	 * first of them on a tie. Null candidates are skipped.
	 * @return index in Candidates, INDEX_NONE if none holds: the command cannot execute in this context.
	 */
	static int32 SelectBinding(TConstArrayView<const FGSTN_TagRequirements*> Candidates, const FGameplayTagContainer& Context);

	/**
	 * Errors of a binding list: two bindings of the same command whose requirements can hold together and that require
	 * the same number of tags. Bindings without a command are ignored (the owning asset reports them).
	 */
	static TArray<FGSTN_BindingConflict> FindBindingConflicts(TConstArrayView<FGSTN_BindingKey> Bindings);
};
