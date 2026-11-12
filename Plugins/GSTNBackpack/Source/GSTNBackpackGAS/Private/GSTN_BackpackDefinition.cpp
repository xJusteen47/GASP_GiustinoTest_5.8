// Backpacks, ability layer: the backpack asset.

#include "GSTN_BackpackDefinition.h"
#include "GameplayEffect.h"
#include "GSTN_BackpackTypes.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FPrimaryAssetType UGSTN_BackpackDefinition::AssetType(TEXT("GSTNBackpack"));

FPrimaryAssetId UGSTN_BackpackDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UGSTN_BackpackDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	int32 Identities = 0;
	for (const FGameplayTag& Tag : BackpackTags)
	{
		Identities += Tag.MatchesTag(GSTNBackpackTags::Backpack) ? 1 : 0;
	}
	if (Identities != 1)
	{
		Context.AddError(FText::FromString(TEXT("BackpackTags needs exactly one identity tag under Backpack")));
		Result = EDataValidationResult::Invalid;
	}
	for (const FGSTN_AbilityBinding& Binding : Abilities)
	{
		if (!Binding.Command.IsValid() || Binding.Ability.IsNull())
		{
			Context.AddWarning(FText::FromString(TEXT("A backpack binding without a command or an ability does nothing")));
		}
	}
	TArray<FText> Warnings;
	FGSTN_EquipmentVisualUtils::Validate(Visual, GetName(), Warnings);
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	return Result;
}
#endif
