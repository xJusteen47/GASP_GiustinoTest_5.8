// Button layout (IO-2).

#include "Layout/GSTN_ButtonLayout.h"

FGameplayTag UGSTN_ButtonLayout::FindButton(const FName Function) const
{
	const FGSTN_ButtonAssignment* Found = Assignments.FindByPredicate([Function](const FGSTN_ButtonAssignment& A) { return A.Function == Function; });
	return Found ? Found->Button : FGameplayTag();
}

FGameplayTag UGSTN_ButtonLayout::GetButton(const FName Function, const FGameplayTag& Default)
{
	const FGameplayTag Button = Get().FindButton(Function);
	return Button.IsValid() ? Button : Default;
}

FGameplayTag UGSTN_ButtonLayout::FindButton(const FName Function, const FGSTN_InputProfile& Profile) const
{
	if (IsRemappable(Function))
	{
		const FGameplayTag Override = Profile.FindOverride(Function);
		if (Override.IsValid())
		{
			return Override;
		}
	}
	return FindButton(Function);
}

const FGSTN_CommandChoiceSet* UGSTN_ButtonLayout::FindChoices(const FGameplayTag& VirtualCommand) const
{
	if (!VirtualCommand.IsValid())
	{
		return nullptr;
	}
	return CommandChoices.FindByPredicate([&VirtualCommand](const FGSTN_CommandChoiceSet& Set) { return Set.VirtualCommand == VirtualCommand; });
}

const FGSTN_CommandChoice* FGSTN_CommandChoiceSet::FindChoice(const FName Id) const
{
	if (Id.IsNone())
	{
		return nullptr;
	}
	return Choices.FindByPredicate([Id](const FGSTN_CommandChoice& Choice) { return Choice.Id == Id; });
}

const FGSTN_CommandChoice* FGSTN_CommandChoiceSet::Resolve(const FGSTN_InputProfile& Profile) const
{
	if (const FGSTN_CommandChoice* Picked = FindChoice(Profile.FindPick(VirtualCommand)))
	{
		return Picked;
	}
	return FindChoice(GetDefault(Profile.Device));
}
