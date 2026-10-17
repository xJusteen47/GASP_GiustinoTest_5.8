// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputMatcher.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_CombatInputSettings.h"
#include "Input/GSTN_InputHistory.h"

namespace
{
	/** The state of one search: what the steps found so far and how much work was spent */
	struct FSearch
	{
		const FGSTN_InputPattern& Pattern;
		const FGSTN_InputHistory& History;
		const FGSTN_MatchParams& Params;

		TArray<int32, TInlineAllocator<4>> Anchors;
		TArray<uint8, TInlineAllocator<8>> Used;
		int32 TerminalPressSlot = INDEX_NONE;
		bool bViaHoldBuffer = false;

		int32 Evaluations = 0;
		bool bAborted = false;

		/** Counts one condition evaluation; false once the budget is spent */
		bool SpendEvaluation()
		{
			if (++Evaluations > FGSTN_InputMatcher::MaxEvaluations)
			{
				bAborted = true;
			}
			return !bAborted;
		}
	};

	uint32 SlotsToMask(const FGSTN_ConditionResult& Result)
	{
		uint32 Mask = 0;
		for (const uint8 Slot : Result.UsedEvents)
		{
			Mask |= 1u << Slot;
		}
		return Mask;
	}

	/** The terminal press must be young enough, or still held when the hold buffer is on */
	bool IsTerminalAccepted(FSearch& Search, const FGSTN_ConditionResult& Anchor, bool& bOutViaHoldBuffer)
	{
		const FGSTN_MatchParams& Params = Search.Params;
		const int32 Age = FGSTN_InputMatcher::GetAge(Anchor.Frame, Params);
		bOutViaHoldBuffer = false;
		if (Age <= Params.BufferFrames)
		{
			return true;
		}
		if (!Params.bHoldBuffer || Anchor.TerminalPressSlot == INDEX_NONE)
		{
			return false;
		}
		if (Params.MaxHoldBufferFrames > 0 && Age > Params.MaxHoldBufferFrames)
		{
			return false;
		}
		const FGSTN_ButtonEvent& Press = Search.History.GetButtonEvent(Anchor.TerminalPressSlot);
		bOutViaHoldBuffer = Search.History.IsHeldContinuouslySince(Press.Button, Press.Frame);
		return bOutViaHoldBuffer;
	}

	/**
	 * Matches Pattern.Steps[StepIndex] and, recursively, every step before it. UpperAnchor is the latest frame the step's
	 * anchor may have (Now for the terminal step, the next step's anchor otherwise); Excluded the button events taken
	 * by later steps or reserved.
	 */
	bool MatchStep(FSearch& Search, int32 StepIndex, int32 UpperAnchor, uint32 Excluded)
	{
		const FGSTN_MatchParams& Params = Search.Params;
		const FGSTN_InputStep& Step = Search.Pattern.Steps[StepIndex];
		const bool bTerminal = StepIndex == Search.Pattern.Steps.Num() - 1;

		const int32 AnchorIndex = Step.FindAnchorIndex();
		const FGSTN_InputCondition* AnchorCondition = AnchorIndex == INDEX_NONE ? nullptr : Step.AllOf[AnchorIndex].GetPtr();
		if (!AnchorCondition)
		{
			return false;
		}

		const int32 Simultaneity = Step.SimultaneityFrames >= 0 ? Step.SimultaneityFrames : Params.DefaultSimultaneityFrames;
		const int32 Earliest = bTerminal ? MIN_int32 : UpperAnchor - Step.MaxGapFrames;

		FGSTN_ConditionContext Context{ Search.History, *Params.Buttons };
		Context.Now = Params.Now;
		Context.SimultaneityFrames = Simultaneity;

		int32 AnchorLimit = UpperAnchor;
		uint32 Enumerated = Excluded;

		for (int32 Candidate = 0; Candidate < FGSTN_InputMatcher::MaxCandidatesPerStep; ++Candidate)
		{
			// The anchor condition lists its candidates newest first: each call finds the newest one not yet excluded.
			if (!Search.SpendEvaluation())
			{
				return false;
			}
			Context.EarliestFrame = Earliest;
			Context.ExcludedEvents = Enumerated;
			FGSTN_ConditionResult Anchor;
			if (!AnchorCondition->Matches(Context, AnchorLimit, Anchor))
			{
				return false;
			}

			// Where the next call continues, whatever this candidate turns out to be: a candidate made of button events
			// excludes them, a stick based one (no events) is passed by moving the limit one frame back.
			if (Anchor.UsedEvents.IsEmpty())
			{
				AnchorLimit = Anchor.Frame - 1;
			}
			else
			{
				AnchorLimit = Anchor.Frame;
				Enumerated |= SlotsToMask(Anchor);
			}

			bool bViaHoldBuffer = false;
			if (bTerminal && !IsTerminalAccepted(Search, Anchor, bViaHoldBuffer))
			{
				continue;
			}

			// The step's other conditions, relative to this anchor.
			uint32 StepExcluded = Excluded | SlotsToMask(Anchor);
			TArray<uint8, TInlineAllocator<8>> StepUsed(Anchor.UsedEvents);
			bool bStepHolds = true;
			for (int32 Index = 0; Index < Step.AllOf.Num() && bStepHolds; ++Index)
			{
				const FGSTN_InputCondition* Condition = Step.AllOf[Index].GetPtr();
				if (Index == AnchorIndex)
				{
					continue;
				}
				if (!Condition)
				{
					bStepHolds = false;
					break;
				}
				if (!Search.SpendEvaluation())
				{
					return false;
				}

				FGSTN_ConditionContext Other = Context;
				Other.ExcludedEvents = StepExcluded;
				int32 ReferenceFrame = Anchor.Frame;
				if (Condition->IsAnchorCondition())
				{
					// Another reference event next to the anchor, e.g. a spin that ends around the press.
					Other.EarliestFrame = Anchor.Frame - Simultaneity;
					ReferenceFrame = FMath::Min(Anchor.Frame + Simultaneity, Params.Now);
				}
				else
				{
					Other.EarliestFrame = MIN_int32;
					if (bTerminal && Params.DirectionPolicy == EGSTN_DirectionPolicy::AtExecution && Condition->IsDirectionCondition())
					{
						// Judged by the stick at the frame the command comes out
						ReferenceFrame = Params.Now;
						Other.SimultaneityFrames = 0;
					}
				}

				FGSTN_ConditionResult Result;
				bStepHolds = Condition->Matches(Other, ReferenceFrame, Result);
				if (bStepHolds)
				{
					StepExcluded |= SlotsToMask(Result);
					StepUsed.Append(Result.UsedEvents);
				}
			}
			if (!bStepHolds)
			{
				if (Search.bAborted)
				{
					return false;
				}
				continue;
			}

			// Commit this step, then the earlier ones; take it back if they cannot be completed.
			const int32 UsedBefore = Search.Used.Num();
			const int32 TerminalBefore = Search.TerminalPressSlot;
			const bool bHoldBefore = Search.bViaHoldBuffer;
			Search.Anchors[StepIndex] = Anchor.Frame;
			Search.Used.Append(StepUsed);
			if (bTerminal)
			{
				Search.TerminalPressSlot = Anchor.TerminalPressSlot;
				Search.bViaHoldBuffer = bViaHoldBuffer;
			}

			if (StepIndex == 0 || MatchStep(Search, StepIndex - 1, Anchor.Frame, StepExcluded))
			{
				return true;
			}

			Search.Used.SetNum(UsedBefore);
			Search.TerminalPressSlot = TerminalBefore;
			Search.bViaHoldBuffer = bHoldBefore;
			if (Search.bAborted)
			{
				return false;
			}
		}
		return false;
	}
}

FGSTN_MatchParams FGSTN_MatchParams::FromSettings(const UGSTN_CombatInputSettings& Settings, const FGSTN_ButtonMap& Buttons, int32 Now)
{
	FGSTN_MatchParams Params;
	Params.Buttons = &Buttons;
	Params.Now = Now;
	Params.BufferFrames = Settings.DefaultBufferFrames;
	Params.bHoldBuffer = Settings.bHoldBufferByDefault;
	Params.MaxHoldBufferFrames = Settings.MaxHoldBufferFrames;
	Params.DefaultSimultaneityFrames = Settings.DefaultSimultaneityFrames;
	return Params;
}

int32 FGSTN_InputMatcher::GetAge(int32 Frame, const FGSTN_MatchParams& Params)
{
	int32 Age = Params.Now - Frame;
	if (Params.PausedStart != INDEX_NONE)
	{
		// Paused frames that fall between the event and now
		Age -= FMath::Max(0, FMath::Min(Params.Now, Params.PausedEnd) - FMath::Max(Frame, Params.PausedStart));
	}
	return Age;
}

bool FGSTN_InputMatcher::Match(const FGSTN_InputPattern& Pattern, const FGSTN_InputHistory& History, const FGSTN_MatchParams& Params, FGSTN_PatternMatch& Out)
{
	Out = FGSTN_PatternMatch();
	if (!ensureMsgf(Params.Buttons, TEXT("FGSTN_MatchParams needs a button map")) || Pattern.Steps.IsEmpty())
	{
		return false;
	}

	FSearch Search{ Pattern, History, Params };
	Search.Anchors.SetNumZeroed(Pattern.Steps.Num());

	const bool bMatched = MatchStep(Search, Pattern.Steps.Num() - 1, Params.Now, Params.ReservedEvents);
	ensureMsgf(!Search.bAborted, TEXT("Input pattern matching gave up after %d evaluations: the pattern is too permissive"), MaxEvaluations);
	if (!bMatched)
	{
		return false;
	}

	Out.StepAnchors = MoveTemp(Search.Anchors);
	Out.UsedButtonEvents = MoveTemp(Search.Used);
	Out.TerminalPressEvent = Search.TerminalPressSlot;
	Out.bViaHoldBuffer = Search.bViaHoldBuffer;
	return true;
}
