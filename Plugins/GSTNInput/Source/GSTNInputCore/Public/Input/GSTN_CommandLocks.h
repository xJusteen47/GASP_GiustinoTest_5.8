// Pure input logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GSTN_CommandLocks.generated.h"

/**
 * Command locks (combat.md section 11.4, M2 section 6b): tags on the owner that stop commands from starting, replacing the
 * old per-component "blocking tags" list.
 *
 * `Input.Lock.Command` (exact) locks every command; `Input.Lock.Command.<path>` locks the commands under `Command.<path>`
 * (the command itself and everything below it). So a stance that owns `Input.Lock.Command.Attack` stops `Command.Attack.*`
 * while the dash command, under another root, still starts. Who declares the per-root lock tags is whoever declares the
 * command root (natively, or in Config/Tags/GSTNCommands.ini).
 *
 * The lock tags are plain gameplay tags that abilities and effects grant (ActivationOwnedTags, granted tags), so the usual
 * tag counts apply and nothing in the simulation writes them. The input simulation reads them from the owner's tags at the
 * end of the previous frame, the same container the binding requirements see.
 *
 * Why exact matching: a container that holds `Input.Lock.Command.Attack` also answers "has Input.Lock.Command" through its
 * parent tags, which would turn every per-root lock into a global one. Lock checks therefore look at the explicit tags only.
 *
 * Pure and deterministic: the result depends on the tags passed in, never on world state.
 */
UCLASS()
class GSTNINPUTCORE_API UGSTN_CommandLocks : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * The lock tag that locks Command and what is below it: `Command.Attack.Light` -> `Input.Lock.Command.Attack.Light`.
	 * Invalid if Command is not under the Command root or the lock tag is not registered (a designer root without a lock tag).
	 * The Command root itself maps to the global lock `Input.Lock.Command`.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|Locks")
	static FGameplayTag GetLockTagForCommand(FGameplayTag Command);

	/**
	 * Every registered lock tag that locks Command except the global one: its own and one per ancestor under the Command
	 * root (`Command.Attack.Light` -> `.Attack.Light` and `.Attack`). Computed once per command when a command table is
	 * built, so the per-frame check is a tag intersection.
	 */
	UFUNCTION(BlueprintPure, Category = "Input|Locks")
	static FGameplayTagContainer GetLockTagsForCommand(FGameplayTag Command);

	/** The owner has `Input.Lock.Command` itself: no command can start */
	UFUNCTION(BlueprintPure, Category = "Input|Locks")
	static bool HasGlobalLock(const FGameplayTagContainer& OwnerTags);

	/** Any of CommandLockTags (see GetLockTagsForCommand) is on the owner */
	UFUNCTION(BlueprintPure, Category = "Input|Locks")
	static bool IsLockedBy(const FGameplayTagContainer& OwnerTags, const FGameplayTagContainer& CommandLockTags);

	/** Whether the owner's tags lock Command, globally or through its root or itself */
	UFUNCTION(BlueprintPure, Category = "Input|Locks")
	static bool IsCommandLocked(const FGameplayTagContainer& OwnerTags, FGameplayTag Command);
};
