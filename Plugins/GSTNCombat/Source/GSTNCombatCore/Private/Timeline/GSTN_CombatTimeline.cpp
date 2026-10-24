#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineValidation.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FPrimaryAssetId UGSTN_CombatTimeline::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(FPrimaryAssetType(TEXT("GSTN_CombatTimeline")), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UGSTN_CombatTimeline::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// The simulation tick rate is a project setting of the NP layer, which the Core cannot see: the montage check assumes
	// the default 60 Hz tick here and the details panel validation of the ability does the exact one
	TArray<FGSTN_TimelineMessage> Messages;
	FGSTN_TimelineValidation::Validate(Data, FGSTN_TimelineValidation::MakeContext(Data, 60.f), Messages);
	for (const FGSTN_TimelineMessage& Message : Messages)
	{
		if (Message.bError)
		{
			Context.AddError(FText::FromString(Message.Text));
			Result = EDataValidationResult::Invalid;
		}
		else
		{
			Context.AddWarning(FText::FromString(Message.Text));
		}
	}

	if (Result == EDataValidationResult::NotValidated)
	{
		Result = EDataValidationResult::Valid;
	}
	return Result;
}
#endif
