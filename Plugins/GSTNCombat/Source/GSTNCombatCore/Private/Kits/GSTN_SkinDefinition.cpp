// Pure combat logic (no GAS, no Network Prediction).

#include "Kits/GSTN_SkinDefinition.h"
#include "Kits/GSTN_CombatAssetTypes.h"

#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "Kits/GSTN_SkinProportions.h"
#include "Misc/DataValidation.h"
#endif

FPrimaryAssetId UGSTN_SkinDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GSTNCombatAssetTypes::Skin(), GetFName());
}

bool UGSTN_SkinDefinition::ChangesAnything() const
{
	if (!Mesh.IsNull() || !AnimClass.IsNull() || Footwear.IsValid())
	{
		return true;
	}
	return Materials.ContainsByPredicate([](const TSoftObjectPtr<UMaterialInterface>& Material) { return !Material.IsNull(); });
}

#if WITH_EDITOR
EDataValidationResult UGSTN_SkinDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (!ChangesAnything())
	{
		Context.AddWarning(FText::FromString(TEXT("The skin sets no mesh, material or animation class, so applying it changes nothing.")));
	}

	// Proportions against the game skeleton's body (owner limits 2026-11-23). Editor validation only: loading two meshes is fine here
	if (!Mesh.IsNull())
	{
		const UGSTN_SkinProportionSettings& Settings = UGSTN_SkinProportionSettings::Get();
		const USkeletalMesh* Base = Settings.BaseMesh.IsNull() ? nullptr : Settings.BaseMesh.LoadSynchronous();
		const USkeletalMesh* Own = Mesh.LoadSynchronous();
		if (!Base)
		{
			Context.AddWarning(FText::FromString(TEXT("Proportions not checked: Project Settings > GSTN Skins > Base Mesh is empty or does not load.")));
		}
		else if (!Own)
		{
			Context.AddError(FText::FromString(TEXT("The skin's mesh does not load.")));
			Result = EDataValidationResult::Invalid;
		}
		else if (Own != Base)
		{
			FGSTN_SkinProportions Ratios;
			TArray<FString> Errors;
			TArray<FString> Unmeasured;
			FGSTN_SkinProportionUtils::CheckMeshes(*Base, *Own, Settings.Limits, Settings.Bones, Ratios, Errors, Unmeasured);
			for (const FString& Error : Errors)
			{
				Context.AddError(FText::FromString(FString::Printf(TEXT("Proportions: %s"), *Error)));
				Result = EDataValidationResult::Invalid;
			}
			if (Unmeasured.Num() > 0)
			{
				Context.AddWarning(FText::FromString(FString::Printf(TEXT("Proportions not measured (bone missing): %s"), *FString::Join(Unmeasured, TEXT(", ")))));
			}
		}
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
