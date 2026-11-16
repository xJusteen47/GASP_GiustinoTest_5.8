// Automation tests of the assembly that feeds the pure compiler and loadout rules from kit and weapon assets. The assets are
// built in memory; nothing is loaded from disk.

#include "Misc/AutomationTest.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbility_CharacterJump.h"
#include "Abilities/GameplayAbility_Montage.h"
#include "GSTN_CombatTags.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_StandardSlots.h"
#include "Kits/GSTN_CombatAssetTypes.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags LoadoutTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	TSoftClassPtr<UGameplayAbility> AbilityX() { return TSoftClassPtr<UGameplayAbility>(UGameplayAbility::StaticClass()); }
	TSoftClassPtr<UGameplayAbility> AbilityY() { return TSoftClassPtr<UGameplayAbility>(UGameplayAbility_CharacterJump::StaticClass()); }
	TSoftClassPtr<UGameplayAbility> AbilityZ() { return TSoftClassPtr<UGameplayAbility>(UGameplayAbility_Montage::StaticClass()); }

	FGSTN_AbilityBinding MakeLoadoutBinding(const FGameplayTag& Command, const TSoftClassPtr<UGameplayAbility>& Ability, const FGameplayTag& Require = FGameplayTag())
	{
		FGSTN_AbilityBinding Binding;
		Binding.Command = Command;
		Binding.Ability = Ability;
		if (Require.IsValid())
		{
			Binding.Requirements.RequireTags.AddTag(Require);
		}
		return Binding;
	}

	int32 CountLoadoutMessages(const TArray<FText>& Messages, const TCHAR* Needle)
	{
		int32 Count = 0;
		for (const FText& Message : Messages)
		{
			Count += Message.ToString().Contains(Needle) ? 1 : 0;
		}
		return Count;
	}

	const FGSTN_CompiledEntry* FindRow(const FGSTN_CompiledBindingTable& Table, EGSTN_EntrySource Source, int32 Index, const FGameplayTag& Command)
	{
		return Table.Entries.FindByPredicate([&](const FGSTN_CompiledEntry& Entry) { return Entry.Source == Source && Entry.SourceIndex == Index && Entry.Command == Command; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TableAssembleFromDefinitionsTest, "GSTNCombat.GAS.Table.AssembleFromDefinitions", LoadoutTestFlags)
bool FGSTN_TableAssembleFromDefinitionsTest::RunTest(const FString& Parameters)
{
	using namespace GSTNCommandTags;
	const UGSTN_InputSettings& Settings = UGSTN_InputSettings::Get();

	TStrongObjectPtr<UGSTN_InputSlotSet> Base(NewObject<UGSTN_InputSlotSet>(GetTransientPackage()));
	for (const FGSTN_InputSlotSpec& Spec : UGSTN_StandardSlots::Build())
	{
		Base->Slots.AddDefaulted_GetRef().Inline = Spec;
	}

	TStrongObjectPtr<UGSTN_PrimaryKitDefinition> Kit(NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage()));
	Kit->CommandSet.BaseSet = TSoftObjectPtr<UGSTN_InputSlotSet>(Base.Get());
	Kit->KitBindings = { MakeLoadoutBinding(Strong_Neutral.GetTag(), AbilityY()) };

	// A parent weapon and a child that replaces the parent's grounded attack and allows swapping it
	const FGameplayTag Grounded = GSTNCombatTags::State_Busy.GetTag();
	TStrongObjectPtr<UGSTN_WeaponDefinition> Parent(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));
	Parent->Bindings = { MakeLoadoutBinding(Attack_Neutral.GetTag(), AbilityX(), Grounded), MakeLoadoutBinding(Special_Neutral.GetTag(), AbilityX()) };
	TStrongObjectPtr<UGSTN_WeaponDefinition> Child(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));
	Child->ParentWeapon = TSoftObjectPtr<UGSTN_WeaponDefinition>(Parent.Get());
	Child->Bindings = { MakeLoadoutBinding(Attack_Neutral.GetTag(), AbilityY(), Grounded) };
	Child->AllowedAlternatives.Add(Attack_Neutral.GetTag()).Abilities = { AbilityX(), AbilityZ() };

	FGSTN_ResolvedLoadout Resolved;
	Resolved.Loadout.PrimaryKit = FPrimaryAssetId(GSTNCombatAssetTypes::PrimaryKit(), TEXT("KitA"));
	Resolved.Kit = Kit.Get();
	Resolved.Weapons = { Child.Get() };

	TStrongObjectPtr<UGSTN_LoadoutRules> Rules(NewObject<UGSTN_LoadoutRules>(GetTransientPackage()));
	FGSTN_CompiledBindingTable Table;
	FGSTN_LoadoutRuleResult Result;

	// The weapon's inherited bindings reach the table: the child's attack, the parent's special; the kit's own binding
	TestTrue(TEXT("+ a consistent loadout validates"), FGSTN_LoadoutAssembly::CompileAndValidate(Resolved, 3, Settings, *Rules, Table, Result));
	TestEqual(TEXT("the table carries the revision"), Table.Revision, 3);
	TestEqual(TEXT("three rows"), Table.Entries.Num(), 3);
	const FGSTN_CompiledEntry* AttackRow = FindRow(Table, EGSTN_EntrySource::Weapon, 0, Attack_Neutral.GetTag());
	const FGSTN_CompiledEntry* SpecialRow = FindRow(Table, EGSTN_EntrySource::Weapon, 0, Special_Neutral.GetTag());
	const FGSTN_CompiledEntry* StrongRow = FindRow(Table, EGSTN_EntrySource::Kit, 0, Strong_Neutral.GetTag());
	if (TestTrue(TEXT("every row is there"), AttackRow && SpecialRow && StrongRow))
	{
		TestTrue(TEXT("the child's binding replaced the parent's"), AttackRow->Ability == AbilityY().ToSoftObjectPath());
		TestTrue(TEXT("the parent's other binding is inherited"), SpecialRow->Ability == AbilityX().ToSoftObjectPath());
		TestTrue(TEXT("the kit's binding is a kit row"), StrongRow->Ability == AbilityY().ToSoftObjectPath());
	}
	TestEqual(TEXT("the abilities here carry no timeline tag, so the registry is empty"), Table.Registry.Num(), 0);

	// Customisation: an allowed alternative swaps the ability and validates; one the weapon does not list is rejected
	FGSTN_SlotCustomisation Customisation;
	Customisation.WeaponIndex = 0;
	Customisation.Command = Attack_Neutral.GetTag();
	Customisation.AlternativeAbility = TSoftClassPtr<UObject>(UGameplayAbility_Montage::StaticClass());
	Resolved.Loadout.SlotCustomisations = { Customisation };
	TestTrue(TEXT("+ an allowed alternative validates"), FGSTN_LoadoutAssembly::CompileAndValidate(Resolved, 3, Settings, *Rules, Table, Result));
	const FGSTN_CompiledEntry* Swapped = FindRow(Table, EGSTN_EntrySource::Weapon, 0, Attack_Neutral.GetTag());
	TestTrue(TEXT("and the row has the alternative"), Swapped && Swapped->Ability == AbilityZ().ToSoftObjectPath());

	Resolved.Loadout.SlotCustomisations[0].AlternativeAbility = TSoftClassPtr<UObject>(UGameplayAbility_CharacterJump::StaticClass());
	TestFalse(TEXT("- an alternative the weapon does not list is rejected"), FGSTN_LoadoutAssembly::CompileAndValidate(Resolved, 3, Settings, *Rules, Table, Result));
	TestEqual(TEXT("with one error"), Result.Errors.Num(), 1);
	Resolved.Loadout.SlotCustomisations.Reset();

	// A kit id that did not resolve
	FGSTN_ResolvedLoadout NoKit = Resolved;
	NoKit.Kit = nullptr;
	TestFalse(TEXT("- an unresolved kit is rejected"), FGSTN_LoadoutAssembly::CompileAndValidate(NoKit, 3, Settings, *Rules, Table, Result));
	TestEqual(TEXT("by the kit rule"), CountLoadoutMessages(Result.Errors, TEXT("does not exist")), 1);

	// Two kit bindings of one command that no exclusive group separates: the validator's ambiguity reaches the rules
	Kit->KitBindings = { MakeLoadoutBinding(Attack_Neutral.GetTag(), AbilityX(), GSTNCombatTags::State_Busy.GetTag()), MakeLoadoutBinding(Attack_Neutral.GetTag(), AbilityY(), GSTNCombatTags::State_Falling.GetTag()) };
	TestFalse(TEXT("- ambiguous bindings are rejected"), FGSTN_LoadoutAssembly::CompileAndValidate(Resolved, 3, Settings, *Rules, Table, Result));
	TestTrue(TEXT("with the validator's message"), CountLoadoutMessages(Result.Errors, TEXT("arbitrary")) >= 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
