// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputSlot.h"
#include "Input/GSTN_CombatInputSettings.h"
#include "Input/GSTN_InputSlotValidation.h"
#include "Input/GSTN_StandardSlots.h"
#include "Misc/DataValidation.h"

FGSTN_CommandPriority FGSTN_InputSlotSpec::GetPriority(const UGSTN_CombatInputSettings& Settings) const
{
	FGSTN_CommandPriority Priority;
	Priority.Tier = Settings.GetCommandTier(Command);
	Priority.Specificity = Pattern.GetSpecificity();
	Priority.Offset = PriorityOffset;
	return Priority;
}

bool FGSTN_InputSlotSpec::UsesChordGrace(const UGSTN_CombatInputSettings& Settings) const
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

FGSTN_MatchParams FGSTN_InputSlotSpec::MakeMatchParams(const UGSTN_CombatInputSettings& Settings, const FGSTN_ButtonMap& Buttons, int32 Now) const
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

	for (const FGSTN_SlotConflict& Conflict : UGSTN_InputSlotValidation::FindConflicts(*this, UGSTN_CombatInputSettings::Get()))
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

void UGSTN_InputSlotSet::FillWithStandardSlots()
{
	Modify();
	Slots.Reset();
	for (const FGSTN_InputSlotSpec& Spec : UGSTN_StandardSlots::Build())
	{
		FGSTN_InputSlotEntry Entry;
		Entry.Inline = Spec;
		Slots.Add(MoveTemp(Entry));
	}
	MarkPackageDirty();
}

#undef LOCTEXT_NAMESPACE
#endif
