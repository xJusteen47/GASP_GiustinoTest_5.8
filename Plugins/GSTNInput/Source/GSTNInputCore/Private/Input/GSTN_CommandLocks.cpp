// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_CommandLocks.h"

#include "GameplayTagsManager.h"
#include "Input/GSTN_InputTags.h"

namespace
{
	/** The lock tag of one command tag, whatever its depth; the root maps to the global lock. Invalid if there is none */
	FGameplayTag LockTagOf(const FGameplayTag& CommandTag)
	{
		const FString CommandRoot = GSTNInputTags::Command.GetTag().ToString();
		const FString Name = CommandTag.ToString();
		if (Name == CommandRoot)
		{
			return GSTNInputTags::Lock_Command.GetTag();
		}
		if (!Name.StartsWith(CommandRoot + TEXT(".")))
		{
			return FGameplayTag();
		}

		// "Command.Attack.Light" -> "Input.Lock.Command" + ".Attack.Light"
		const FString LockName = GSTNInputTags::Lock_Command.GetTag().ToString() + Name.RightChop(CommandRoot.Len());
		return UGameplayTagsManager::Get().RequestGameplayTag(FName(*LockName), false);
	}
}

FGameplayTag UGSTN_CommandLocks::GetLockTagForCommand(FGameplayTag Command)
{
	return Command.IsValid() ? LockTagOf(Command) : FGameplayTag();
}

FGameplayTagContainer UGSTN_CommandLocks::GetLockTagsForCommand(FGameplayTag Command)
{
	FGameplayTagContainer LockTags;
	if (!Command.IsValid())
	{
		return LockTags;
	}

	// GetGameplayTagParents holds the tag itself and every ancestor
	const FGameplayTag GlobalLock = GSTNInputTags::Lock_Command.GetTag();
	for (const FGameplayTag& Ancestor : Command.GetGameplayTagParents())
	{
		const FGameplayTag LockTag = LockTagOf(Ancestor);
		if (LockTag.IsValid() && LockTag != GlobalLock)
		{
			LockTags.AddTag(LockTag);
		}
	}
	return LockTags;
}

bool UGSTN_CommandLocks::HasGlobalLock(const FGameplayTagContainer& OwnerTags)
{
	return OwnerTags.HasTagExact(GSTNInputTags::Lock_Command.GetTag());
}

bool UGSTN_CommandLocks::IsLockedBy(const FGameplayTagContainer& OwnerTags, const FGameplayTagContainer& CommandLockTags)
{
	return !CommandLockTags.IsEmpty() && OwnerTags.HasAnyExact(CommandLockTags);
}

bool UGSTN_CommandLocks::IsCommandLocked(const FGameplayTagContainer& OwnerTags, FGameplayTag Command)
{
	return HasGlobalLock(OwnerTags) || IsLockedBy(OwnerTags, GetLockTagsForCommand(Command));
}
