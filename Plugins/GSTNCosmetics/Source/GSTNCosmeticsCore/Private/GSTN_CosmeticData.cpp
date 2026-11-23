// Modular cosmetics, pure layer.

#include "GSTN_CosmeticData.h"
#include "GSTN_CosmeticPlanning.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
#if WITH_EDITOR
	EDataValidationResult Report(FDataValidationContext& Context, EDataValidationResult Result, const TArray<FString>& Errors, const TArray<FString>& Warnings)
	{
		for (const FString& Error : Errors)
		{
			Context.AddError(FText::FromString(Error));
		}
		for (const FString& Warning : Warnings)
		{
			Context.AddWarning(FText::FromString(Warning));
		}
		if (Errors.Num() > 0)
		{
			return EDataValidationResult::Invalid;
		}
		return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
	}
#endif
}

FPrimaryAssetId UGSTN_CosmeticThemeData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GSTNCosmeticAssetTypes::Theme(), GetFName());
}

FPrimaryAssetId UGSTN_CosmeticHostData::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GSTNCosmeticAssetTypes::Host(), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UGSTN_CosmeticThemeData::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);
	TArray<FString> Errors;
	TArray<FString> Warnings;
	FGSTN_CosmeticPlanning::ValidateTheme(*this, Errors, Warnings);
	return Report(Context, Result, Errors, Warnings);
}

EDataValidationResult UGSTN_CosmeticHostData::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);
	TArray<FString> Errors;
	TArray<FString> Warnings;
	// Default limits until a settings asset exists (phase 2, with the first real items)
	FGSTN_CosmeticPlanning::ValidateHost(*this, FGSTN_CosmeticLimits(), Errors, Warnings);
	return Report(Context, Result, Errors, Warnings);
}
#endif
