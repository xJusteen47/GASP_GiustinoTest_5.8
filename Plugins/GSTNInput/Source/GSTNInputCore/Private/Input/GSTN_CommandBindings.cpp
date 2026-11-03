// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_CommandBindings.h"

int32 UGSTN_CommandBindingUtils::SelectBinding(TConstArrayView<const FGSTN_TagRequirements*> Candidates, const FGameplayTagContainer& Context)
{
	int32 Best = INDEX_NONE;
	int32 BestRequired = -1;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		const FGSTN_TagRequirements* Requirements = Candidates[Index];
		if (Requirements && Requirements->IsSatisfiedBy(Context) && Requirements->GetRequiredCount() > BestRequired)
		{
			Best = Index;
			BestRequired = Requirements->GetRequiredCount();
		}
	}
	return Best;
}

TArray<FGSTN_BindingConflict> UGSTN_CommandBindingUtils::FindBindingConflicts(TConstArrayView<FGSTN_BindingKey> Bindings, TConstArrayView<FGSTN_ExclusiveTagGroup> ExclusiveGroups)
{
	TArray<FGSTN_BindingConflict> Conflicts;
	for (int32 A = 0; A < Bindings.Num(); ++A)
	{
		if (!Bindings[A].Command.IsValid() || !Bindings[A].Requirements)
		{
			continue;
		}
		for (int32 B = A + 1; B < Bindings.Num(); ++B)
		{
			if (Bindings[B].Command == Bindings[A].Command && Bindings[B].Requirements
				&& Bindings[A].Requirements->CanHoldTogether(*Bindings[B].Requirements, ExclusiveGroups)
				&& Bindings[A].Requirements->GetRequiredCount() == Bindings[B].Requirements->GetRequiredCount())
			{
				FGSTN_BindingConflict Conflict;
				Conflict.BindingA = A;
				Conflict.BindingB = B;
				Conflict.Message = FString::Printf(TEXT("Bindings %d and %d of %s can both hold and require the same number of tags (%d): the choice between them is arbitrary."),
					A, B, *Bindings[A].Command.ToString(), Bindings[A].Requirements->GetRequiredCount());
				Conflicts.Add(MoveTemp(Conflict));
			}
		}
	}
	return Conflicts;
}

TArray<FGSTN_BindingConflict> UGSTN_CommandBindingUtils::FindImpossibleBindings(TConstArrayView<FGSTN_BindingKey> Bindings, TConstArrayView<FGSTN_ExclusiveTagGroup> ExclusiveGroups)
{
	TArray<FGSTN_BindingConflict> Impossible;
	for (int32 Index = 0; Index < Bindings.Num(); ++Index)
	{
		if (Bindings[Index].Command.IsValid() && Bindings[Index].Requirements && Bindings[Index].Requirements->RequiresTwoOfAGroup(ExclusiveGroups))
		{
			FGSTN_BindingConflict Entry;
			Entry.BindingA = Index;
			Entry.BindingB = Index;
			Entry.Message = FString::Printf(TEXT("Binding %d of %s requires two tags of one exclusive context group: no context can satisfy it."),
				Index, *Bindings[Index].Command.ToString());
			Impossible.Add(MoveTemp(Entry));
		}
	}
	return Impossible;
}
