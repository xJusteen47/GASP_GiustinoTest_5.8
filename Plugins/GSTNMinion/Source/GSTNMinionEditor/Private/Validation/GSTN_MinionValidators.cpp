// Minion editor tooling (editor only; the runtime never depends on it).

#include "Validation/GSTN_MinionValidators.h"

#include "Data/GSTN_MinionDefinition.h"
#include "Data/GSTN_MinionValidation.h"
#include "Data/GSTN_MinionWaveDefinition.h"
#include "Misc/DataValidation.h"
#include "Store/GSTN_MinionStore.h"

namespace
{
	/** Adds the findings to the context and returns the verdict: Invalid when there is an error, Valid otherwise */
	EDataValidationResult Report(const TArray<FString>& Errors, const TArray<FString>& Warnings, FDataValidationContext& Context)
	{
		for (const FString& Warning : Warnings)
		{
			Context.AddWarning(FText::FromString(Warning));
		}
		for (const FString& Error : Errors)
		{
			Context.AddError(FText::FromString(Error));
		}
		return Errors.IsEmpty() ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
	}
}

bool UGSTN_MinionDefinitionValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	return Cast<UGSTN_MinionDefinition>(InAsset) != nullptr;
}

EDataValidationResult UGSTN_MinionDefinitionValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext)
{
	const UGSTN_MinionDefinition* Definition = Cast<UGSTN_MinionDefinition>(InAsset);
	if (!Definition)
	{
		return EDataValidationResult::NotValidated;
	}
	TArray<FString> Errors;
	TArray<FString> Warnings;
	GSTNMinionValidation::CheckDefinitionReferences(*Definition, Errors, Warnings);
	return Report(Errors, Warnings, InContext);
}

bool UGSTN_MinionWaveValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	return Cast<UGSTN_MinionWaveDefinition>(InAsset) != nullptr;
}

EDataValidationResult UGSTN_MinionWaveValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext)
{
	const UGSTN_MinionWaveDefinition* Wave = Cast<UGSTN_MinionWaveDefinition>(InAsset);
	if (!Wave)
	{
		return EDataValidationResult::NotValidated;
	}
	TArray<FString> Errors;
	GSTNMinionValidation::CheckWaveCrossAsset(*Wave, GSTNMinion::GetConfiguredCapacity(), Errors);
	return Report(Errors, TArray<FString>(), InContext);
}
