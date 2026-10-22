// Command to ability bindings of a character (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_BufferResetRules.h"
#include "Input/GSTN_CommandBindings.h"
#include "Input/GSTN_InputSlot.h"
#include "StructUtils/InstancedStruct.h"
#include "GSTN_CommandBindingSet.generated.h"

class UGameplayAbility;
class UGSTN_CommandBindingSet;

/**
 * What a command starts when its requirements hold. A command can have several bindings (grounded, airborne, on a wall...):
 * the one the owner's context picks wins, see FGSTN_TagRequirements and UGSTN_CommandBindingUtils::SelectBinding.
 */
USTRUCT(BlueprintType)
struct GSTNINPUTNP_API FGSTN_CommandBinding
{
	GENERATED_BODY()

	/** The command (the tag of one of the set's slots) this binding belongs to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding", meta = (Categories = "Command"))
	FGameplayTag Command;

	/** Context the binding applies in: tags the owner must / must not have (the Input.Context.* tags, gameplay tags...) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	FGSTN_TagRequirements Requirements;

	/**
	 * Ability the command activates. Must be the granted class exactly: the ability system looks the spec up by class. A soft
	 * class, so a binding set does not load every ability of a character with it: BuildTable resolves it once, before the
	 * first simulation tick, into FGSTN_CompiledBinding::Ability
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	TSoftClassPtr<UGameplayAbility> Ability;
};

/** A binding as the simulation reads it: plain data, owned by the table, so selecting one allocates nothing. */
struct FGSTN_CompiledBinding
{
	/** Index in FGSTN_CommandTable::Commands */
	int32 CommandIndex = INDEX_NONE;

	FGSTN_TagRequirements Requirements;

	/** Loaded when the table was built: the simulation reads it every frame and never resolves a soft reference */
	TSubclassOf<UGameplayAbility> Ability;
};

/**
 * The runtime form of a binding set: the commands and bindings in an order that is the same on every machine, which is
 * what makes the indices stored in the sync state (RequestedBinding) and in the input command (ForcedCommand) mean the
 * same thing everywhere. Built once (BeginPlay) from the asset and read-only afterwards: it is not simulation state, so
 * it needs no rollback, but it must never be rebuilt while a match runs (a rebuild would reorder the indices).
 *
 * Commands are sorted by tag name; the bindings by (command, declaration order). Commands without a binding stay in
 * Commands (a forced command or a lock can still name them) but are not candidates for the resolver, so they never
 * reserve a press. The slot pointers and the reset rules point into the asset, which the host must keep alive.
 */
struct GSTNINPUTNP_API FGSTN_CommandTable
{
	struct FCommand
	{
		FGameplayTag Tag;
		const FGSTN_InputSlotSpec* Slot = nullptr;

		/** The command's bindings are Bindings[FirstBinding, FirstBinding + NumBindings) */
		int32 FirstBinding = 0;
		int32 NumBindings = 0;

		/**
		 * The lock tags that lock this command (UGSTN_CommandLocks::GetLockTagsForCommand: its own and its roots'), worked out
		 * when the table is built so the per-frame check is a tag intersection
		 */
		FGameplayTagContainer LockTags;
	};

	/** At most 255 bindings: the request stores a binding index in a byte */
	static constexpr int32 MaxBindings = 255;

	/** At most 64 watched tags: they are a bit mask in the sync state */
	static constexpr int32 MaxWatchedTags = 64;

	/** Every slot of the set, sorted by command tag name */
	TArray<FCommand> Commands;

	/** Sorted by (command, declaration order) */
	TArray<FGSTN_CompiledBinding> Bindings;

	/** The slots of the commands that have at least one binding, in command order: what the resolver is given */
	TArray<const FGSTN_InputSlotSpec*> CandidateSlots;

	/** CandidateSlots[i] is the slot of command CandidateCommands[i] */
	TArray<int32> CandidateCommands;

	/** Tags the reset rules wait for (OnTagAdded), sorted by name: bit i of the watched mask is WatchedTags[i] */
	TArray<FGameplayTag> WatchedTags;

	/** The binding set's own reset rules, applied after the host's global ones */
	const TArray<TInstancedStruct<FGSTN_BufferResetRule>>* ResetRules = nullptr;

	bool IsEmpty() const { return Commands.IsEmpty(); }

	/** Index of a command by tag (exact), INDEX_NONE if the table does not have it */
	int32 FindCommand(const FGameplayTag& Command) const;

	/** Whether the owner's tags lock the command: the global lock, or a lock tag of its root or of itself (M2 section 6b) */
	bool IsCommandLocked(int32 CommandIndex, const FGameplayTagContainer& OwnerTags) const;

	/** Index of a command by the 1-based value of FGSTN_InputCmd::ForcedCommand, INDEX_NONE if 0 or out of range */
	int32 FromForcedCommand(uint16 ForcedCommand) const;

	/** The ForcedCommand value that names Command, 0 if the table does not have it */
	uint16 ToForcedCommand(const FGameplayTag& Command) const;
};

/**
 * The commands of a character and what they start (M2 section 7). Slots define how each command is entered (shared slot
 * assets, or specs written in place; empty entries are skipped); bindings tie commands to abilities per context; reset
 * rules throw buffered presses away when something happens. A slot without a binding is legal: the command exists (an AI can
 * request it, a lock can name it) but starts nothing and never reserves a press.
 *
 * The combat plugin replaces this asset by the compiled loadout table of M3 and keeps calling the simulator the same way.
 */
UCLASS(BlueprintType)
class GSTNINPUTNP_API UGSTN_CommandBindingSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Slot sets whose slots are included before Slots: the way to reuse a shared set (for instance one filled from the
	 * combat plugin's standard slots) without copying it. Soft references, loaded by GetSpecs / BuildTable and kept alive by
	 * this asset from then on
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commands")
	TArray<TSoftObjectPtr<UGSTN_InputSlotSet>> SlotSets;

	/** Slots specific to this set */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commands")
	TArray<FGSTN_InputSlotEntry> Slots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commands")
	TArray<FGSTN_CommandBinding> Bindings;

	/** Applied after the host's global rules */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Commands")
	TArray<TInstancedStruct<FGSTN_BufferResetRule>> ResetRules;

	/**
	 * Every slot spec: the included sets' first, in order, then this asset's own. Loads the included sets that are not loaded,
	 * so call it at setup, never from a tick
	 */
	void GetSpecs(TArray<const FGSTN_InputSlotSpec*>& OutSpecs) const;

	/**
	 * Builds the runtime table. Slots with no command tag or a repeated tag, bindings with no slot, no ability or a server only ability (IsBindableAbility) and anything
	 * past the table limits are left out and logged; IsDataValid reports the same problems to the designer.
	 * @param ExtraResetRules The host's global rules, whose watched tags join this set's in the table.
	 */
	FGSTN_CommandTable BuildTable(TConstArrayView<TInstancedStruct<FGSTN_BufferResetRule>> ExtraResetRules = {}) const;

	/**
	 * Whether a command can be bound to this ability. The simulation decides press consumption with the ability's
	 * CanActivateAbility, which checks the net role through ShouldActivateAbility: an ability whose net security policy is
	 * ServerOnly or ServerOnlyExecution answers "no" on a client and "yes" on the server, so the two would consume the press
	 * differently and the input state would diverge. BuildTable drops such bindings and IsDataValid reports them.
	 * (TODO: a role free activation check in the fork would lift this.)
	 * @param OutReason Why the ability is refused, when it is.
	 */
	static bool IsBindableAbility(const UGameplayAbility* Ability, FString* OutReason = nullptr);

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif

private:
	/** The loaded slot sets, strongly referenced because the table points into them and a soft pointer would not keep them alive */
	UPROPERTY(Transient)
	mutable TArray<TObjectPtr<UGSTN_InputSlotSet>> LoadedSlotSets;
};
