// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputSlotValidation.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_CombatInputSettings.h"
#include "Input/GSTN_InputHistory.h"
#include "Input/GSTN_InputMatcher.h"
#include "Input/GSTN_InputScript.h"

namespace
{
	/** The input that satisfies a slot, played into a history */
	struct FCanonical
	{
		bool bValid = false;
		FGSTN_InputHistory History;
	};

	/** Frame the canonical inputs start at: room for holds and for the stick to settle before the first event */
	constexpr int32 CanonicalStartFrame = 20;

	FString Name(const FGSTN_InputSlotSpec& Spec)
	{
		return Spec.Command.IsValid() ? Spec.Command.ToString() : FString(TEXT("(no command)"));
	}

	void Add(TArray<FGSTN_SlotConflict>& Out, int32 SlotA, int32 SlotB, EGSTN_SlotConflictKind Kind, const FString& Message)
	{
		FGSTN_SlotConflict Conflict;
		Conflict.SlotA = SlotA;
		Conflict.SlotB = SlotB;
		Conflict.Kind = Kind;
		Conflict.Message = Message;
		Out.Add(MoveTemp(Conflict));
	}
}

TArray<FGSTN_SlotConflict> UGSTN_InputSlotValidation::FindConflicts(const UGSTN_InputSlotSet& Set, const UGSTN_CombatInputSettings& Settings)
{
	TArray<const FGSTN_InputSlotSpec*> Specs;
	Set.GetSpecs(Specs);
	return FindConflicts(Specs, Settings);
}

TArray<FGSTN_SlotConflict> UGSTN_InputSlotValidation::FindConflicts(TConstArrayView<const FGSTN_InputSlotSpec*> Slots, const UGSTN_CombatInputSettings& Settings)
{
	TArray<FGSTN_SlotConflict> Conflicts;

	// Bits for the check: the set's own button tags, sorted by name so every run gives the same map.
	FGameplayTagContainer Referenced;
	for (const FGSTN_InputSlotSpec* Spec : Slots)
	{
		for (const FGSTN_InputStep& Step : Spec->Pattern.Steps)
		{
			for (const TInstancedStruct<FGSTN_InputCondition>& Condition : Step.AllOf)
			{
				if (const FGSTN_InputCondition* Pointer = Condition.GetPtr())
				{
					Pointer->GetReferencedButtons(Referenced);
				}
			}
		}
	}
	TArray<FGameplayTag> ButtonTags = Referenced.GetGameplayTagArray();
	ButtonTags.Sort([](const FGameplayTag& A, const FGameplayTag& B) { return A.GetTagName().Compare(B.GetTagName()) < 0; });
	if (ButtonTags.Num() > 32)
	{
		Add(Conflicts, INDEX_NONE, INDEX_NONE, EGSTN_SlotConflictKind::InvalidSlot,
			FString::Printf(TEXT("The slots use %d different buttons, an input config holds 32."), ButtonTags.Num()));
		ButtonTags.SetNum(32);
	}
	const FGSTN_ButtonMap Buttons(ButtonTags);

	// Structure, duplicates and each slot's canonical input.
	TArray<FCanonical> Canonical;
	Canonical.SetNum(Slots.Num());
	TArray<FGSTN_CommandPriority> Priorities;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const FGSTN_InputSlotSpec& Spec = *Slots[Index];
		Priorities.Add(Spec.GetPriority(Settings));

		if (!Spec.Command.IsValid())
		{
			Add(Conflicts, Index, INDEX_NONE, EGSTN_SlotConflictKind::InvalidSlot, FString::Printf(TEXT("Slot %d has no command tag."), Index));
		}
		for (int32 Other = 0; Other < Index; ++Other)
		{
			if (Spec.Command.IsValid() && Slots[Other]->Command == Spec.Command)
			{
				Add(Conflicts, Other, Index, EGSTN_SlotConflictKind::DuplicateCommand,
					FString::Printf(TEXT("Slots %d and %d both define the command %s."), Other, Index, *Name(Spec)));
			}
		}

		FString Reason;
		if (!Spec.Pattern.IsStructurallyValid(&Reason))
		{
			Add(Conflicts, Index, INDEX_NONE, EGSTN_SlotConflictKind::InvalidSlot, FString::Printf(TEXT("%s: %s."), *Name(Spec), *Reason));
			continue;
		}

		FGSTN_InputScript Script(ButtonTags);
		int32 Frame = CanonicalStartFrame;
		Spec.Pattern.AppendCanonicalInput(Script, Buttons, Frame);
		FString PlayError;
		if (Script.Advance(Canonical[Index].History, Settings, Frame, &PlayError))
		{
			Canonical[Index].bValid = true;
		}
		else
		{
			Add(Conflicts, Index, INDEX_NONE, EGSTN_SlotConflictKind::InvalidSlot,
				FString::Printf(TEXT("%s: its canonical input cannot be played (%s)."), *Name(Spec), *PlayError));
		}
	}

	// Every slot's own input against every other slot.
	TArray<TPair<int32, int32>, TInlineAllocator<8>> ReportedAmbiguities;
	for (int32 A = 0; A < Slots.Num(); ++A)
	{
		if (!Canonical[A].bValid)
		{
			continue;
		}
		for (int32 B = 0; B < Slots.Num(); ++B)
		{
			if (B == A || !Slots[B]->Pattern.IsStructurallyValid())
			{
				continue;
			}

			FGSTN_PatternMatch Match;
			const FGSTN_InputHistory& History = Canonical[A].History;
			if (!FGSTN_InputMatcher::Match(Slots[B]->Pattern, History, Slots[B]->MakeMatchParams(Settings, Buttons, History.GetLatestFrame()), Match))
			{
				continue;
			}

			if (Priorities[B] == Priorities[A])
			{
				const TPair<int32, int32> Pair(FMath::Min(A, B), FMath::Max(A, B));
				if (!ReportedAmbiguities.Contains(Pair))
				{
					ReportedAmbiguities.Add(Pair);
					Add(Conflicts, Pair.Key, Pair.Value, EGSTN_SlotConflictKind::Ambiguous,
						FString::Printf(TEXT("%s and %s are ambiguous: the input of %s also triggers %s with the same priority, so the slot order decides."),
							*Name(*Slots[Pair.Key]), *Name(*Slots[Pair.Value]), *Name(*Slots[A]), *Name(*Slots[B])));
				}
			}
			else if (Priorities[A] < Priorities[B])
			{
				Add(Conflicts, A, B, EGSTN_SlotConflictKind::Shadowed,
					FString::Printf(TEXT("%s is shadowed by %s: the input of %s also matches %s, which ranks higher."),
						*Name(*Slots[A]), *Name(*Slots[B]), *Name(*Slots[A]), *Name(*Slots[B])));
			}
		}
	}
	return Conflicts;
}
