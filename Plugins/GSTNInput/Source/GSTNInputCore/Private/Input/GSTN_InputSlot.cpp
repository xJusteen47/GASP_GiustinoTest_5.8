// Pure input logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputSlot.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_InputSlotValidation.h"
#include "Input/GSTN_InputConditions.h"
#include "Layout/GSTN_ButtonLayout.h"
#include "Misc/DataValidation.h"

FGSTN_CommandPriority FGSTN_InputSlotSpec::GetPriority(const UGSTN_InputSettings& Settings) const
{
	const FGSTN_PriorityContext Context{ *this, Pattern, Settings };
	FGSTN_CommandPriority Priority;
	for (const TInstancedStruct<FGSTN_PriorityCriterion>& Criterion : Settings.PriorityCriteria)
	{
		if (Priority.Scores.Num() == GSTNPriority::MaxCriteria)
		{
			break;
		}
		// An entry left empty in the editor scores 0, so every slot keeps one score per listed criterion.
		const FGSTN_PriorityCriterion* Instance = Criterion.GetPtr<FGSTN_PriorityCriterion>();
		Priority.Scores.Add(Instance ? Instance->Score(Context) : 0);
	}
	return Priority;
}

bool FGSTN_InputSlotSpec::UsesChordGrace(const UGSTN_InputSettings& Settings) const
{
	switch (ChordGrace)
	{
	case EGSTN_OverrideBool::On:
		return true;
	case EGSTN_OverrideBool::Off:
		return false;
	default:
		return Settings.bChordGraceByDefault;
	}
}

FGSTN_MatchParams FGSTN_InputSlotSpec::MakeMatchParams(const UGSTN_InputSettings& Settings, const FGSTN_ButtonMap& Buttons, int32 Now) const
{
	FGSTN_MatchParams Params = FGSTN_MatchParams::FromSettings(Settings, Buttons, Now);
	if (BufferFramesOverride >= 0)
	{
		Params.BufferFrames = BufferFramesOverride;
	}
	if (HoldBuffer != EGSTN_OverrideBool::Default)
	{
		Params.bHoldBuffer = HoldBuffer == EGSTN_OverrideBool::On;
	}
	Params.DirectionPolicy = DirectionPolicy;
	return Params;
}

FPrimaryAssetId UGSTN_InputSlotAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("GSTN_InputSlot")), GetFName());
}

void UGSTN_InputSlotSet::GetSpecs(TArray<const FGSTN_InputSlotSpec*>& OutSpecs) const
{
	OutSpecs.Reset(Slots.Num());
	for (const FGSTN_InputSlotEntry& Entry : Slots)
	{
		OutSpecs.Add(&Entry.GetSpec());
	}
}

#if WITH_EDITOR
#define LOCTEXT_NAMESPACE "GSTNInputSlotSet"

EDataValidationResult UGSTN_InputSlotSet::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	for (const FGSTN_SlotConflict& Conflict : UGSTN_InputSlotValidation::FindConflicts(*this, UGSTN_InputSettings::Get()))
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
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}

#undef LOCTEXT_NAMESPACE
#endif

FGameplayTag FGSTN_InputSlotSpec::AddLayoutButton(FName Function, const FGameplayTag& Default)
{
	if (!Function.IsNone() && Default.IsValid())
	{
		FGSTN_SlotLayoutButton& Entry = LayoutButtons.AddDefaulted_GetRef();
		Entry.Button = Default;
		Entry.Function = Function;
	}
	return Default;
}

void FGSTN_InputSlotSpec::ApplyButtonLayout(const UGSTN_ButtonLayout& Layout)
{
	if (LayoutButtons.IsEmpty())
	{
		return;
	}
	TMap<FGameplayTag, FGameplayTag> Map;
	for (const FGSTN_SlotLayoutButton& Entry : LayoutButtons)
	{
		const FGameplayTag Button = Layout.FindButton(Entry.Function);
		if (Button.IsValid() && Button != Entry.Button && !Map.Contains(Entry.Button))
		{
			Map.Add(Entry.Button, Button);
		}
	}
	if (!Map.IsEmpty())
	{
		for (FGSTN_InputStep& Step : Pattern.Steps)
		{
			for (TInstancedStruct<FGSTN_InputCondition>& Condition : Step.AllOf)
			{
				if (FGSTN_InputCondition* Pointer = Condition.GetMutablePtr())
				{
					Pointer->RemapButtons(Map);
				}
			}
		}
	}
	LayoutButtons.Reset();
}

void FGSTN_InputSlotSpec::ApplyActiveButtonLayout()
{
	ApplyButtonLayout(UGSTN_ButtonLayout::Get());
}
