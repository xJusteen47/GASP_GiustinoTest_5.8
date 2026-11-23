// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Input/GSTN_InputSlot.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_CommandSet.generated.h"

/**
 * One missing-move fallback: when the kit binds nothing to Command in some context, the first command of Fallbacks that has a binding in that
 * context starts its move instead (owner 2026-11-17: a kit with fewer moves still answers every input of a button). An empty list switches the
 * fallback off for Command.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_CommandFallback
{
	GENERATED_BODY()

	/** The command that may be missing */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fallback", meta = (Categories = "Command"))
	FGameplayTag Command;

	/** Nearest first. The default lists keep the button and try the same stance (standing / crouched) before the same direction */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fallback", meta = (Categories = "Command"))
	TArray<FGameplayTag> Fallbacks;
};

/**
 * The commands a kit understands: a base slot set, minus the commands the kit removes, plus the slots the kit adds. A kit that
 * has no dodge removes the dodge command instead of editing the shared set, and a kit with a special move adds its slot, so
 * kits share one default set and stay small (combat-m3 8). Pure data: the composition below reads slot specs and does not
 * touch abilities, so a unit test can build a set in memory.
 *
 * Removal applies to the base set only and matches the command tag exactly. An added slot whose command is already in the
 * set replaces that slot (the kit's version wins), and adding a command the kit also removes is a contradiction the kit
 * validation reports.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_CommandSet
{
	GENERATED_BODY()

	/** The set to start from. Empty = the combat settings' DefaultCommandSet. Soft: loaded only when the set is composed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Command Set")
	TSoftObjectPtr<UGSTN_InputSlotSet> BaseSet;

	/** Slots this kit adds, or replaces by command (a move only this kit has, a different input for a shared command) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Command Set")
	TArray<FGSTN_InputSlotEntry> AddedSlots;

	/** Commands of the base set this kit does not have. Bindings to them are an error */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Command Set", meta = (Categories = "Command"))
	FGameplayTagContainer RemovedCommands;

	/**
	 * Missing-move fallback (owner 2026-11-17). On: a command of the standard families (attack, strong, special) the kit leaves unbound in a context
	 * borrows the move of the nearest command on the same button, stance first then direction (UGSTN_StandardSlots::BuildDefaultFallbacks). The
	 * compiler adds the borrowed rows; a real binding always wins over a borrowed one
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Command Set|Fallback")
	bool bFillMissingMoves = true;

	/** Per-command replacements of the default lists (and lists for the kit's own commands). An entry with no fallbacks turns one command off */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Command Set|Fallback", meta = (EditCondition = "bFillMissingMoves"))
	TArray<FGSTN_CommandFallback> FallbackOverrides;

	/** The fallback lists that apply: the defaults, each replaced by the override of its command, then the overrides of other commands. Empty when off */
	void GetFallbacks(TArray<FGSTN_CommandFallback>& OutFallbacks) const;

	/**
	 * The base set in memory: BaseSet, or the settings' default when BaseSet is empty. Null when neither is set or the asset
	 * cannot be loaded. Loads synchronously, so call it from loadout and validation code, never from the simulation.
	 */
	const UGSTN_InputSlotSet* ResolveBaseSet() const;

	/** Base minus removed plus added, in that order (the base order is kept; a replaced slot keeps its place). Base may be null. */
	void Compose(const UGSTN_InputSlotSet* Base, TArray<const FGSTN_InputSlotSpec*>& OutSpecs) const;

	/** The commands of the composed set, in the same order as Compose */
	void GetCommands(const UGSTN_InputSlotSet* Base, TArray<FGameplayTag>& OutCommands) const;
};

/** Blueprint-facing readers of a command set. */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_CommandSetUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** The commands of the kit's composed set. Loads the base set, so it is for UI and tools, not for gameplay ticks. */
	UFUNCTION(BlueprintCallable, Category = "Combat|Kits")
	static TArray<FGameplayTag> GetCommandSetCommands(const FGSTN_CommandSet& CommandSet);
};
