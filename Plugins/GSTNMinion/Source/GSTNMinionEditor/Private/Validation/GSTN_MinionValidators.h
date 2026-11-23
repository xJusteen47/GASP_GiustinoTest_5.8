// Minion editor tooling (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "GSTN_MinionValidators.generated.h"

/**
 * Data Validation validators of the minion data (M8 G12). Data Validation finds them by class when the editor starts and runs them on save,
 * on demand and in CI (-run=DataValidation). They hold no rule of their own: the rules are GSTNMinionValidation:: in GSTNMinionCore,
 * which the automation tests run headless. The asset's own IsDataValid runs first (its texts are not repeated here); a validator adds only
 * the checks that need a second asset or a loaded reference. Errors invalidate the asset, warnings do not.
 */
UCLASS()
class UGSTN_MinionDefinitionValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) override;
};

/** A wave: the total against the store capacity, unresolved definition paths and the registry rules over the types it names */
UCLASS()
class UGSTN_MinionWaveValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) override;
};
