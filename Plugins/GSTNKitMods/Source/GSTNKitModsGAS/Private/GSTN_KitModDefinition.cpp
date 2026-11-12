// Kit mods, ability layer: the mod asset.

#include "GSTN_KitModDefinition.h"
#include "GameplayEffect.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FPrimaryAssetType UGSTN_KitModDefinition::AssetType(TEXT("GSTNKitMod"));

FPrimaryAssetId UGSTN_KitModDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(AssetType, GetFName());
}

#if WITH_EDITOR
EDataValidationResult UGSTN_KitModDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	int32 Identities = 0;
	for (const FGameplayTag& Tag : ModTags)
	{
		Identities += Tag.MatchesTag(GSTNKitModsTags::Mod) && !Tag.MatchesTag(GSTNKitModsTags::Mod_Slot) ? 1 : 0;
	}
	if (Identities != 1)
	{
		Context.AddError(FText::FromString(TEXT("ModTags needs exactly one identity tag under Mod (not Mod.Slot)")));
		Result = EDataValidationResult::Invalid;
	}
	if (!Slot.IsValid() || !Slot.MatchesTag(GSTNKitModsTags::Mod_Slot))
	{
		Context.AddError(FText::FromString(TEXT("Slot must be a tag under Mod.Slot")));
		Result = EDataValidationResult::Invalid;
	}
	if (!StatsMagnitudes.IsEmpty() && !StatsEffect)
	{
		Context.AddWarning(FText::FromString(TEXT("StatsMagnitudes without a StatsEffect do nothing")));
	}
	for (const FGSTN_MoveModifier& Modifier : MoveModifiers)
	{
		if (Modifier.TimeScale == 1.f && Modifier.ReachScale == 1.f && Modifier.HitboxScale.Equals(FVector::OneVector, 0.0) && Modifier.DamageScale == 1.f
			&& Modifier.KnockbackScale == 1.f && Modifier.StaggerForceScale == 1.f && Modifier.HitstopScale == 1.f && Modifier.AddedTags.IsEmpty())
		{
			Context.AddWarning(FText::FromString(TEXT("A move modifier changes nothing")));
		}
	}
	return Result;
}
#endif
