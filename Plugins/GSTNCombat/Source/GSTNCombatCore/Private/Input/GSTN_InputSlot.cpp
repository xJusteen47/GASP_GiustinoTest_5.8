// Pure combat logic (no GAS, no Network Prediction).

#include "Input/GSTN_InputSlot.h"
#include "Input/GSTN_CombatInputSettings.h"

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
