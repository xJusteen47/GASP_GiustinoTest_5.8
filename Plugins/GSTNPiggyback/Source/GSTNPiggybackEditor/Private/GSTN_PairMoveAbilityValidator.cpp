// Piggyback, editor part (editor only; the runtime never depends on it).

#include "GSTN_PairMoveAbilityValidator.h"

#include "Engine/Blueprint.h"
#include "GSTN_PairMoveDefinition.h"
#include "Misc/DataValidation.h"

namespace
{
	const UClass* PairMoveAbilityClassOf(const UObject* Asset)
	{
		const UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
		const UClass* Generated = Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
		return Generated && Generated->IsChildOf(UGSTN_GA_PairMove::StaticClass()) ? Generated : nullptr;
	}
}

void UGSTN_PairMoveAbilityValidator::CheckPairMoveAbility(const UClass* AbilityClass, const UGSTN_PairMoveDefinition* Definition, TArray<FString>& OutErrors,
	TArray<FString>& OutWarnings)
{
	if (!AbilityClass)
	{
		return;
	}
	if (!Definition)
	{
		OutWarnings.Add(FString::Printf(TEXT("%s names no pair move definition (PairMove): nothing checks that its timeline agrees with a pair move."),
			*AbilityClass->GetName()));
		return;
	}
	const UClass* Initiator = Definition->InitiatorAbility.Get();
	if (!Initiator || !AbilityClass->IsChildOf(Initiator))
	{
		OutErrors.Add(FString::Printf(TEXT("%s names the pair move %s, whose initiator ability is %s: the pair move plays that ability, not this one."),
			*AbilityClass->GetName(), *Definition->GetName(), Initiator ? *Initiator->GetName() : TEXT("(none)")));
	}
	TArray<FString> DefinitionErrors;
	Definition->Validate(DefinitionErrors);
	for (const FString& Error : DefinitionErrors)
	{
		OutErrors.Add(FString::Printf(TEXT("Pair move %s: %s"), *Definition->GetName(), *Error));
	}
}

bool UGSTN_PairMoveAbilityValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const
{
	return PairMoveAbilityClassOf(InAsset) != nullptr;
}

EDataValidationResult UGSTN_PairMoveAbilityValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext)
{
	const UClass* AbilityClass = PairMoveAbilityClassOf(InAsset);
	if (!AbilityClass)
	{
		return EDataValidationResult::NotValidated;
	}
	const UGSTN_GA_PairMove* Defaults = GetDefault<UGSTN_GA_PairMove>(const_cast<UClass*>(AbilityClass));
	TArray<FString> Errors;
	TArray<FString> Warnings;
	CheckPairMoveAbility(AbilityClass, Defaults ? Defaults->PairMove.Get() : nullptr, Errors, Warnings);
	for (const FString& Warning : Warnings)
	{
		AssetWarning(InAsset, FText::FromString(Warning));
	}
	for (const FString& Error : Errors)
	{
		AssetFails(InAsset, FText::FromString(Error));
	}
	if (Errors.Num() == 0)
	{
		AssetPasses(InAsset);
		return EDataValidationResult::Valid;
	}
	return EDataValidationResult::Invalid;
}
