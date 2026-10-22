// Combat vocabulary that needs the ability system (no Network Prediction, no Mover).

#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Abilities/GameplayAbility.h"
#include "GSTN_CombatTags.h"
#include "Kits/GSTN_AbilityBinding.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_SkinDefinition.h"
#include "Kits/GSTN_SuperDefinition.h"
#include "Kits/GSTN_SuperInputScheme.h"
#include "Kits/GSTN_WeaponDefinition.h"

namespace
{
	void AppendPaths(const TArray<TSoftClassPtr<UGameplayAbility>>& Abilities, TArray<FSoftObjectPath>& Out)
	{
		for (const TSoftClassPtr<UGameplayAbility>& Ability : Abilities)
		{
			Out.Add(Ability.ToSoftObjectPath());
		}
	}
}

FGSTN_BindingInput FGSTN_LoadoutAssembly::MakeBindingInput(const FGSTN_AbilityBinding& Binding)
{
	FGSTN_BindingInput Input;
	Input.Command = Binding.Command;
	Input.Requirements = Binding.Requirements;
	Input.bHasSlotOverride = Binding.HasSlotOverride();
	if (Input.bHasSlotOverride)
	{
		Input.SlotOverride = Binding.SlotOverride.GetSpec();
	}
	Input.Ability = Binding.Ability.ToSoftObjectPath();
	Input.TimelineOverride = Binding.TimelineOverride.ToSoftObjectPath();
	Input.PriorityOffset = Binding.PriorityOffset;
	Input.BufferFramesOverride = Binding.BufferFramesOverride;
	return Input;
}

bool FGSTN_LoadoutAssembly::IsTimelineAbilityClass(const FSoftObjectPath& Path)
{
	if (Path.IsNull())
	{
		return false;
	}
	const UClass* Class = TSoftClassPtr<UGameplayAbility>(Path).LoadSynchronous();
	const UGameplayAbility* Default = Class ? Class->GetDefaultObject<UGameplayAbility>() : nullptr;
	return Default && Default->GetAssetTags().HasTagExact(GSTNCombatTags::Ability_Timeline.GetTag());
}

void FGSTN_LoadoutAssembly::BuildCompileInput(const FGSTN_ResolvedLoadout& Resolved, int32 Revision, FGSTN_CompileInput& OutInput)
{
	OutInput = FGSTN_CompileInput();
	OutInput.Revision = Revision;
	OutInput.Customisations = Resolved.Loadout.SlotCustomisations;
	OutInput.SuperScheme = Resolved.SuperScheme;
	OutInput.IsTimelineAbility = &FGSTN_LoadoutAssembly::IsTimelineAbilityClass;

	if (const UGSTN_PrimaryKitDefinition* Kit = Resolved.Kit)
	{
		OutInput.Kit.CommandSet = Kit->CommandSet;
		OutInput.Kit.BaseSet = Kit->CommandSet.ResolveBaseSet();
		OutInput.Kit.SuperSlotCount = Kit->SuperSlotCount;
		for (const FGSTN_AbilityBinding& Binding : Kit->KitBindings)
		{
			OutInput.Kit.Bindings.Add(MakeBindingInput(Binding));
		}
		AppendPaths(Kit->TriggeredAbilities, OutInput.Kit.TriggeredAbilities);
		AppendPaths(Kit->Passives, OutInput.Kit.Passives);
	}

	// A weapon that did not resolve keeps its place (empty) so the loadout indices stay aligned
	OutInput.Weapons.SetNum(Resolved.Weapons.Num());
	for (int32 Index = 0; Index < Resolved.Weapons.Num(); ++Index)
	{
		const UGSTN_WeaponDefinition* Weapon = Resolved.Weapons[Index];
		if (!Weapon)
		{
			continue;
		}
		TArray<FGSTN_AbilityBinding> Effective;
		Weapon->GetEffectiveBindings(Effective);
		for (const FGSTN_AbilityBinding& Binding : Effective)
		{
			OutInput.Weapons[Index].Bindings.Add(MakeBindingInput(Binding));
		}
		AppendPaths(Weapon->PassivesWhileActive, OutInput.Weapons[Index].PassivesWhileActive);
	}

	OutInput.Supers.SetNum(Resolved.Supers.Num());
	for (int32 Slot = 0; Slot < Resolved.Supers.Num(); ++Slot)
	{
		if (const UGSTN_SuperDefinition* Super = Resolved.Supers[Slot])
		{
			FGSTN_SuperCompileInput& Out = OutInput.Supers[Slot];
			Out.bPresent = true;
			Out.Ability = Super->Ability.ToSoftObjectPath();
			Out.TimelineOverride = Super->TimelineOverride.ToSoftObjectPath();
			Out.InputCode = Super->InputCode;
			Out.Requirements = Super->Requirements;
		}
	}
}

void FGSTN_LoadoutAssembly::BuildRulesView(const FGSTN_ResolvedLoadout& Resolved, const FGSTN_CompiledBindingTable& Table, const FGSTN_CompileReport& Report, FGSTN_LoadoutRulesView& OutView)
{
	OutView = FGSTN_LoadoutRulesView();
	OutView.Loadout = Resolved.Loadout;

	if (const UGSTN_PrimaryKitDefinition* Kit = Resolved.Kit)
	{
		OutView.Kit.bFound = true;
		OutView.Kit.AllowedAdditionalWeapons = Kit->AllowedAdditionalWeapons;
		OutView.Kit.MaxAdditionalWeapons = Kit->MaxAdditionalWeapons;
		OutView.Kit.SuperSlotCount = Kit->SuperSlotCount;
		OutView.Kit.AllowedSupers = Kit->AllowedSupers;
		OutView.Kit.CompatibleSkins = Kit->CompatibleSkins;

		for (const TSoftClassPtr<UGameplayAbility>& Ability : Kit->TriggeredAbilities)
		{
			if (IsTimelineAbilityClass(Ability.ToSoftObjectPath()))
			{
				OutView.Table.RequiredTimelineAbilities.AddUnique(Ability.ToSoftObjectPath());
			}
		}
		for (const TSoftClassPtr<UGameplayAbility>& Ability : Kit->Passives)
		{
			if (IsTimelineAbilityClass(Ability.ToSoftObjectPath()))
			{
				OutView.Table.RequiredTimelineAbilities.AddUnique(Ability.ToSoftObjectPath());
			}
		}
	}

	OutView.Weapons.SetNum(Resolved.Weapons.Num());
	for (int32 Index = 0; Index < Resolved.Weapons.Num(); ++Index)
	{
		FGSTN_LoadoutWeaponView& Out = OutView.Weapons[Index];
		// Weapon 0 is the kit's primary weapon, named by the kit and not by an id of the loadout
		Out.Id = Index > 0 && Resolved.Loadout.AdditionalWeapons.IsValidIndex(Index - 1) ? Resolved.Loadout.AdditionalWeapons[Index - 1] : FPrimaryAssetId();
		if (const UGSTN_WeaponDefinition* Weapon = Resolved.Weapons[Index])
		{
			Out.bFound = true;
			Out.Tags = Weapon->WeaponTags;
			for (const TPair<FGameplayTag, FGSTN_AbilityClassList>& Pair : Weapon->AllowedAlternatives)
			{
				FGSTN_AllowedAlternativeView& Allowed = Out.AllowedAlternatives.AddDefaulted_GetRef();
				Allowed.Command = Pair.Key;
				AppendPaths(Pair.Value.Abilities, Allowed.Abilities);
			}
		}
	}

	OutView.Supers.SetNum(Resolved.Supers.Num());
	for (int32 Slot = 0; Slot < Resolved.Supers.Num(); ++Slot)
	{
		OutView.Supers[Slot].Id = Resolved.Loadout.Supers.IsValidIndex(Slot) ? Resolved.Loadout.Supers[Slot] : FPrimaryAssetId();
		if (const UGSTN_SuperDefinition* Super = Resolved.Supers[Slot])
		{
			OutView.Supers[Slot].bFound = true;
			OutView.Supers[Slot].Tags = Super->SuperTags;
		}
	}

	if (Resolved.Skin)
	{
		OutView.Skin.bFound = true;
		OutView.Skin.Tags = Resolved.Skin->SkinTags;
	}

	for (const FText& Error : Report.Errors)
	{
		OutView.Table.Errors.Add(Error.ToString());
	}
	for (const FText& Warning : Report.Warnings)
	{
		OutView.Table.Warnings.Add(Warning.ToString());
	}
	for (const FGSTN_TimelineRegistryEntry& Entry : Table.Registry)
	{
		OutView.Table.RegistryAbilities.AddUnique(Entry.Ability);
	}
}

bool FGSTN_LoadoutAssembly::CompileAndValidate(const FGSTN_ResolvedLoadout& Resolved, int32 Revision, const UGSTN_InputSettings& Settings, const UGSTN_LoadoutRules& Rules,
	FGSTN_CompiledBindingTable& OutTable, FGSTN_LoadoutRuleResult& OutResult)
{
	FGSTN_CompileInput Input;
	BuildCompileInput(Resolved, Revision, Input);

	FGSTN_CompileReport Report;
	OutTable = FGSTN_BindingTableCompiler::Compile(Input, Settings, &Report);
	FGSTN_BindingTableCompiler::FindConflicts(OutTable, Settings, Report);

	FGSTN_LoadoutRulesView View;
	BuildRulesView(Resolved, OutTable, Report, View);
	OutResult = Rules.Validate(View);
	return OutResult.IsValid();
}
