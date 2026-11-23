// Command to ability bindings of a character (GAS-NPP fork).

#include "Bindings/GSTN_CommandBindingSet.h"

#include "Abilities/GameplayAbility.h"
#include "Algo/StableSort.h"
#include "GSTN_InputLog.h"
#include "Input/GSTN_CommandLocks.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_InputSlotValidation.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "GSTNCommandBindingSet"

namespace
{
	/** Name order, not tag index order: the index depends on the load order of the tag tables, which differs per machine */
	bool TagNameLess(const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.GetTagName().Compare(B.GetTagName()) < 0;
	}

	void CollectWatchedTags(TConstArrayView<TInstancedStruct<FGSTN_BufferResetRule>> First,
		TConstArrayView<TInstancedStruct<FGSTN_BufferResetRule>> Second, TArray<FGameplayTag>& OutTags)
	{
		FGameplayTagContainer Collected;
		FGSTN_BufferResetRules::GetWatchedTags(First, Collected);
		FGSTN_BufferResetRules::GetWatchedTags(Second, Collected);

		OutTags.Reset();
		for (const FGameplayTag& Tag : Collected)
		{
			OutTags.Add(Tag);
		}
		OutTags.Sort(TagNameLess);
	}
}

bool UGSTN_CommandBindingSet::IsBindableAbility(const UGameplayAbility* Ability, FString* OutReason)
{
	if (!Ability)
	{
		return true;
	}
	const EGameplayAbilityNetSecurityPolicy::Type Policy = Ability->GetNetSecurityPolicy();
	if (Policy == EGameplayAbilityNetSecurityPolicy::ServerOnly || Policy == EGameplayAbilityNetSecurityPolicy::ServerOnlyExecution)
	{
		if (OutReason)
		{
			*OutReason = FString::Printf(TEXT("ability %s has a server only net security policy: its CanActivateAbility depends on the net role, so a client and the server would consume the press differently"),
				*Ability->GetClass()->GetName());
		}
		return false;
	}
	return true;
}

bool FGSTN_CommandTable::HasSourceMasks() const
{
	for (const FGSTN_CompiledBinding& Binding : Bindings)
	{
		if (Binding.SourceMask != 0)
		{
			return true;
		}
	}
	return false;
}

int32 FGSTN_CommandTable::FindCommand(const FGameplayTag& Command) const
{
	return Commands.IndexOfByPredicate([&Command](const FCommand& Entry) { return Entry.Tag == Command; });
}

bool FGSTN_CommandTable::IsCommandLocked(int32 CommandIndex, const FGameplayTagContainer& OwnerTags) const
{
	return Commands.IsValidIndex(CommandIndex)
		&& (UGSTN_CommandLocks::HasGlobalLock(OwnerTags) || UGSTN_CommandLocks::IsLockedBy(OwnerTags, Commands[CommandIndex].LockTags));
}

int32 FGSTN_CommandTable::FromForcedCommand(uint16 ForcedCommand) const
{
	const int32 Index = static_cast<int32>(ForcedCommand) - 1;
	return Commands.IsValidIndex(Index) ? Index : INDEX_NONE;
}

uint16 FGSTN_CommandTable::ToForcedCommand(const FGameplayTag& Command) const
{
	const int32 Index = FindCommand(Command);
	return Index == INDEX_NONE ? 0 : static_cast<uint16>(Index + 1);
}

void UGSTN_CommandBindingSet::GetSpecs(TArray<const FGSTN_InputSlotSpec*>& OutSpecs) const
{
	OutSpecs.Reset();
	LoadedSlotSets.Reset();
	for (const TSoftObjectPtr<UGSTN_InputSlotSet>& SoftSet : SlotSets)
	{
		UGSTN_InputSlotSet* Set = SoftSet.IsNull() ? nullptr : SoftSet.LoadSynchronous();
		if (!SoftSet.IsNull() && !Set)
		{
			UE_LOG(LogGSTNInput, Warning, TEXT("%s: slot set %s could not be loaded and is ignored"), *GetPathName(), *SoftSet.ToString());
		}
		if (Set)
		{
			LoadedSlotSets.Add(Set);
			for (const FGSTN_InputSlotEntry& Entry : Set->Slots)
			{
				OutSpecs.Add(&Entry.GetSpec());
			}
		}
	}
	for (const FGSTN_InputSlotEntry& Entry : Slots)
	{
		OutSpecs.Add(&Entry.GetSpec());
	}
}

FGSTN_CommandTable UGSTN_CommandBindingSet::BuildTable(TConstArrayView<TInstancedStruct<FGSTN_BufferResetRule>> ExtraResetRules) const
{
	FGSTN_CommandTable Table;
	Table.ResetRules = &ResetRules;

	TArray<const FGSTN_InputSlotSpec*> Specs;
	GetSpecs(Specs);

	// A stable sort keeps the declaration order of equal tags, so which duplicate is dropped does not depend on the sort
	Algo::StableSort(Specs, [](const FGSTN_InputSlotSpec* A, const FGSTN_InputSlotSpec* B) { return TagNameLess(A->Command, B->Command); });
	for (const FGSTN_InputSlotSpec* Spec : Specs)
	{
		if (!Spec->Command.IsValid())
		{
			UE_LOG(LogGSTNInput, Warning, TEXT("%s: a slot has no command tag and is ignored"), *GetPathName());
			continue;
		}
		if (Table.Commands.Num() > 0 && Table.Commands.Last().Tag == Spec->Command)
		{
			UE_LOG(LogGSTNInput, Warning, TEXT("%s: command %s has more than one slot, the first is used"), *GetPathName(), *Spec->Command.ToString());
			continue;
		}
		FGSTN_CommandTable::FCommand& Command = Table.Commands.AddDefaulted_GetRef();
		Command.Tag = Spec->Command;
		Command.Slot = Spec;
		Command.LockTags = UGSTN_CommandLocks::GetLockTagsForCommand(Spec->Command);
	}

	// Bindings grouped by command, in declaration order inside a command. The group is found through the sorted commands
	TArray<int32> Order;
	for (int32 Index = 0; Index < Bindings.Num(); ++Index)
	{
		const FGSTN_CommandBinding& Binding = Bindings[Index];
		if (Table.FindCommand(Binding.Command) == INDEX_NONE)
		{
			UE_LOG(LogGSTNInput, Warning, TEXT("%s: binding %d names command %s, which has no slot, and is ignored"), *GetPathName(), Index, *Binding.Command.ToString());
			continue;
		}
		if (Binding.Ability.IsNull())
		{
			UE_LOG(LogGSTNInput, Warning, TEXT("%s: binding %d (%s) has no ability and is ignored"), *GetPathName(), Index, *Binding.Command.ToString());
			continue;
		}
		if (!Binding.Ability.LoadSynchronous())
		{
			UE_LOG(LogGSTNInput, Warning, TEXT("%s: binding %d (%s): ability %s could not be loaded and is ignored"), *GetPathName(), Index, *Binding.Command.ToString(), *Binding.Ability.ToString());
			continue;
		}
		FString Reason;
		if (!IsBindableAbility(Binding.Ability->GetDefaultObject<UGameplayAbility>(), &Reason))
		{
			UE_LOG(LogGSTNInput, Error, TEXT("%s: binding %d (%s) is ignored: %s"), *GetPathName(), Index, *Binding.Command.ToString(), *Reason);
			continue;
		}
		Order.Add(Index);
	}
	Order.StableSort([this](int32 A, int32 B) { return TagNameLess(Bindings[A].Command, Bindings[B].Command); });
	if (Order.Num() > FGSTN_CommandTable::MaxBindings)
	{
		UE_LOG(LogGSTNInput, Error, TEXT("%s: %d bindings, only the first %d are used"), *GetPathName(), Order.Num(), FGSTN_CommandTable::MaxBindings);
		Order.SetNum(FGSTN_CommandTable::MaxBindings);
	}

	for (const int32 Index : Order)
	{
		const FGSTN_CommandBinding& Binding = Bindings[Index];
		const int32 CommandIndex = Table.FindCommand(Binding.Command);
		FGSTN_CommandTable::FCommand& Command = Table.Commands[CommandIndex];
		if (Command.NumBindings == 0)
		{
			Command.FirstBinding = Table.Bindings.Num();
		}
		++Command.NumBindings;

		FGSTN_CompiledBinding& Compiled = Table.Bindings.AddDefaulted_GetRef();
		Compiled.CommandIndex = CommandIndex;
		Compiled.Requirements = Binding.Requirements;
		Compiled.Ability = Binding.Ability.Get();
	}

	for (int32 CommandIndex = 0; CommandIndex < Table.Commands.Num(); ++CommandIndex)
	{
		if (Table.Commands[CommandIndex].NumBindings > 0)
		{
			Table.CandidateSlots.Add(Table.Commands[CommandIndex].Slot);
			Table.CandidateCommands.Add(CommandIndex);
		}
	}

	CollectWatchedTags(ExtraResetRules, ResetRules, Table.WatchedTags);
	if (Table.WatchedTags.Num() > FGSTN_CommandTable::MaxWatchedTags)
	{
		UE_LOG(LogGSTNInput, Error, TEXT("%s: %d watched tags, only the first %d are watched"), *GetPathName(), Table.WatchedTags.Num(), FGSTN_CommandTable::MaxWatchedTags);
		Table.WatchedTags.SetNum(FGSTN_CommandTable::MaxWatchedTags);
	}
	return Table;
}

#if WITH_EDITOR
EDataValidationResult UGSTN_CommandBindingSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<const FGSTN_InputSlotSpec*> Specs;
	GetSpecs(Specs);
	TSet<FGameplayTag> SlotCommands;
	for (const FGSTN_InputSlotSpec* Spec : Specs)
	{
		SlotCommands.Add(Spec->Command);
	}

	for (const FGSTN_SlotConflict& Conflict : UGSTN_InputSlotValidation::FindConflicts(Specs, UGSTN_InputSettings::Get()))
	{
		if (Conflict.IsError())
		{
			Context.AddError(FText::FromString(Conflict.Message));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			Context.AddWarning(FText::FromString(Conflict.Message));
		}
	}

	TArray<FGSTN_BindingKey> Keys;
	TArray<int32> KeyToBinding;
	for (int32 Index = 0; Index < Bindings.Num(); ++Index)
	{
		const FGSTN_CommandBinding& Binding = Bindings[Index];
		if (!Binding.Command.IsValid())
		{
			Context.AddError(FText::Format(LOCTEXT("BindingNoCommand", "Binding {0} has no command."), Index));
			Result = EDataValidationResult::Invalid;
			continue;
		}
		if (!SlotCommands.Contains(Binding.Command))
		{
			Context.AddError(FText::Format(LOCTEXT("BindingNoSlot", "Binding {0} names {1}, which has no slot in this set."),
				Index, FText::FromName(Binding.Command.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}
		if (Binding.Ability.IsNull())
		{
			Context.AddError(FText::Format(LOCTEXT("BindingNoAbility", "Binding {0} ({1}) has no ability."),
				Index, FText::FromName(Binding.Command.GetTagName())));
			Result = EDataValidationResult::Invalid;
		}
		else if (const UClass* AbilityClass = Binding.Ability.LoadSynchronous())
		{
			FString Reason;
			if (!IsBindableAbility(AbilityClass->GetDefaultObject<UGameplayAbility>(), &Reason))
			{
				Context.AddError(FText::Format(LOCTEXT("BindingRoleDependent", "Binding {0} ({1}): {2}."),
					Index, FText::FromName(Binding.Command.GetTagName()), FText::FromString(Reason)));
				Result = EDataValidationResult::Invalid;
			}
		}
		Keys.Add({ Binding.Command, &Binding.Requirements });
		KeyToBinding.Add(Index);
	}

	const TArray<FGSTN_ExclusiveTagGroup> ExclusiveGroups = UGSTN_InputSettings::Get().GetAllExclusiveContextGroups();
	for (const FGSTN_BindingConflict& Impossible : UGSTN_CommandBindingUtils::FindImpossibleBindings(Keys, ExclusiveGroups))
	{
		Context.AddError(FText::Format(LOCTEXT("BindingImpossible", "Binding {0}: {1}"),
			KeyToBinding.IsValidIndex(Impossible.BindingA) ? KeyToBinding[Impossible.BindingA] : Impossible.BindingA,
			FText::FromString(Impossible.Message)));
		Result = EDataValidationResult::Invalid;
	}
	for (const FGSTN_BindingConflict& Conflict : UGSTN_CommandBindingUtils::FindBindingConflicts(Keys, ExclusiveGroups))
	{
		Context.AddError(FText::Format(LOCTEXT("BindingConflict", "Bindings {0} and {1}: {2}"),
			KeyToBinding.IsValidIndex(Conflict.BindingA) ? KeyToBinding[Conflict.BindingA] : Conflict.BindingA,
			KeyToBinding.IsValidIndex(Conflict.BindingB) ? KeyToBinding[Conflict.BindingB] : Conflict.BindingB,
			FText::FromString(Conflict.Message)));
		Result = EDataValidationResult::Invalid;
	}

	if (Bindings.Num() > FGSTN_CommandTable::MaxBindings)
	{
		Context.AddError(FText::Format(LOCTEXT("TooManyBindings", "{0} bindings, a request holds an index up to {1}."), Bindings.Num(), FGSTN_CommandTable::MaxBindings));
		Result = EDataValidationResult::Invalid;
	}

	TArray<FGameplayTag> Watched;
	CollectWatchedTags({}, ResetRules, Watched);
	if (Watched.Num() > FGSTN_CommandTable::MaxWatchedTags)
	{
		Context.AddError(FText::Format(LOCTEXT("TooManyWatchedTags", "The reset rules watch {0} tags, the limit is {1}."), Watched.Num(), FGSTN_CommandTable::MaxWatchedTags));
		Result = EDataValidationResult::Invalid;
	}

	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif

#undef LOCTEXT_NAMESPACE
