// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Kits/GSTN_SuperDefinition.h"
#include "Kits/GSTN_CombatAssetTypes.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FPrimaryAssetId UGSTN_SuperDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GSTNCombatAssetTypes::Super(), GetFName());
}

void UGSTN_SuperDefinition::Validate(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	if (Ability.IsNull())
	{
		OutErrors.Add(FText::FromString(TEXT("A super needs an ability.")));
	}
	if (MeterCost < 0.f)
	{
		OutErrors.Add(FText::FromString(TEXT("The meter cost cannot be negative.")));
	}
	else if (MeterCost > 0.f && !Meter.IsValid())
	{
		OutErrors.Add(FText::FromString(TEXT("The super costs meter but names no meter.")));
	}
	if (MeterCost == 0.f)
	{
		OutWarnings.Add(FText::FromString(TEXT("The super costs nothing.")));
	}
	if (SuperTags.IsEmpty())
	{
		OutWarnings.Add(FText::FromString(TEXT("The super has no tags, so no kit's AllowedSupers query can match it.")));
	}
}

#if WITH_EDITOR
EDataValidationResult UGSTN_SuperDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FText> Errors;
	TArray<FText> Warnings;
	Validate(Errors, Warnings);
	for (const FText& Error : Errors)
	{
		Context.AddError(Error);
		Result = EDataValidationResult::Invalid;
	}
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
