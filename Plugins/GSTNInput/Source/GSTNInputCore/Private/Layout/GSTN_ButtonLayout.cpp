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
