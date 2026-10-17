// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputPattern.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_InputScript.h"

int32 FGSTN_InputStep::FindAnchorIndex() const
{
	for (int32 Index = 0; Index < AllOf.Num(); ++Index)
	{
		const FGSTN_InputCondition* Condition = AllOf[Index].GetPtr();
		if (Condition && Condition->IsAnchorCondition())
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 FGSTN_InputStep::GetSpecificity() const
{
	int32 Result = 0;
	for (const TInstancedStruct<FGSTN_InputCondition>& Condition : AllOf)
	{
		if (const FGSTN_InputCondition* Pointer = Condition.GetPtr())
		{
			Result += Pointer->GetSpecificity();
		}
	}
	return Result;
}

int32 FGSTN_InputPattern::GetSpecificity() const
{
	int32 Result = 0;
	for (const FGSTN_InputStep& Step : Steps)
	{
		Result += Step.GetSpecificity();
	}
	return Result;
}

bool FGSTN_InputPattern::IsStructurallyValid(FString* OutReason) const
{
	if (Steps.IsEmpty())
	{
		if (OutReason)
		{
			*OutReason = TEXT("the pattern has no step");
		}
		return false;
	}
	for (int32 Index = 0; Index < Steps.Num(); ++Index)
	{
		if (Steps[Index].FindAnchorIndex() == INDEX_NONE)
		{
			if (OutReason)
			{
				*OutReason = FString::Printf(TEXT("step %d has no anchor condition (press, release, chord, motion, spin or mash)"), Index);
			}
			return false;
		}
	}
	return true;
}

void FGSTN_InputPattern::AppendCanonicalInput(FGSTN_InputScript& Script, const FGSTN_ButtonMap& Buttons, int32& Frame) const
{
	for (int32 StepIndex = 0; StepIndex < Steps.Num(); ++StepIndex)
	{
		const FGSTN_InputStep& Step = Steps[StepIndex];
		const int32 AnchorIndex = Step.FindAnchorIndex();

		// States first (stick, held buttons), then the other anchor-capable conditions, the anchor last: it lands on the
		// frame all of them have led up to.
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			for (int32 Index = 0; Index < Step.AllOf.Num(); ++Index)
			{
				const FGSTN_InputCondition* Condition = Step.AllOf[Index].GetPtr();
				if (!Condition)
				{
					continue;
				}
				const int32 ConditionPass = Index == AnchorIndex ? 2 : (Condition->IsAnchorCondition() ? 1 : 0);
				if (ConditionPass == Pass)
				{
					Condition->AppendCanonicalInput(Script, Buttons, Frame);
				}
			}
		}

		if (StepIndex < Steps.Num() - 1)
		{
			++Frame;
		}
	}
}
