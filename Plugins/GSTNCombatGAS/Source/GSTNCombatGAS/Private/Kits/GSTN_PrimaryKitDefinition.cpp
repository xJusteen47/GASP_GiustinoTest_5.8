// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Abilities/GameplayAbility.h"
#include "GSTN_CombatTags.h"
#include "Kits/GSTN_CombatAssetTypes.h"
#include "Kits/GSTN_DesignerIdentity.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FPrimaryAssetId UGSTN_PrimaryKitDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(GSTNCombatAssetTypes::PrimaryKit(), GetFName());
}

void UGSTN_PrimaryKitDefinition::Validate(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const
{
	auto AddError = [&OutErrors](const FString& Text) { OutErrors.Add(FText::FromString(Text)); };
	auto AddWarning = [&OutWarnings](const FString& Text) { OutWarnings.Add(FText::FromString(Text)); };

	FGSTN_DesignerIdentity::ValidateExactlyOne(KitTags, GSTNCombatTags::Root_Kit.GetTag(), TEXT("kit"), OutErrors);

	if (PrimaryWeapon.IsNull())
	{
		AddError(TEXT("A kit needs a primary weapon."));
	}

	// Stats and meters
	Stats.Validate(OutErrors);
	if (!Stats.Values.IsEmpty() && InitStatsEffect.IsNull())
	{
		AddError(TEXT("The kit has stats but no init stats effect, so they would never reach the character."));
	}
	TSet<FGameplayTag> MeterTags;
	for (int32 Index = 0; Index < Meters.Num(); ++Index)
	{
		if (!Meters[Index].IsValid())
		{
			AddError(FString::Printf(TEXT("Meter %d needs a meter tag and two different attributes."), Index));
		}
		else if (MeterTags.Contains(Meters[Index].Meter))
		{
			AddError(FString::Printf(TEXT("Meter %d repeats the meter %s."), Index, *Meters[Index].Meter.ToString()));
		}
		MeterTags.Add(Meters[Index].Meter);
	}

	// Supers
	if (SuperSlotCount < 0 || SuperSlotCount > GSTNCombatAssetTypes::MaxSuperSlots)
	{
		AddError(FString::Printf(TEXT("A kit has 0 to %d super slots and this one has %d."), GSTNCombatAssetTypes::MaxSuperSlots, SuperSlotCount));
	}
	if (SuperSlotCount > 0 && SuperInputScheme.IsNull())
	{
		AddError(TEXT("The kit has super slots but no super input scheme."));
	}
	else if (SuperSlotCount == 0 && !SuperInputScheme.IsNull())
	{
		AddWarning(TEXT("The kit names a super input scheme but has no super slot."));
	}

	// Command set. The base set is loaded here (never in the simulation); without one the set is only what the kit adds.
	const UGSTN_InputSlotSet* Base = CommandSet.ResolveBaseSet();
	if (!Base)
	{
		if (CommandSet.BaseSet.IsNull())
		{
			AddWarning(TEXT("The command set has no base set and the combat settings name no default command set: the kit understands only the slots it adds."));
		}
		else
		{
			AddError(FString::Printf(TEXT("The base command set %s cannot be loaded."), *CommandSet.BaseSet.ToString()));
		}
	}

	TSet<FGameplayTag> AddedCommands;
	for (int32 Index = 0; Index < CommandSet.AddedSlots.Num(); ++Index)
	{
		const FGameplayTag Command = CommandSet.AddedSlots[Index].GetSpec().Command;
		if (!Command.IsValid())
		{
			AddError(FString::Printf(TEXT("Added slot %d has no command."), Index));
			continue;
		}
		if (AddedCommands.Contains(Command))
		{
			AddError(FString::Printf(TEXT("Added slot %d repeats the command %s."), Index, *Command.ToString()));
		}
		AddedCommands.Add(Command);
		if (CommandSet.RemovedCommands.HasTagExact(Command))
		{
			AddError(FString::Printf(TEXT("The command set both removes and adds %s."), *Command.ToString()));
		}
	}
	if (Base)
	{
		TSet<FGameplayTag> BaseCommands;
		for (const FGSTN_InputSlotEntry& Entry : Base->Slots)
		{
			BaseCommands.Add(Entry.GetSpec().Command);
		}
		for (const FGameplayTag& Removed : CommandSet.RemovedCommands)
		{
			if (!BaseCommands.Contains(Removed))
			{
				AddWarning(FString::Printf(TEXT("The command set removes %s, which the base set does not have."), *Removed.ToString()));
			}
		}
	}

	FGSTN_BindingValidationContext BindingContext;
	BindingContext.CommandSet = &CommandSet;
	BindingContext.BaseSet = Base;
	BindingContext.bCheckMembership = Base != nullptr;
	UGSTN_AbilityBindingUtils::Validate(KitBindings, BindingContext, TEXT("Kit"), OutErrors, OutWarnings);

	// Granted abilities
	if (TriggeredAbilities.ContainsByPredicate([](const TSoftClassPtr<UGameplayAbility>& Ability) { return Ability.IsNull(); }))
	{
		AddWarning(TEXT("TriggeredAbilities has an empty entry."));
	}
	if (Passives.ContainsByPredicate([](const TSoftClassPtr<UGameplayAbility>& Ability) { return Ability.IsNull(); }))
	{
		AddWarning(TEXT("Passives has an empty entry."));
	}

	if (bHasRicochetFallback && !RicochetFallback.IsAuthored())
	{
		AddWarning(TEXT("The ricochet fallback is on but has no recoil frames, so it does nothing."));
	}
}

#if WITH_EDITOR
EDataValidationResult UGSTN_PrimaryKitDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<FText> Errors;
	TArray<FText> Warnings;
	Validate(Errors, Warnings);

	// The identity must be unique among kits: compare with every registered kit. Nothing is compared until the project
	// registers the primary asset type.
	TArray<FGSTN_UniqueKeyEntry> Entries;
	Entries.Add({ GetFName(), NAME_None });
	TArray<FGameplayTag> Leaves;
	FGSTN_DesignerIdentity::GetIdentityLeaves(KitTags, GSTNCombatTags::Root_Kit.GetTag(), Leaves);
	Entries[0].Key = Leaves.Num() == 1 ? Leaves[0].GetTagName() : NAME_None;
	FGSTN_DesignerIdentity::GatherFromPrimaryAssets(GSTNCombatAssetTypes::PrimaryKit(), *this, [](const UObject& Other)
	{
		const UGSTN_PrimaryKitDefinition* Kit = Cast<UGSTN_PrimaryKitDefinition>(&Other);
		TArray<FGameplayTag> OtherLeaves;
		if (Kit)
		{
			FGSTN_DesignerIdentity::GetIdentityLeaves(Kit->KitTags, GSTNCombatTags::Root_Kit.GetTag(), OtherLeaves);
		}
		return OtherLeaves.Num() == 1 ? OtherLeaves[0].GetTagName() : NAME_None;
	}, Entries);
	FGSTN_DesignerIdentity::FindDuplicateKeys(Entries, TEXT("Kit identity"), Errors);

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
