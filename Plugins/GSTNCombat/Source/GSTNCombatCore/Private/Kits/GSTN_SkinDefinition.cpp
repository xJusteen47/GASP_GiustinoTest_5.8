// Pure combat logic (no GAS, no Network Prediction).

#include "Kits/GSTN_SkinDefinition.h"
#include "Kits/GSTN_CombatAssetTypes.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FPrimaryAssetId UGSTN_SkinDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GSTNCombatAssetTypes::Skin(), GetFName());
}

bool UGSTN_SkinDefinition::ChangesAnything() const
{
	if (!Mesh.IsNull() || !AnimClass.IsNull())
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
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
