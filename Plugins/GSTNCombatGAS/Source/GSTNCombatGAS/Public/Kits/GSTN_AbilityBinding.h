// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_CommandBindings.h"
#include "Input/GSTN_InputSlot.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kits/GSTN_CommandSet.h"
#include "GSTN_AbilityBinding.generated.h"

class UGameplayAbility;
class UGSTN_CombatTimeline;

/**
 * Binds a command to an ability, for the contexts its requirements describe (combat-m3 8). A command is the same input
 * everywhere (ground, air, wall...); what it starts depends on the character's context, so a kit lists the same command
 * several times with different requirements (Grounded jab, Airborne neutral air).
 *
 * The ability and the shared timeline are soft: the binding is data, and the loadout resolves the classes when it grants them,
 * outside the simulation. The slot override is the input plugin's entry type, which holds its slot asset by hard pointer; that
 * is the one hard reference in this struct and it is the input plugin's choice, used as is.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATGAS_API FGSTN_AbilityBinding
{
	GENERATED_BODY()

	/** The command this binding answers (the key of the binding together with Requirements) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding", meta = (Categories = "Command"))
	FGameplayTag Command;

	/** Context the binding needs (e.g. grounded, or airborne and falling). Empty = any context */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	FGSTN_TagRequirements Requirements;

	/**
	 * A slot for a command the kit's command set does not have (a move only this kit enters). Empty = use the command set's
	 * slot for Command. When set, its command must be Command.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	FGSTN_InputSlotEntry SlotOverride;

	/** The ability to activate. A binding without one does nothing; a child binding without one hides its parent's */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	TSoftClassPtr<UGameplayAbility> Ability;

	/** Replaces the timeline of the ability (only meaningful for timeline abilities), so one class serves several weapons */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	TSoftObjectPtr<UGSTN_CombatTimeline> TimelineOverride;

	/** Added to the slot's priority offset for this binding only */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding|Priority")
	int32 PriorityOffset = 0;

	/** Buffer of this binding. -1 = the slot's */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding|Buffer", meta = (ClampMin = "-1", ForceUnits = "Frames"))
	int32 BufferFramesOverride = -1;

	/** Whether SlotOverride names a slot */
	bool HasSlotOverride() const { return SlotOverride.Asset != nullptr || SlotOverride.Inline.Command.IsValid(); }

	/** The command of the override slot, invalid when there is none */
	FGameplayTag GetSlotOverrideCommand() const { return SlotOverride.GetSpec().Command; }

	/** Same command and the same requirement sets: the pair that identifies a binding for inheritance and for duplicates */
	bool HasSameKey(const FGSTN_AbilityBinding& Other) const
	{
		return Command == Other.Command && Requirements.RequireTags == Other.Requirements.RequireTags && Requirements.IgnoreTags == Other.Requirements.IgnoreTags;
	}
};

/** What a binding list is checked against: the command set it must fit, when it belongs to a kit. */
struct FGSTN_BindingValidationContext
{
	/** The kit's command set, or null for a list that has none (a weapon: its commands are the kit's, checked when the loadout is compiled) */
	const FGSTN_CommandSet* CommandSet = nullptr;

	/** The resolved base set of CommandSet (may be null: then only AddedSlots form the set) */
	const UGSTN_InputSlotSet* BaseSet = nullptr;

	/**
	 * Whether a binding to a command outside the composed set is an error. Off when the base set could not be resolved, since
	 * then the set is unknown and every command would be reported (removed commands are still checked).
	 */
	bool bCheckMembership = true;
};

/** Binding inheritance and validation, stateless. */
UCLASS()
class GSTNCOMBATGAS_API UGSTN_AbilityBindingUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * The bindings of a child over its parent: the parent's entries in order, each replaced in place by the child entry with the
	 * same command and requirements; then the child entries that replace nothing, in the child's order. The whole parent chain is
	 * folded by calling this with the result so far as Parent.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat|Bindings")
	static TArray<FGSTN_AbilityBinding> InheritBindings(const TArray<FGSTN_AbilityBinding>& Parent, const TArray<FGSTN_AbilityBinding>& Child);

	/**
	 * Checks a binding list. Errors: an invalid command, two bindings with the same command and requirements, an override
	 * slot of another command, a negative buffer other than -1; and, with a command set: a binding to a removed command, or to
	 * a command that is neither in the set nor given a slot override. Warnings: a binding without an ability.
	 * What names the owner in the messages.
	 */
	static void Validate(TConstArrayView<FGSTN_AbilityBinding> Bindings, const FGSTN_BindingValidationContext& Context, const FString& What, TArray<FText>& OutErrors, TArray<FText>& OutWarnings);
};
