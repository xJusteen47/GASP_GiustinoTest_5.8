// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Kits/GSTN_WeaponDefinition.h"
#include "Abilities/GameplayAbility.h"
#include "GSTN_CombatTags.h"
#include "Kits/GSTN_CombatAssetTypes.h"
#include "Kits/GSTN_DesignerIdentity.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FPrimaryAssetId UGSTN_WeaponDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GSTNCombatAssetTypes::Weapon(), GetFName());
}

void UGSTN_WeaponDefinition::GetParentChain(TArray<const UGSTN_WeaponDefinition*>& OutChain, bool& bOutBroken) const
{
	OutChain.Reset();
	bOutBroken = false;

	// Walked leaf to root, then reversed
	TArray<const UGSTN_WeaponDefinition*> Walk;
	const UGSTN_WeaponDefinition* Current = this;
	while (Current)
	{
		if (Walk.Contains(Current) || Walk.Num() >= MaxParentDepth)
		{
			bOutBroken = true;
			break;
		}
		Walk.Add(Current);
		if (Current->ParentWeapon.IsNull())
		{
			break;
		}
		Current = Current->ParentWeapon.LoadSynchronous();
		if (!Current)
		{
			bOutBroken = true;
		}
	}

	for (int32 Index = Walk.Num() - 1; Index >= 0; --Index)
	{
		OutChain.Add(Walk[Index]);
	}
}

void UGSTN_WeaponDefinition::GetEffectiveBindings(TArray<FGSTN_AbilityBinding>& OutBindings) const
{
	TArray<const UGSTN_WeaponDefinition*> Chain;
	bool bBroken = false;
	GetParentChain(Chain, bBroken);

	OutBindings.Reset();
	for (const UGSTN_WeaponDefinition* Weapon : Chain)
	{
		OutBindings = UGSTN_AbilityBindingUtils::InheritBindings(OutBindings, Weapon->Bindings);
	}
}

void UGSTN_WeaponDefinition::Validate(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	auto AddError = [&OutErrors](const FString& Text) { OutErrors.Add(FText::FromString(Text)); };
	auto AddWarning = [&OutWarnings](const FString& Text) { OutWarnings.Add(FText::FromString(Text)); };

	FGSTN_DesignerIdentity::ValidateExactlyOne(WeaponTags, GSTNCombatTags::Root_Weapon.GetTag(), TEXT("weapon"), OutErrors);

	TArray<const UGSTN_WeaponDefinition*> Chain;
	bool bBroken = false;
	GetParentChain(Chain, bBroken);
	if (bBroken)
	{
		AddError(FString::Printf(TEXT("The parent chain loops, is deeper than %d weapons or names a parent that cannot be loaded."), MaxParentDepth));
	}

	// The weapon's commands belong to the kit's command set, which is only known once a loadout is compiled: here the list
	// is checked on its own
	UGSTN_AbilityBindingUtils::Validate(Bindings, FGSTN_BindingValidationContext(), TEXT("Weapon"), OutErrors, OutWarnings);

	// One stats effect class per weapon: it is removed by class, so the class identifies the weapon's stats
	FGSTN_StatValue::Validate(StatModifiers, OutErrors);
	if (StatsEffect.IsNull() && !StatModifiers.IsEmpty())
	{
		AddError(TEXT("The weapon has stat modifiers but no stats effect, so they would never apply."));
	}
	else if (!StatsEffect.IsNull() && StatModifiers.IsEmpty())
	{
		AddWarning(TEXT("The weapon names a stats effect but no stat modifiers."));
	}

	for (const TPair<FGameplayTag, FGSTN_AbilityClassList>& Pair : AllowedAlternatives)
	{
		if (!Pair.Key.IsValid())
		{
			AddError(TEXT("AllowedAlternatives has an entry without a command."));
		}
		else if (Pair.Value.Abilities.IsEmpty())
		{
			AddWarning(FString::Printf(TEXT("AllowedAlternatives lists no ability for %s."), *Pair.Key.ToString()));
		}
	}

	if (PassivesWhileActive.ContainsByPredicate([](const TSoftClassPtr<UGameplayAbility>& Ability) { return Ability.IsNull(); }))
	{
		AddWarning(TEXT("PassivesWhileActive has an empty entry."));
	}
	if (bHasRicochetFallback && !RicochetFallback.IsAuthored())
	{
		AddWarning(TEXT("The ricochet fallback is on but has no recoil frames, so it does nothing."));
	}
}

#if WITH_EDITOR
namespace
{
	FName GetWeaponIdentity(const UGSTN_WeaponDefinition& Weapon)
	{
		TArray<FGameplayTag> Leaves;
		FGSTN_DesignerIdentity::GetIdentityLeaves(Weapon.WeaponTags, GSTNCombatTags::Root_Weapon.GetTag(), Leaves);
		return Leaves.Num() == 1 ? Leaves[0].GetTagName() : NAME_None;
	}

	FName GetStatsEffectKey(const UGSTN_WeaponDefinition& Weapon)
	{
		return Weapon.StatsEffect.IsNull() ? NAME_None : FName(*Weapon.StatsEffect.ToString());
	}
}

EDataValidationResult UGSTN_WeaponDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FText> Errors;
	TArray<FText> Warnings;
	Validate(Errors, Warnings);

	// Identity and stats effect class are both unique per weapon: compare with every registered weapon (nothing is compared
	// until the project registers the primary asset type)
	TArray<FGSTN_UniqueKeyEntry> Identities;
	Identities.Add({ GetFName(), GetWeaponIdentity(*this) });
	FGSTN_DesignerIdentity::GatherFromPrimaryAssets(GSTNCombatAssetTypes::Weapon(), *this, [](const UObject& Other)
	{
		const UGSTN_WeaponDefinition* Weapon = Cast<UGSTN_WeaponDefinition>(&Other);
		return Weapon ? GetWeaponIdentity(*Weapon) : NAME_None;
	}, Identities);
	FGSTN_DesignerIdentity::FindDuplicateKeys(Identities, TEXT("Weapon identity"), Errors);

	TArray<FGSTN_UniqueKeyEntry> StatsEffects;
	StatsEffects.Add({ GetFName(), GetStatsEffectKey(*this) });
	FGSTN_DesignerIdentity::GatherFromPrimaryAssets(GSTNCombatAssetTypes::Weapon(), *this, [](const UObject& Other)
	{
		const UGSTN_WeaponDefinition* Weapon = Cast<UGSTN_WeaponDefinition>(&Other);
		return Weapon ? GetStatsEffectKey(*Weapon) : NAME_None;
	}, StatsEffects);
	FGSTN_DesignerIdentity::FindDuplicateKeys(StatsEffects, TEXT("Weapon stats effect"), Errors);

	for (const FText& Error : Errors)
	{
		Context.AddError(Error);
		Result = EDataValidationResult::Invalid;
	}
	for (const FText& Warning : Warnings)
	{
		Context.AddWarning(Warning);
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
