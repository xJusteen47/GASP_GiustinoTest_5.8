// Pure combat logic (no GAS, no Network Prediction).

#include "Loadout/GSTN_LoadoutRules.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	void AddError(FGSTN_LoadoutRuleResult& Result, const FString& Text)
	{
		Result.Errors.Add(FText::FromString(Text));
	}

	void AddWarning(FGSTN_LoadoutRuleResult& Result, const FString& Text)
	{
		Result.Warnings.Add(FText::FromString(Text));
	}
}

FGSTN_LoadoutRuleResult UGSTN_LoadoutRule::Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const
{
	return FGSTN_LoadoutRuleResult();
}

FGSTN_LoadoutRuleResult UGSTN_LoadoutRule_KitExists::Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const
{
	FGSTN_LoadoutRuleResult Result;
	if (!View.Loadout.PrimaryKit.IsValid())
	{
		AddError(Result, TEXT("The loadout names no kit."));
	}
	else if (!View.Kit.bFound)
	{
		AddError(Result, FString::Printf(TEXT("The kit %s does not exist."), *View.Loadout.PrimaryKit.ToString()));
	}
	return Result;
}

FGSTN_LoadoutRuleResult UGSTN_LoadoutRule_AdditionalWeapons::Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const
{
	FGSTN_LoadoutRuleResult Result;
	if (!View.Kit.bFound)
	{
		// Without the kit there is no query to judge by; KitExists reports it
		return Result;
	}

	const int32 Additional = View.Loadout.AdditionalWeapons.Num();
	if (Additional > View.Kit.MaxAdditionalWeapons)
	{
		AddError(Result, FString::Printf(TEXT("The loadout has %d additional weapons and the kit allows %d."), Additional, View.Kit.MaxAdditionalWeapons));
	}

	for (int32 Index = 0; Index < Additional; ++Index)
	{
		const FPrimaryAssetId& Id = View.Loadout.AdditionalWeapons[Index];
		// Weapons[0] is the kit's primary weapon, so additional weapon Index is Weapons[Index + 1]
		const FGSTN_LoadoutWeaponView* Weapon = View.Weapons.IsValidIndex(Index + 1) ? &View.Weapons[Index + 1] : nullptr;
		if (!Id.IsValid() || !Weapon || !Weapon->bFound)
		{
			AddError(Result, FString::Printf(TEXT("Additional weapon %d (%s) does not exist."), Index, *Id.ToString()));
		}
		else if (!View.Kit.AllowedAdditionalWeapons.Matches(Weapon->Tags))
		{
			AddError(Result, FString::Printf(TEXT("Additional weapon %d (%s) is not one the kit may carry."), Index, *Id.ToString()));
		}
	}
	return Result;
}

FGSTN_LoadoutRuleResult UGSTN_LoadoutRule_Supers::Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const
{
	FGSTN_LoadoutRuleResult Result;
	if (!View.Kit.bFound)
	{
		return Result;
	}

	for (int32 Slot = 0; Slot < View.Loadout.Supers.Num(); ++Slot)
	{
		const FPrimaryAssetId& Id = View.Loadout.Supers[Slot];
		if (!Id.IsValid())
		{
			// An empty slot is allowed
			continue;
		}
		if (Slot >= View.Kit.SuperSlotCount)
		{
			AddError(Result, FString::Printf(TEXT("Super %s is in slot %d and the kit has %d super slots."), *Id.ToString(), Slot, View.Kit.SuperSlotCount));
			continue;
		}
		const FGSTN_LoadoutSuperView* Super = View.Supers.IsValidIndex(Slot) ? &View.Supers[Slot] : nullptr;
		if (!Super || !Super->bFound)
		{
			AddError(Result, FString::Printf(TEXT("Super %s in slot %d does not exist."), *Id.ToString(), Slot));
		}
		else if (!View.Kit.AllowedSupers.Matches(Super->Tags))
		{
			AddError(Result, FString::Printf(TEXT("Super %s in slot %d is not one the kit may use."), *Id.ToString(), Slot));
		}
	}
	return Result;
}

FGSTN_LoadoutRuleResult UGSTN_LoadoutRule_Customisations::Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const
{
	FGSTN_LoadoutRuleResult Result;
	for (int32 Index = 0; Index < View.Loadout.SlotCustomisations.Num(); ++Index)
	{
		const FGSTN_SlotCustomisation& Customisation = View.Loadout.SlotCustomisations[Index];
		const FSoftObjectPath Alternative = Customisation.AlternativeAbility.ToSoftObjectPath();
		const FString Command = Customisation.Command.IsValid() ? Customisation.Command.ToString() : FString(TEXT("(no command)"));

		const FGSTN_LoadoutWeaponView* Weapon = View.Weapons.IsValidIndex(Customisation.WeaponIndex) ? &View.Weapons[Customisation.WeaponIndex] : nullptr;
		if (!Weapon || !Weapon->bFound)
		{
			AddError(Result, FString::Printf(TEXT("Customisation %d is for weapon %d, which the loadout does not have."), Index, Customisation.WeaponIndex));
			continue;
		}
		if (!Customisation.Command.IsValid() || Alternative.IsNull())
		{
			AddError(Result, FString::Printf(TEXT("Customisation %d names no command or no ability."), Index));
			continue;
		}

		const FGSTN_AllowedAlternativeView* Allowed = Weapon->AllowedAlternatives.FindByPredicate([&Customisation](const FGSTN_AllowedAlternativeView& Entry) { return Entry.Command == Customisation.Command; });
		if (!Allowed || !Allowed->Abilities.Contains(Alternative))
		{
			AddError(Result, FString::Printf(TEXT("Customisation %d swaps %s on weapon %d for %s, which that weapon does not allow."), Index, *Command, Customisation.WeaponIndex, *Alternative.ToString()));
		}
	}
	return Result;
}

FGSTN_LoadoutRuleResult UGSTN_LoadoutRule_Skin::Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const
{
	FGSTN_LoadoutRuleResult Result;
	if (!View.Kit.bFound || !View.Loadout.Skin.IsValid())
	{
		return Result;
	}
	if (!View.Skin.bFound)
	{
		AddError(Result, FString::Printf(TEXT("The skin %s does not exist."), *View.Loadout.Skin.ToString()));
	}
	else if (!View.Kit.CompatibleSkins.Matches(View.Skin.Tags))
	{
		AddError(Result, FString::Printf(TEXT("The skin %s does not fit the kit."), *View.Loadout.Skin.ToString()));
	}
	return Result;
}

FGSTN_LoadoutRuleResult UGSTN_LoadoutRule_NoAmbiguousBindings::Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const
{
	FGSTN_LoadoutRuleResult Result;
	for (const FString& Error : View.Table.Errors)
	{
		AddError(Result, Error);
	}
	for (const FString& Warning : View.Table.Warnings)
	{
		AddWarning(Result, Warning);
	}
	return Result;
}

FGSTN_LoadoutRuleResult UGSTN_LoadoutRule_TimelineRegistry::Evaluate_Implementation(const FGSTN_LoadoutRulesView& View) const
{
	FGSTN_LoadoutRuleResult Result;
	for (const FSoftObjectPath& Required : View.Table.RequiredTimelineAbilities)
	{
		if (!View.Table.RegistryAbilities.Contains(Required))
		{
			AddError(Result, FString::Printf(TEXT("The timeline ability %s can be triggered by the kit but is not in the timeline registry."), *Required.ToString()));
		}
	}
	return Result;
}

UGSTN_LoadoutRules::UGSTN_LoadoutRules()
{
	// The built-in rules, each its own object so the list can be edited per instance
	Rules.Add(CreateDefaultSubobject<UGSTN_LoadoutRule_KitExists>(TEXT("Rule_KitExists")));
	Rules.Add(CreateDefaultSubobject<UGSTN_LoadoutRule_AdditionalWeapons>(TEXT("Rule_AdditionalWeapons")));
	Rules.Add(CreateDefaultSubobject<UGSTN_LoadoutRule_Supers>(TEXT("Rule_Supers")));
	Rules.Add(CreateDefaultSubobject<UGSTN_LoadoutRule_Customisations>(TEXT("Rule_Customisations")));
	Rules.Add(CreateDefaultSubobject<UGSTN_LoadoutRule_Skin>(TEXT("Rule_Skin")));
	Rules.Add(CreateDefaultSubobject<UGSTN_LoadoutRule_NoAmbiguousBindings>(TEXT("Rule_NoAmbiguousBindings")));
	Rules.Add(CreateDefaultSubobject<UGSTN_LoadoutRule_TimelineRegistry>(TEXT("Rule_TimelineRegistry")));
}

FGSTN_LoadoutRuleResult UGSTN_LoadoutRules::Validate(const FGSTN_LoadoutRulesView& View) const
{
	FGSTN_LoadoutRuleResult Joined;
	for (const UGSTN_LoadoutRule* Rule : Rules)
	{
		if (!Rule)
		{
			continue;
		}
		const FGSTN_LoadoutRuleResult Found = Rule->Evaluate(View);
		Joined.Errors.Append(Found.Errors);
		Joined.Warnings.Append(Found.Warnings);
	}
	return Joined;
}

#if WITH_EDITOR
EDataValidationResult UGSTN_LoadoutRules::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	for (int32 Index = 0; Index < Rules.Num(); ++Index)
	{
		if (!Rules[Index])
		{
			Context.AddWarning(FText::FromString(FString::Printf(TEXT("Rule %d is empty and is skipped."), Index)));
		}
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
