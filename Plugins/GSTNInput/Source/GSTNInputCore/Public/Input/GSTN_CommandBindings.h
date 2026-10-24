// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_CommandBindings.generated.h"

/**
 * A set of state tags of which a context holds at most one (e.g. Grounded and Airborne). Binding validation uses the groups to know
 * that two bindings requiring different members can never apply together, which "requires what the other ignores" cannot
 * express. Data, not code: the validator never names a project tag, the settings and the plugins that own the tags list them
 * (UGSTN_InputSettings::ExclusiveContextGroups). Members match exactly, not through the tag hierarchy.
 */
USTRUCT(BlueprintType)
struct GSTNINPUTCORE_API FGSTN_ExclusiveTagGroup
{
	GENERATED_BODY()

	/** A context holds at most one of these. State tags of any root, so no Categories filter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirements")
	FGameplayTagContainer Tags;
};

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

	/**
	 * Every one of these must be present. State tags of any root (Input.Context.*, gameplay tags, the movement system's own), so no
	 * Categories filter
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Requirements")
	FGameplayTagContainer RequireTags;

	/** None of these may be present. State tags of any root, so no Categories filter */
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

	/**
	 * As above, and also false when some group holds a tag this requires and a different tag Other requires: no context has
	 * both members of an exclusive group.
	 */
	bool CanHoldTogether(const FGSTN_TagRequirements& Other, TConstArrayView<FGSTN_ExclusiveTagGroup> Groups) const
	{
		if (!CanHoldTogether(Other))
		{
			return false;
		}
		for (const FGSTN_ExclusiveTagGroup& Group : Groups)
		{
			for (const FGameplayTag& Mine : RequireTags)
			{
				if (!Group.Tags.HasTagExact(Mine))
				{
					continue;
				}
				for (const FGameplayTag& Theirs : Other.RequireTags)
				{
					if (Theirs != Mine && Group.Tags.HasTagExact(Theirs))
					{
						return false;
					}
				}
			}
		}
		return true;
	}

	/** Whether RequireTags holds two different tags of one group: no context satisfies it */
	bool RequiresTwoOfAGroup(TConstArrayView<FGSTN_ExclusiveTagGroup> Groups) const
	{
		for (const FGSTN_ExclusiveTagGroup& Group : Groups)
		{
			int32 Found = 0;
			for (const FGameplayTag& Tag : RequireTags)
			{
				Found += Group.Tags.HasTagExact(Tag) ? 1 : 0;
			}
			if (Found > 1)
			{
				return true;
			}
		}
		return false;
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
	/** Blueprint form of FGSTN_TagRequirements::IsSatisfiedBy: all required tags are in Context and none of the ignored ones is */
	UFUNCTION(BlueprintPure, Category = "Input|Bindings")
	static bool AreRequirementsSatisfied(const FGSTN_TagRequirements& Requirements, const FGameplayTagContainer& Context)
	{
		return Requirements.IsSatisfiedBy(Context);
	}

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
	 * @param ExclusiveGroups Tags of which no context holds two (UGSTN_InputSettings::GetAllExclusiveContextGroups): bindings
	 * requiring different members of a group cannot hold together, so they are not reported. Empty = only the ignore lists count.
	 */
	static TArray<FGSTN_BindingConflict> FindBindingConflicts(TConstArrayView<FGSTN_BindingKey> Bindings,
		TConstArrayView<FGSTN_ExclusiveTagGroup> ExclusiveGroups = TConstArrayView<FGSTN_ExclusiveTagGroup>());

	/**
	 * Bindings that can never apply because they require two tags of one exclusive group. One entry per binding, with
	 * BindingA == BindingB == its index in Bindings. Bindings without a command are ignored, like in FindBindingConflicts.
	 */
	static TArray<FGSTN_BindingConflict> FindImpossibleBindings(TConstArrayView<FGSTN_BindingKey> Bindings,
		TConstArrayView<FGSTN_ExclusiveTagGroup> ExclusiveGroups);
};
