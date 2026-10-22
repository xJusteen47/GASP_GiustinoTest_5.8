#include "Misc/AutomationTest.h"
#include "Abilities/GameplayAbility.h"
#include "GSTN_CombatTags.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Kits/GSTN_AbilityBinding.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Tests/GSTN_DataTestTypes.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	FGSTN_AbilityBinding MakeBinding(const FGameplayTag& Command, const FGameplayTag& Require, int32 PriorityOffset)
	{
		FGSTN_AbilityBinding Binding;
		Binding.Command = Command;
		if (Require.IsValid())
		{
			Binding.Requirements.RequireTags.AddTag(Require);
		}
		Binding.PriorityOffset = PriorityOffset;
		return Binding;
	}

	int32 CountContaining(const TArray<FText>& Messages, const TCHAR* Needle)
	{
		int32 Count = 0;
		for (const FText& Message : Messages)
		{
			Count += Message.ToString().Contains(Needle) ? 1 : 0;
		}
		return Count;
	}

	FGSTN_InputSlotEntry MakeInlineSlot(const FGameplayTag& Command)
	{
		FGSTN_InputSlotEntry Entry;
		Entry.Inline.Command = Command;
		return Entry;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DataBindingInheritanceTest, "GSTNCombat.GAS.Data.BindingInheritance", TestFlags)
bool FGSTN_DataBindingInheritanceTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Grounded = GSTNCombatTags::State_Busy.GetTag();
	const FGameplayTag Airborne = GSTNCombatTags::State_Falling.GetTag();
	const FGameplayTag Jab = GSTNCommandTags::Attack_Neutral.GetTag();
	const FGameplayTag Special = GSTNCommandTags::Special_Neutral.GetTag();
	const FGameplayTag Strong = GSTNCommandTags::Strong_Neutral.GetTag();

	const TArray<FGSTN_AbilityBinding> Parent = { MakeBinding(Jab, Grounded, 1), MakeBinding(Jab, Airborne, 2), MakeBinding(Special, FGameplayTag(), 3) };
	const TArray<FGSTN_AbilityBinding> Child = { MakeBinding(Strong, FGameplayTag(), 40), MakeBinding(Jab, Grounded, 10) };

	// Parent first; the child entry with the same command and requirements replaces in place; new child entries follow
	const TArray<FGSTN_AbilityBinding> Result = UGSTN_AbilityBindingUtils::InheritBindings(Parent, Child);
	if (TestEqual(TEXT("three inherited slots plus one new"), Result.Num(), 4))
	{
		TestEqual(TEXT("the grounded jab is replaced in place"), Result[0].PriorityOffset, 10);
		TestEqual(TEXT("the airborne jab is inherited"), Result[1].PriorityOffset, 2);
		TestEqual(TEXT("the special is inherited"), Result[2].PriorityOffset, 3);
		TestTrue(TEXT("the new child binding comes last"), Result[3].Command == Strong);
	}

	// Requirements are sets: the order the tags were added in does not change the key
	FGSTN_AbilityBinding TwoTagsA = MakeBinding(Jab, Grounded, 0);
	TwoTagsA.Requirements.RequireTags.AddTag(Airborne);
	FGSTN_AbilityBinding TwoTagsB = MakeBinding(Jab, Airborne, 0);
	TwoTagsB.Requirements.RequireTags.AddTag(Grounded);
	TestTrue(TEXT("same required tags in another order is the same key"), TwoTagsA.HasSameKey(TwoTagsB));
	TestFalse(TEXT("another requirement is another key"), MakeBinding(Jab, Grounded, 0).HasSameKey(MakeBinding(Jab, Airborne, 0)));

	// A different command with the same requirements replaces nothing
	const TArray<FGSTN_AbilityBinding> Other = UGSTN_AbilityBindingUtils::InheritBindings(Parent, { MakeBinding(Strong, Grounded, 5) });
	TestEqual(TEXT("an unrelated child binding is appended"), Other.Num(), 4);

	// The same list twice is a duplicate
	TArray<FText> Errors;
	TArray<FText> Warnings;
	UGSTN_AbilityBindingUtils::Validate({ MakeBinding(Jab, Grounded, 0), MakeBinding(Jab, Grounded, 1) }, FGSTN_BindingValidationContext(), TEXT("Test"), Errors, Warnings);
	TestEqual(TEXT("two bindings with one key are an error"), CountContaining(Errors, TEXT("arbitrary")), 1);

	// Along a weapon parent chain: root, middle, leaf
	TStrongObjectPtr<UGSTN_WeaponDefinition> Root(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));
	TStrongObjectPtr<UGSTN_WeaponDefinition> Middle(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));
	TStrongObjectPtr<UGSTN_WeaponDefinition> Leaf(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));
	Root->Bindings = { MakeBinding(Jab, Grounded, 1), MakeBinding(Special, FGameplayTag(), 2) };
	Middle->ParentWeapon = TSoftObjectPtr<UGSTN_WeaponDefinition>(Root.Get());
	Middle->Bindings = { MakeBinding(Special, FGameplayTag(), 20), MakeBinding(Strong, FGameplayTag(), 21) };
	Leaf->ParentWeapon = TSoftObjectPtr<UGSTN_WeaponDefinition>(Middle.Get());
	Leaf->Bindings = { MakeBinding(Jab, Grounded, 100) };

	TArray<FGSTN_AbilityBinding> Effective;
	Leaf->GetEffectiveBindings(Effective);
	if (TestEqual(TEXT("the chain yields three distinct bindings"), Effective.Num(), 3))
	{
		TestEqual(TEXT("the leaf wins over the root for the jab"), Effective[0].PriorityOffset, 100);
		TestEqual(TEXT("the middle wins over the root for the special"), Effective[1].PriorityOffset, 20);
		TestEqual(TEXT("the middle's new binding is kept"), Effective[2].PriorityOffset, 21);
	}

	TArray<const UGSTN_WeaponDefinition*> Chain;
	bool bBroken = true;
	Leaf->GetParentChain(Chain, bBroken);
	TestFalse(TEXT("an acyclic chain is not broken"), bBroken);
	TestTrue(TEXT("the chain is root first"), Chain.Num() == 3 && Chain[0] == Root.Get() && Chain[2] == Leaf.Get());

	Root->ParentWeapon = TSoftObjectPtr<UGSTN_WeaponDefinition>(Leaf.Get());
	Leaf->GetParentChain(Chain, bBroken);
	TestTrue(TEXT("a loop is reported and the walk stops"), bBroken);
	Root->ParentWeapon.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DataBindingToRemovedCommandTest, "GSTNCombat.GAS.Data.BindingToRemovedCommandIsError", TestFlags)
bool FGSTN_DataBindingToRemovedCommandTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Jab = GSTNCommandTags::Attack_Neutral.GetTag();
	const FGameplayTag Directional = GSTNCommandTags::Attack_Directional.GetTag();
	const FGameplayTag Special = GSTNCommandTags::Special_Neutral.GetTag();
	const FGameplayTag Combo = GSTNCommandTags::Combo_AttackSpecial_Neutral.GetTag();

	TStrongObjectPtr<UGSTN_InputSlotSet> Base(NewObject<UGSTN_InputSlotSet>(GetTransientPackage()));
	Base->Slots = { MakeInlineSlot(Jab), MakeInlineSlot(Directional), MakeInlineSlot(Special) };

	FGSTN_CommandSet CommandSet;
	CommandSet.BaseSet = TSoftObjectPtr<UGSTN_InputSlotSet>(Base.Get());
	CommandSet.RemovedCommands.AddTag(Directional);

	FGSTN_BindingValidationContext Context;
	Context.CommandSet = &CommandSet;
	Context.BaseSet = Base.Get();

	auto Run = [&](const TArray<FGSTN_AbilityBinding>& Bindings, TArray<FText>& Errors)
	{
		TArray<FText> Warnings;
		Errors.Reset();
		UGSTN_AbilityBindingUtils::Validate(Bindings, Context, TEXT("Kit"), Errors, Warnings);
	};

	TArray<FText> Errors;
	Run({ MakeBinding(Jab, FGameplayTag(), 0), MakeBinding(Special, FGameplayTag(), 0) }, Errors);
	TestEqual(TEXT("bindings to commands of the set are fine"), Errors.Num(), 0);

	Run({ MakeBinding(Directional, FGameplayTag(), 0) }, Errors);
	TestEqual(TEXT("a binding to a removed command is an error"), CountContaining(Errors, TEXT("removes")), 1);

	Run({ MakeBinding(Combo, FGameplayTag(), 0) }, Errors);
	TestEqual(TEXT("a command outside the set without a slot override is an error"), CountContaining(Errors, TEXT("not in the command set")), 1);

	FGSTN_AbilityBinding WithSlot = MakeBinding(Combo, FGameplayTag(), 0);
	WithSlot.SlotOverride = MakeInlineSlot(Combo);
	Run({ WithSlot }, Errors);
	TestEqual(TEXT("a slot override makes a command outside the set valid"), Errors.Num(), 0);

	FGSTN_AbilityBinding WrongSlot = MakeBinding(Combo, FGameplayTag(), 0);
	WrongSlot.SlotOverride = MakeInlineSlot(Special);
	Run({ WrongSlot }, Errors);
	TestEqual(TEXT("a slot override of another command is an error"), CountContaining(Errors, TEXT("another command")), 1);

	// The same on the kit asset, and an added slot makes its command part of the set
	TStrongObjectPtr<UGSTN_PrimaryKitDefinition> Kit(NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage()));
	Kit->CommandSet = CommandSet;
	Kit->KitBindings = { MakeBinding(Directional, FGameplayTag(), 0) };
	TArray<FText> KitErrors;
	TArray<FText> KitWarnings;
	Kit->Validate(KitErrors, KitWarnings);
	TestEqual(TEXT("the kit reports the removed command"), CountContaining(KitErrors, TEXT("removes")), 1);

	Kit->KitBindings = { MakeBinding(Combo, FGameplayTag(), 0) };
	Kit->CommandSet.AddedSlots = { MakeInlineSlot(Combo) };
	KitErrors.Reset();
	KitWarnings.Reset();
	Kit->Validate(KitErrors, KitWarnings);
	TestEqual(TEXT("an added slot puts its command in the set"), CountContaining(KitErrors, TEXT("not in the command set")), 0);

	Kit->CommandSet.AddedSlots.Add(MakeInlineSlot(Directional));
	KitErrors.Reset();
	KitWarnings.Reset();
	Kit->Validate(KitErrors, KitWarnings);
	TestEqual(TEXT("adding a command the kit also removes is a contradiction"), CountContaining(KitErrors, TEXT("both removes and adds")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DataUnknownExtensionTest, "GSTNCombat.GAS.Data.UnknownExtensionIgnored", TestFlags)
bool FGSTN_DataUnknownExtensionTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_PrimaryKitDefinition> Kit(NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage()));
	Kit->KitTags.AddTag(GSTNCombatGASTest::Kit_TestA);

	TArray<FText> BaselineErrors;
	TArray<FText> BaselineWarnings;
	Kit->Validate(BaselineErrors, BaselineWarnings);

	// An entry whose struct is not loaded (its plugin is off) is an empty instanced struct: kept, never read, never an error
	FGSTN_TestKitExtensionA Known;
	Known.Value = 7;
	Kit->Extensions.ExtensionData.Add(FInstancedStruct());
	Kit->Extensions.ExtensionData.Add(FInstancedStruct::Make(Known));
	Kit->Extensions.ExtensionData.Add(FInstancedStruct());

	const FGSTN_TestKitExtensionA* Found = Kit->Extensions.Find<FGSTN_TestKitExtensionA>();
	if (TestNotNull(TEXT("the loaded extension is found past the empty entries"), Found))
	{
		TestEqual(TEXT("its data is intact"), Found->Value, 7);
	}
	TestNull(TEXT("an extension type no entry holds is not found"), Kit->Extensions.Find<FGSTN_TestKitExtensionB>());
	TestEqual(TEXT("the empty entries stay in the asset"), Kit->Extensions.ExtensionData.Num(), 3);

	TArray<FText> Errors;
	TArray<FText> Warnings;
	Kit->Validate(Errors, Warnings);
	TestEqual(TEXT("unknown extension entries add no error"), Errors.Num(), BaselineErrors.Num());
	TestEqual(TEXT("unknown extension entries add no warning"), Warnings.Num(), BaselineWarnings.Num());

	// Only empty entries
	Kit->Extensions.ExtensionData = { FInstancedStruct(), FInstancedStruct() };
	TestNull(TEXT("a list of only empty entries finds nothing"), Kit->Extensions.Find<FGSTN_TestKitExtensionA>());

	// The same list on a weapon
	TStrongObjectPtr<UGSTN_WeaponDefinition> Weapon(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));
	Weapon->Extensions.ExtensionData = { FInstancedStruct(), FInstancedStruct::Make(Known) };
	TestNotNull(TEXT("a weapon reads its extension past an empty entry"), Weapon->Extensions.Find<FGSTN_TestKitExtensionA>());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DataKitIdentityTest, "GSTNCombat.GAS.Data.KitIdentityExactlyOne", TestFlags)
bool FGSTN_DataKitIdentityTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_PrimaryKitDefinition> Kit(NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage()));
	auto IdentityErrors = [&Kit]()
	{
		TArray<FText> Errors;
		TArray<FText> Warnings;
		Kit->Validate(Errors, Warnings);
		return CountContaining(Errors, TEXT("identity tag"));
	};

	TestEqual(TEXT("no identity tag is an error"), IdentityErrors(), 1);

	Kit->KitTags.AddTag(GSTNCombatGASTest::Kit_TestA);
	TestEqual(TEXT("exactly one identity tag is valid"), IdentityErrors(), 0);

	Kit->KitTags.AddTag(GSTNCombatTags::Weapon_Role_Primary.GetTag());
	TestEqual(TEXT("a tag of another root only classifies"), IdentityErrors(), 0);

	Kit->KitTags.AddTag(GSTNCombatGASTest::Weapon_TestA);
	TestEqual(TEXT("a leaf of the weapon root is not a kit identity"), IdentityErrors(), 0);

	Kit->KitTags.AddTag(GSTNCombatGASTest::Kit_TestB);
	TestEqual(TEXT("two identity tags are an error"), IdentityErrors(), 1);

	Kit->KitTags.RemoveTag(GSTNCombatGASTest::Kit_TestA);
	Kit->KitTags.RemoveTag(GSTNCombatGASTest::Kit_TestB);
	Kit->KitTags.AddTag(GSTNCombatTags::Root_Kit.GetTag());
	TestEqual(TEXT("the bare root is not an identity"), IdentityErrors(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_DataWeaponIdentityTest, "GSTNCombat.GAS.Data.WeaponIdentityExactlyOne", TestFlags)
bool FGSTN_DataWeaponIdentityTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_WeaponDefinition> Weapon(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));
	auto IdentityErrors = [&Weapon]()
	{
		TArray<FText> Errors;
		TArray<FText> Warnings;
		Weapon->Validate(Errors, Warnings);
		return CountContaining(Errors, TEXT("identity tag"));
	};

	TestEqual(TEXT("no identity tag is an error"), IdentityErrors(), 1);

	Weapon->WeaponTags.AddTag(GSTNCombatGASTest::Weapon_TestA);
	TestEqual(TEXT("exactly one identity tag is valid"), IdentityErrors(), 0);

	// The role is a classifier under another root
	Weapon->WeaponTags.AddTag(GSTNCombatTags::Weapon_Role_Support.GetTag());
	TestEqual(TEXT("the weapon role does not count as an identity"), IdentityErrors(), 0);

	Weapon->WeaponTags.AddTag(GSTNCombatGASTest::Kit_TestA);
	TestEqual(TEXT("a leaf of the kit root is not a weapon identity"), IdentityErrors(), 0);

	Weapon->WeaponTags.AddTag(GSTNCombatGASTest::Weapon_TestB);
	TestEqual(TEXT("two identity tags are an error"), IdentityErrors(), 1);

	// Stat modifiers need the effect that reads them
	Weapon->WeaponTags.RemoveTag(GSTNCombatGASTest::Weapon_TestB);
	FGSTN_StatValue Modifier;
	Modifier.Stat = GSTNCombatTags::Root_SetByCallerStat.GetTag();
	Weapon->StatModifiers.Add(Modifier);
	TArray<FText> Errors;
	TArray<FText> Warnings;
	Weapon->Validate(Errors, Warnings);
	TestEqual(TEXT("modifiers without a stats effect are an error"), CountContaining(Errors, TEXT("no stats effect")), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
