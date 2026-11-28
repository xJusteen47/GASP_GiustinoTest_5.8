// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputProfile.h"

#include "Layout/GSTN_ButtonLayout.h"

FGSTN_InputProfile FGSTN_InputProfile::Sanitized(const UGSTN_ButtonLayout& Layout) const
{
	FGSTN_InputProfile Out;
	Out.Device = Device == EGSTN_InputDevice::MouseKeyboard ? EGSTN_InputDevice::MouseKeyboard : EGSTN_InputDevice::Gamepad;

	// A logical button is any tag under Input.Button; the input config decides which of them a key can press
	const FGameplayTag ButtonRoot = FGameplayTag::RequestGameplayTag(TEXT("Input.Button"), false);
	for (const FGSTN_ButtonOverride& Override : ButtonOverrides)
	{
		if (Out.ButtonOverrides.Num() >= MaxEntries)
		{
			break;
		}
		const bool bButtonOk = Override.Button.IsValid() && (!ButtonRoot.IsValid() || Override.Button.MatchesTag(ButtonRoot));
		const bool bSeen = Out.ButtonOverrides.ContainsByPredicate([&Override](const FGSTN_ButtonOverride& Kept) { return Kept.Function == Override.Function; });
		if (bButtonOk && !bSeen && Layout.IsRemappable(Override.Function))
		{
			Out.ButtonOverrides.Add(Override);
		}
	}
	for (const FGSTN_CommandPick& Pick : CommandPicks)
	{
		if (Out.CommandPicks.Num() >= MaxEntries)
		{
			break;
		}
		const FGSTN_CommandChoiceSet* Set = Layout.FindChoices(Pick.VirtualCommand);
		const bool bSeen = Out.CommandPicks.ContainsByPredicate([&Pick](const FGSTN_CommandPick& Kept) { return Kept.VirtualCommand == Pick.VirtualCommand; });
		if (Set && Set->FindChoice(Pick.Choice) && !bSeen)
		{
			Out.CommandPicks.Add(Pick);
		}
	}

	// Same choices, same bytes, same table checksum: the order the player made them in must not matter
	Out.ButtonOverrides.Sort([](const FGSTN_ButtonOverride& A, const FGSTN_ButtonOverride& B) { return A.Function.LexicalLess(B.Function); });
	Out.CommandPicks.Sort([](const FGSTN_CommandPick& A, const FGSTN_CommandPick& B) { return A.VirtualCommand.GetTagName().LexicalLess(B.VirtualCommand.GetTagName()); });
	return Out;
}

FGameplayTag FGSTN_InputProfile::FindOverride(const FName Function) const
{
	const FGSTN_ButtonOverride* Found = ButtonOverrides.FindByPredicate([Function](const FGSTN_ButtonOverride& Entry) { return Entry.Function == Function; });
	return Found ? Found->Button : FGameplayTag();
}

FName FGSTN_InputProfile::FindPick(const FGameplayTag& VirtualCommand) const
{
	const FGSTN_CommandPick* Found = CommandPicks.FindByPredicate([&VirtualCommand](const FGSTN_CommandPick& Entry) { return Entry.VirtualCommand == VirtualCommand; });
	return Found ? Found->Choice : NAME_None;
}
