// Editor tooling of GSTN Combat (editor only; the runtime never depends on it).

#pragma once

#include "CoreMinimal.h"
#include "EditorValidatorBase.h"
#include "Validation/GSTN_ValidationReport.h"
struct FGSTN_LoadoutData;

#include "GSTN_EditorValidators.generated.h"

/**
 * Data Validation validators of the combat data (combat-m5-editor 9.1). Data Validation finds them by class on editor start and runs
 * them on save, on demand and in CI (-run=DataValidation); they hold no rule of their own, they run the rules of
 * FGSTN_CombatValidators on the asset and hand the findings to the validation context.
 *
 * Data Validation calls the asset's own IsDataValid first and stops there when it says invalid, then runs the validators. The assets
 * already report what they check themselves (a timeline's Core rules, a kit's own checks), so a finding whose text is already in the
 * context is not added a second time. What a validator adds on top is what no asset could check alone: the rules that need the editor
 * (the track registry), a loaded montage at the project's tick rate, and the command set of a kit with its weapons.
 *
 * Errors invalidate the asset, warnings do not.
 */
UCLASS(Abstract)
class UGSTN_CombatValidatorBase : public UEditorValidatorBase
{
	GENERATED_BODY()

public:
	/**
	 * Adds the report to the context (new findings only) and returns the verdict: Invalid when the report has an error, Valid otherwise.
	 * Public and static so the Validate button and the tests can reuse the mapping.
	 */
	static EDataValidationResult AddReportToContext(const FGSTN_ValidationReport& Report, const FAssetData& AssetData, FDataValidationContext& Context);
};

/** A UGSTN_CombatTimeline asset: the timeline rules at the project's tick rate, with the montage loaded */
UCLASS()
class UGSTN_TimelineValidator : public UGSTN_CombatValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) override;
};

/** A kit: its own checks, and the conflicts of its command set (Ambiguous, DuplicateCommand, InvalidSlot errors, Shadowed warnings) */
UCLASS()
class UGSTN_KitValidator : public UGSTN_CombatValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) override;
};

/** A weapon: its own checks, and what it adds to the command table of each kit that can carry it (found through the Asset Registry) */
UCLASS()
class UGSTN_WeaponValidator : public UGSTN_CombatValidatorBase
{
	GENERATED_BODY()

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) override;
};

/**
 * A loadout preset. M3 has no preset asset class yet (combat.md names UGSTN_LoadoutDefinition, designer presets), so this validator takes
 * any asset whose class has an FGSTN_LoadoutData property, which is that class when it arrives and any game's own Blueprint holding one.
 * It checks each such property with the loadout rules and the command table of the whole loadout, and names the weapon or super
 * that introduces a conflicting slot.
 */
UCLASS()
class UGSTN_LoadoutPresetValidator : public UGSTN_CombatValidatorBase
{
	GENERATED_BODY()

public:
	/** The FGSTN_LoadoutData properties of the object's class, with their values */
	static void GatherLoadouts(const UObject& Object, TArray<TPair<FName, const FGSTN_LoadoutData*>>& OutLoadouts);

protected:
	virtual bool CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) const override;
	virtual EDataValidationResult ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& InContext) override;
};
