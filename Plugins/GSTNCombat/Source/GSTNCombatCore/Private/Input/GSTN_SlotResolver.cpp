// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_SlotResolver.h"
#include "Algo/Sort.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_CombatInputSettings.h"
#include "Input/GSTN_InputConditions.h"
#include "Input/GSTN_InputHistory.h"

namespace
{
	/** A slot whose pattern matched, with what the resolver needs to rank it */
	struct FCandidate
	{
		int32 Index = INDEX_NONE;
		FGSTN_CommandPriority Priority;
		FGSTN_PatternMatch Match;

		/** Frame of the terminal press (or of the terminal anchor when it is not a press): what press groups are made of */
		int32 Frame = 0;

		bool bInGroup = true;
	};
	using FCandidates = TArray<FCandidate, TInlineAllocator<16>>;

	FGSTN_MatchParams MakeMatchParams(const FGSTN_ResolveParams& Params, const UGSTN_CombatInputSettings& Settings, const FGSTN_InputSlotSpec& Spec, uint32 Reserved)
	{
		FGSTN_MatchParams Result = Spec.MakeMatchParams(Settings, *Params.Buttons, Params.Now);
		Result.ReservedEvents = Reserved;
		Result.PausedStart = Params.PausedStart;
		Result.PausedEnd = Params.PausedEnd;
		return Result;
	}

	uint32 SlotsToMask(const TArray<uint8, TInlineAllocator<8>>& Slots)
	{
		uint32 Mask = 0;
		for (const uint8 Slot : Slots)
		{
			Mask |= 1u << Slot;
		}
		return Mask;
	}

	/** The anchor condition of a slot's terminal step if it is a T, null otherwise (also for a malformed pattern) */
	template<typename T>
	const T* GetTerminalAnchorAs(const FGSTN_InputSlotSpec& Spec)
	{
		if (Spec.Pattern.Steps.IsEmpty())
		{
			return nullptr;
		}
		const FGSTN_InputStep& Terminal = Spec.Pattern.Steps.Last();
		const int32 AnchorIndex = Terminal.FindAnchorIndex();
		return AnchorIndex == INDEX_NONE ? nullptr : Terminal.AllOf[AnchorIndex].GetPtr<T>();
	}

	/**
	 * A single-button command waits a few frames when a higher command with a chord on the same button has not matched yet:
	 * the player may be about to press the second button.
	 */
	bool ShouldDeferForChord(const FCandidate& Winner, TConstArrayView<const FGSTN_InputSlotSpec*> Slots, const FCandidates& Candidates,
		const FGSTN_InputHistory& History, const FGSTN_ResolveParams& Params, const UGSTN_CombatInputSettings& Settings)
	{
		const FGSTN_InputSlotSpec& Spec = *Slots[Winner.Index];
		if (!Spec.UsesChordGrace(Settings) || Winner.Match.TerminalPressEvent == INDEX_NONE || !GetTerminalAnchorAs<FGSTN_InputCondition_Press>(Spec))
		{
			return false;
		}

		FGSTN_MatchParams AgeParams;
		AgeParams.Now = Params.Now;
		AgeParams.PausedStart = Params.PausedStart;
		AgeParams.PausedEnd = Params.PausedEnd;
		const FGSTN_ButtonEvent& Press = History.GetButtonEvent(Winner.Match.TerminalPressEvent);
		if (FGSTN_InputMatcher::GetAge(Press.Frame, AgeParams) >= Settings.ChordGraceFrames)
		{
			return false;
		}

		// A slot of higher rank that ends in a chord containing this button and has not matched (yet).
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			const FGSTN_InputSlotSpec* Other = Slots[Index];
			if (!Other || Index == Winner.Index || !(Winner.Priority < Other->GetPriority(Settings)))
			{
				continue;
			}
			const FGSTN_InputCondition_Chord* Chord = GetTerminalAnchorAs<FGSTN_InputCondition_Chord>(*Other);
			const bool bMatched = Candidates.ContainsByPredicate([Index](const FCandidate& Candidate) { return Candidate.Index == Index; });
			if (Chord && !bMatched && (Params.Buttons->GetMask(Chord->Buttons) & (1u << Press.Button)) != 0)
			{
				return true;
			}
		}
		return false;
	}
}

FGSTN_SlotResolution FGSTN_SlotResolver::Resolve(TConstArrayView<const FGSTN_InputSlotSpec*> Slots, const FGSTN_InputHistory& History,
	const FGSTN_ResolveParams& Params, const UGSTN_CombatInputSettings& Settings, TFunctionRef<bool(int32 SlotIndex)> CanExecute)
{
	FGSTN_SlotResolution Resolution;
	if (!ensureMsgf(Params.Buttons, TEXT("FGSTN_ResolveParams needs a button map")))
	{
		return Resolution;
	}

	// 1. Match every slot whose context holds, each with its own buffer rules.
	FCandidates Candidates;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const FGSTN_InputSlotSpec* Spec = Slots[Index];
		if (!Spec)
		{
			continue;
		}
		FCandidate Candidate;
		if (FGSTN_InputMatcher::Match(Spec->Pattern, History, MakeMatchParams(Params, Settings, *Spec, 0), Candidate.Match))
		{
			Candidate.Index = Index;
			Candidate.Priority = Spec->GetPriority(Settings);
			Candidate.Frame = Candidate.Match.StepAnchors.Last();
			Candidates.Add(MoveTemp(Candidate));
		}
	}
	if (Candidates.IsEmpty())
	{
		return Resolution;
	}

	// 2. Press group: only the presses around the newest (or oldest) one compete.
	int32 GroupFrame = Candidates[0].Frame;
	for (const FCandidate& Candidate : Candidates)
	{
		GroupFrame = Settings.BufferConflictPolicy == EGSTN_BufferConflictPolicy::Oldest ? FMath::Min(GroupFrame, Candidate.Frame) : FMath::Max(GroupFrame, Candidate.Frame);
	}
	if (Settings.BufferConflictPolicy != EGSTN_BufferConflictPolicy::HighestPriority)
	{
		for (FCandidate& Candidate : Candidates)
		{
			Candidate.bInGroup = FMath::Abs(Candidate.Frame - GroupFrame) <= Settings.SimultaneousFrames;
		}
	}

	// 3. Order by rank, descending; the slot order makes the order total.
	TArray<int32, TInlineAllocator<16>> Order;
	for (int32 Position = 0; Position < Candidates.Num(); ++Position)
	{
		if (Candidates[Position].bInGroup)
		{
			Order.Add(Position);
		}
	}
	Algo::Sort(Order, [&Candidates](int32 A, int32 B)
	{
		const FCandidate& First = Candidates[A];
		const FCandidate& Second = Candidates[B];
		return Second.Priority < First.Priority || (First.Priority == Second.Priority && First.Index < Second.Index);
	});

	// 4. The first command that can execute wins; one that cannot keeps its press from the ones below it.
	uint32 Reserved = 0;
	int32 WinnerPosition = INDEX_NONE;
	for (const int32 Position : Order)
	{
		FCandidate& Candidate = Candidates[Position];
		const FGSTN_InputSlotSpec& Spec = *Slots[Candidate.Index];

		if ((Reserved & SlotsToMask(Candidate.Match.UsedButtonEvents)) != 0)
		{
			// A higher command reserved a press this match used: look for another one, or give up the slot.
			FGSTN_PatternMatch Rematch;
			if (!FGSTN_InputMatcher::Match(Spec.Pattern, History, MakeMatchParams(Params, Settings, Spec, Reserved), Rematch))
			{
				continue;
			}
			Candidate.Match = MoveTemp(Rematch);
			Candidate.Frame = Candidate.Match.StepAnchors.Last();
		}

		if (CanExecute(Candidate.Index))
		{
			if (ShouldDeferForChord(Candidate, Slots, Candidates, History, Params, Settings))
			{
				Resolution.bDeferredForChord = true;
				return Resolution;
			}
			WinnerPosition = Position;
			break;
		}
		if (!Spec.bFallThroughWhenUnavailable && Candidate.Match.TerminalPressEvent != INDEX_NONE)
		{
			Reserved |= 1u << Candidate.Match.TerminalPressEvent;
		}
	}
	if (WinnerPosition == INDEX_NONE)
	{
		return Resolution;
	}

	// 5. Consumption: the winner's events and the losing presses of its group.
	FCandidate& Winner = Candidates[WinnerPosition];
	Resolution.SlotIndex = Winner.Index;
	Resolution.EventsToConsume = Winner.Match.UsedButtonEvents;

	uint32 ExclusiveMask = 0;
	for (const FGameplayTag& Button : Settings.ExclusiveButtons)
	{
		ExclusiveMask |= Params.Buttons->GetMask(Button);
	}
	if (Settings.BufferConflictPolicy == EGSTN_BufferConflictPolicy::HighestPriority)
	{
		GroupFrame = Winner.Frame;
	}
	for (int32 Age = 0; Age < History.GetNumButtonEvents(); ++Age)
	{
		const int32 Slot = History.GetButtonSlot(Age);
		const FGSTN_ButtonEvent& Event = History.GetButtonEvent(Slot);
		if (!Event.bRelease && !Event.bConsumed && (ExclusiveMask & (1u << Event.Button)) != 0
			&& FMath::Abs(Event.Frame - GroupFrame) <= Settings.SimultaneousFrames && !Resolution.EventsToConsume.Contains(static_cast<uint8>(Slot)))
		{
			Resolution.EventsToConsume.Add(static_cast<uint8>(Slot));
		}
	}

	Resolution.Match = MoveTemp(Winner.Match);
	return Resolution;
}
