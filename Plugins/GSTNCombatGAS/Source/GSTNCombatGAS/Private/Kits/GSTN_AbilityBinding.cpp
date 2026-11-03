// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Kits/GSTN_AbilityBinding.h"
#include "Abilities/GameplayAbility.h"

TArray<FGSTN_AbilityBinding> UGSTN_AbilityBindingUtils::InheritBindings(const TArray<FGSTN_AbilityBinding>& Parent, const TArray<FGSTN_AbilityBinding>& Child)
{
	TArray<FGSTN_AbilityBinding> Result;
	Result.Reserve(Parent.Num() + Child.Num());
	TBitArray<> ChildUsed(false, Child.Num());

	for (const FGSTN_AbilityBinding& Inherited : Parent)
	{
		const int32 Replacement = Child.IndexOfByPredicate([&Inherited](const FGSTN_AbilityBinding& Own) { return Own.HasSameKey(Inherited); });
		if (Replacement != INDEX_NONE)
		{
			Result.Add(Child[Replacement]);
			ChildUsed[Replacement] = true;
		}
		else
		{
			Result.Add(Inherited);
		}
	}

	// A child entry that matched no parent entry is new; one that shares its key with an earlier child entry is a duplicate
	// the validation reports, and it is kept so the list the designer sees is the list that runs
	for (int32 Index = 0; Index < Child.Num(); ++Index)
	{
		if (!ChildUsed[Index])
		{
			Result.Add(Child[Index]);
		}
	}
	return Result;
}

void UGSTN_AbilityBindingUtils::Validate(TConstArrayView<FGSTN_AbilityBinding> Bindings, const FGSTN_BindingValidationContext& Context, const FString& What, TArray<FText>& OutErrors, TArray<FText>& OutWarnings)
{
	TArray<FGameplayTag> SetCommands;
	if (Context.CommandSet)
	{
		Context.CommandSet->GetCommands(Context.BaseSet, SetCommands);
	}

	for (int32 Index = 0; Index < Bindings.Num(); ++Index)
	{
		const FGSTN_AbilityBinding& Binding = Bindings[Index];
		const FString Where = FString::Printf(TEXT("%s binding %d"), *What, Index);

		if (!Binding.Command.IsValid())
		{
			OutErrors.Add(FText::FromString(FString::Printf(TEXT("%s has no command."), *Where)));
			continue;
		}
		const FString Command = Binding.Command.ToString();

		for (int32 Earlier = 0; Earlier < Index; ++Earlier)
		{
			if (Bindings[Earlier].HasSameKey(Binding))
			{
				OutErrors.Add(FText::FromString(FString::Printf(TEXT("%s repeats the command %s with the same requirements as binding %d: the choice would be arbitrary."), *Where, *Command, Earlier)));
				break;
			}
		}

		if (Binding.HasSlotOverride() && Binding.GetSlotOverrideCommand() != Binding.Command)
		{
			OutErrors.Add(FText::FromString(FString::Printf(TEXT("%s overrides the slot with another command (%s) than the one it binds (%s)."), *Where, *Binding.GetSlotOverrideCommand().ToString(), *Command)));
		}

		if (Binding.BufferFramesOverride < -1)
		{
			OutErrors.Add(FText::FromString(FString::Printf(TEXT("%s has a buffer of %d frames (-1 means the slot's)."), *Where, Binding.BufferFramesOverride)));
		}

		if (Binding.Ability.IsNull())
		{
			OutWarnings.Add(FText::FromString(FString::Printf(TEXT("%s (%s) binds no ability, so it only hides what a parent binds."), *Where, *Command)));
		}

		if (Context.CommandSet)
		{
			if (Context.CommandSet->RemovedCommands.HasTagExact(Binding.Command))
			{
				OutErrors.Add(FText::FromString(FString::Printf(TEXT("%s binds %s, which the command set removes."), *Where, *Command)));
			}
			else if (Context.bCheckMembership && !SetCommands.Contains(Binding.Command) && !Binding.HasSlotOverride())
			{
				OutErrors.Add(FText::FromString(FString::Printf(TEXT("%s binds %s, which is not in the command set: add the slot to the command set or give the binding a slot override."), *Where, *Command)));
			}
		}
	}
}
