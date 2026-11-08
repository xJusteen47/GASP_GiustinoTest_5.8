#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "GSTN_PartSetValidator.generated.h"

/**
 * Editor validator for part sets and penetration tables (plan P13). The assets' IsDataValid already hold the rules
 * (FGSTN_PartsValidation); this wrapper runs them through the validation subsystem (Validate Assets, save, commandlets) and
 * prefixes every message with the asset path, so a finding in a list of hundreds of assets names its asset. The engine also
 * calls the asset's own IsDataValid, so the unprefixed message appears there as well.
 *
 * A set flagged bForMinions also gets the minion rules (plan P15, FGSTN_PartsValidation::ValidateForMinions): at most 8 parts and no
 * Stats break effect.
 */
UCLASS()
class GSTNPARTSEDITOR_API UGSTN_PartSetValidator : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context) override;

	/**
	 * Validates a part set or a penetration table and appends every finding to Out as "[AssetPath] message". KnownBones, when
	 * given, replaces the preview mesh as the source of bone names for a part set (a Physics Asset or a skeleton the caller has).
	 * Anything else is NotValidated.
	 */
	static EDataValidationResult ValidateAsset(const UObject* Asset, FDataValidationContext& Out, const TSet<FName>* KnownBones = nullptr);
};
