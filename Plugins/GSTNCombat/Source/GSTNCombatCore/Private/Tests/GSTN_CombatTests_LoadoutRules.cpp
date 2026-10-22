// Automation tests of the loadout rules: each built-in rule with a view that satisfies it and views that break it. Pure: a view
// is plain data, so no asset, world or ability system is involved.

#include "GSTN_LoadoutRulesTestTypes.h"
#include "Tests/GSTN_CombatTestUtils.h"
#include "Input/GSTN_StandardSlots.h"
#include "Input/GSTN_InputTags.h"
#include "Kits/GSTN_CombatAssetTypes.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutRules.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Any tags serve as classifiers: the rules only match containers against the kit's queries
	FGameplayTag LRLightTag() { return GSTNCommandTags::Attack.GetTag(); }
	FGameplayTag LRHeavyTag() { return GSTNCommandTags::Strong.GetTag(); }
	FGameplayTag LRSuperTag() { return GSTNCommandTags::Special.GetTag(); }
	FGameplayTag LRSkinTag() { return GSTNCommandTags::Combo.GetTag(); }

	FGameplayTagContainer LRTags(const FGameplayTag& Tag)
	{
		FGameplayTagContainer Container;
		Container.AddTag(Tag);
		return Container;
	}

	FPrimaryAssetId LRWeaponId(const TCHAR* Name) { return FPrimaryAssetId(GSTNCombatAssetTypes::Weapon(), Name); }
	FPrimaryAssetId LRSuperId(const TCHAR* Name) { return FPrimaryAssetId(GSTNCombatAssetTypes::Super(), Name); }

	FSoftObjectPath LRAbility(const UClass* Class) { return FSoftObjectPath(Class); }

	/** A loadout every built-in rule accepts: a kit that carries one light weapon, has two super slots, and fits one skin kind */
	FGSTN_LoadoutRulesView LRMakeValidView()
	{
		FGSTN_LoadoutRulesView View;
		View.Loadout.PrimaryKit = FPrimaryAssetId(GSTNCombatAssetTypes::PrimaryKit(), TEXT("KitA"));
		View.Kit.bFound = true;
		View.Kit.AllowedAdditionalWeapons = FGameplayTagQuery::MakeQuery_MatchAnyTags(LRTags(LRLightTag()));
		View.Kit.MaxAdditionalWeapons = 1;
		View.Kit.SuperSlotCount = 2;
		View.Kit.AllowedSupers = FGameplayTagQuery::MakeQuery_MatchAnyTags(LRTags(LRSuperTag()));
		View.Kit.CompatibleSkins = FGameplayTagQuery::MakeQuery_MatchAnyTags(LRTags(LRSkinTag()));

		// The primary weapon, which allows swapping the attack for UStruct or UEnum
		FGSTN_LoadoutWeaponView& Primary = View.Weapons.AddDefaulted_GetRef();
		Primary.bFound = true;
		Primary.Tags = LRTags(LRHeavyTag());
		FGSTN_AllowedAlternativeView& Allowed = Primary.AllowedAlternatives.AddDefaulted_GetRef();
		Allowed.Command = GSTNCommandTags::Attack_Neutral.GetTag();
		Allowed.Abilities = { LRAbility(UStruct::StaticClass()), LRAbility(UEnum::StaticClass()) };

		View.Loadout.AdditionalWeapons = { LRWeaponId(TEXT("WeaponA")) };
		FGSTN_LoadoutWeaponView& Additional = View.Weapons.AddDefaulted_GetRef();
		Additional.Id = LRWeaponId(TEXT("WeaponA"));
		Additional.bFound = true;
		Additional.Tags = LRTags(LRLightTag());

		View.Loadout.Supers = { LRSuperId(TEXT("SuperA")), FPrimaryAssetId() };
		FGSTN_LoadoutSuperView& Super = View.Supers.AddDefaulted_GetRef();
		Super.Id = LRSuperId(TEXT("SuperA"));
		Super.bFound = true;
		Super.Tags = LRTags(LRSuperTag());
		View.Supers.AddDefaulted();

		View.Loadout.Skin = FPrimaryAssetId(GSTNCombatAssetTypes::Skin(), TEXT("SkinA"));
		View.Skin.bFound = true;
		View.Skin.Tags = LRTags(LRSkinTag());
		return View;
	}

	FGSTN_SlotCustomisation LRCustomise(int32 Weapon, const FGameplayTag& Command, const UClass* Alternative)
	{
		FGSTN_SlotCustomisation Customisation;
		Customisation.WeaponIndex = Weapon;
		Customisation.Command = Command;
		Customisation.AlternativeAbility = Alternative ? TSoftClassPtr<UObject>(Alternative) : TSoftClassPtr<UObject>();
		return Customisation;
	}

	template <typename TRule>
	FGSTN_LoadoutRuleResult LRRun(const FGSTN_LoadoutRulesView& View)
	{
		return GetDefault<TRule>()->Evaluate(View);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RuleKitExistsTest, "GSTNCombat.Core.LoadoutRules.KitExists", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RuleKitExistsTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("+ a kit that exists passes"), LRRun<UGSTN_LoadoutRule_KitExists>(LRMakeValidView()).IsValid());

	FGSTN_LoadoutRulesView Unknown = LRMakeValidView();
	Unknown.Kit.bFound = false;
	TestEqual(TEXT("- an id that resolves to nothing is an error"), LRRun<UGSTN_LoadoutRule_KitExists>(Unknown).Errors.Num(), 1);

	FGSTN_LoadoutRulesView None = LRMakeValidView();
	None.Loadout.PrimaryKit = FPrimaryAssetId();
	None.Kit.bFound = false;
	TestEqual(TEXT("- no kit id is an error"), LRRun<UGSTN_LoadoutRule_KitExists>(None).Errors.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RuleAdditionalWeaponsTest, "GSTNCombat.Core.LoadoutRules.AdditionalWeapons", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RuleAdditionalWeaponsTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("+ one allowed weapon within the count passes"), LRRun<UGSTN_LoadoutRule_AdditionalWeapons>(LRMakeValidView()).IsValid());

	FGSTN_LoadoutRulesView None = LRMakeValidView();
	None.Loadout.AdditionalWeapons.Reset();
	None.Weapons.SetNum(1);
	TestTrue(TEXT("+ no additional weapon passes"), LRRun<UGSTN_LoadoutRule_AdditionalWeapons>(None).IsValid());

	FGSTN_LoadoutRulesView TooMany = LRMakeValidView();
	TooMany.Loadout.AdditionalWeapons.Add(LRWeaponId(TEXT("WeaponB")));
	FGSTN_LoadoutWeaponView& Second = TooMany.Weapons.AddDefaulted_GetRef();
	Second.bFound = true;
	Second.Tags = LRTags(LRLightTag());
	TestEqual(TEXT("- more weapons than the kit allows is one error"), LRRun<UGSTN_LoadoutRule_AdditionalWeapons>(TooMany).Errors.Num(), 1);

	FGSTN_LoadoutRulesView Unmatched = LRMakeValidView();
	Unmatched.Weapons[1].Tags = LRTags(LRHeavyTag());
	TestEqual(TEXT("- a weapon the kit's query does not match is an error"), LRRun<UGSTN_LoadoutRule_AdditionalWeapons>(Unmatched).Errors.Num(), 1);

	FGSTN_LoadoutRulesView Missing = LRMakeValidView();
	Missing.Weapons[1].bFound = false;
	TestEqual(TEXT("- a weapon that does not exist is an error"), LRRun<UGSTN_LoadoutRule_AdditionalWeapons>(Missing).Errors.Num(), 1);

	FGSTN_LoadoutRulesView EmptyQuery = LRMakeValidView();
	EmptyQuery.Kit.AllowedAdditionalWeapons = FGameplayTagQuery();
	TestEqual(TEXT("- an empty query allows none"), LRRun<UGSTN_LoadoutRule_AdditionalWeapons>(EmptyQuery).Errors.Num(), 1);

	FGSTN_LoadoutRulesView NoKit = LRMakeValidView();
	NoKit.Kit.bFound = false;
	TestTrue(TEXT("a loadout with no kit is left to the kit rule"), LRRun<UGSTN_LoadoutRule_AdditionalWeapons>(NoKit).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RuleSupersTest, "GSTNCombat.Core.LoadoutRules.Supers", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RuleSupersTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("+ an allowed super in a slot, and an empty slot, pass"), LRRun<UGSTN_LoadoutRule_Supers>(LRMakeValidView()).IsValid());

	FGSTN_LoadoutRulesView Beyond = LRMakeValidView();
	Beyond.Kit.SuperSlotCount = 0;
	TestEqual(TEXT("- a super beyond the kit's slots is an error"), LRRun<UGSTN_LoadoutRule_Supers>(Beyond).Errors.Num(), 1);

	FGSTN_LoadoutRulesView Missing = LRMakeValidView();
	Missing.Supers[0].bFound = false;
	TestEqual(TEXT("- a super that does not exist is an error"), LRRun<UGSTN_LoadoutRule_Supers>(Missing).Errors.Num(), 1);

	FGSTN_LoadoutRulesView Unmatched = LRMakeValidView();
	Unmatched.Supers[0].Tags = LRTags(LRHeavyTag());
	TestEqual(TEXT("- a super the kit's query does not match is an error"), LRRun<UGSTN_LoadoutRule_Supers>(Unmatched).Errors.Num(), 1);

	FGSTN_LoadoutRulesView EmptyQuery = LRMakeValidView();
	EmptyQuery.Kit.AllowedSupers = FGameplayTagQuery();
	TestEqual(TEXT("- an empty query allows none"), LRRun<UGSTN_LoadoutRule_Supers>(EmptyQuery).Errors.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RuleCustomisationsTest, "GSTNCombat.Core.LoadoutRules.Customisations", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RuleCustomisationsTest::RunTest(const FString& Parameters)
{
	const FGameplayTag Neutral = GSTNCommandTags::Attack_Neutral.GetTag();

	FGSTN_LoadoutRulesView Allowed = LRMakeValidView();
	Allowed.Loadout.SlotCustomisations = { LRCustomise(0, Neutral, UEnum::StaticClass()) };
	TestTrue(TEXT("+ a swap for an allowed alternative passes"), LRRun<UGSTN_LoadoutRule_Customisations>(Allowed).IsValid());
	TestTrue(TEXT("+ no customisation passes"), LRRun<UGSTN_LoadoutRule_Customisations>(LRMakeValidView()).IsValid());

	FGSTN_LoadoutRulesView NotAllowed = LRMakeValidView();
	NotAllowed.Loadout.SlotCustomisations = { LRCustomise(0, Neutral, UFunction::StaticClass()) };
	TestEqual(TEXT("- an ability the weapon does not list is an error"), LRRun<UGSTN_LoadoutRule_Customisations>(NotAllowed).Errors.Num(), 1);

	FGSTN_LoadoutRulesView OtherCommand = LRMakeValidView();
	OtherCommand.Loadout.SlotCustomisations = { LRCustomise(0, GSTNCommandTags::Strong_Neutral.GetTag(), UEnum::StaticClass()) };
	TestEqual(TEXT("- a command the weapon lists no alternatives for is an error"), LRRun<UGSTN_LoadoutRule_Customisations>(OtherCommand).Errors.Num(), 1);

	FGSTN_LoadoutRulesView OtherWeapon = LRMakeValidView();
	OtherWeapon.Loadout.SlotCustomisations = { LRCustomise(1, Neutral, UEnum::StaticClass()) };
	TestEqual(TEXT("- the alternatives of another weapon do not count"), LRRun<UGSTN_LoadoutRule_Customisations>(OtherWeapon).Errors.Num(), 1);

	FGSTN_LoadoutRulesView NoWeapon = LRMakeValidView();
	NoWeapon.Loadout.SlotCustomisations = { LRCustomise(5, Neutral, UEnum::StaticClass()) };
	TestEqual(TEXT("- a weapon the loadout does not have is an error"), LRRun<UGSTN_LoadoutRule_Customisations>(NoWeapon).Errors.Num(), 1);

	FGSTN_LoadoutRulesView NoAbility = LRMakeValidView();
	NoAbility.Loadout.SlotCustomisations = { LRCustomise(0, Neutral, nullptr) };
	TestEqual(TEXT("- a swap for nothing is an error"), LRRun<UGSTN_LoadoutRule_Customisations>(NoAbility).Errors.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RuleSkinTest, "GSTNCombat.Core.LoadoutRules.Skin", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RuleSkinTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("+ a skin that fits passes"), LRRun<UGSTN_LoadoutRule_Skin>(LRMakeValidView()).IsValid());

	FGSTN_LoadoutRulesView NoSkin = LRMakeValidView();
	NoSkin.Loadout.Skin = FPrimaryAssetId();
	NoSkin.Skin.bFound = false;
	TestTrue(TEXT("+ no skin passes"), LRRun<UGSTN_LoadoutRule_Skin>(NoSkin).IsValid());

	FGSTN_LoadoutRulesView Missing = LRMakeValidView();
	Missing.Skin.bFound = false;
	TestEqual(TEXT("- a skin that does not exist is an error"), LRRun<UGSTN_LoadoutRule_Skin>(Missing).Errors.Num(), 1);

	FGSTN_LoadoutRulesView Unfit = LRMakeValidView();
	Unfit.Skin.Tags = LRTags(LRHeavyTag());
	TestEqual(TEXT("- a skin the kit's query does not match is an error"), LRRun<UGSTN_LoadoutRule_Skin>(Unfit).Errors.Num(), 1);

	FGSTN_LoadoutRulesView EmptyQuery = LRMakeValidView();
	EmptyQuery.Kit.CompatibleSkins = FGameplayTagQuery();
	TestEqual(TEXT("- an empty query fits no skin"), LRRun<UGSTN_LoadoutRule_Skin>(EmptyQuery).Errors.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RuleNoAmbiguousBindingsTest, "GSTNCombat.Core.LoadoutRules.NoAmbiguousBindings", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RuleNoAmbiguousBindingsTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("+ a table with no findings passes"), LRRun<UGSTN_LoadoutRule_NoAmbiguousBindings>(LRMakeValidView()).IsValid());

	FGSTN_LoadoutRulesView Found = LRMakeValidView();
	Found.Table.Errors = { TEXT("Bindings 0 and 1 of Command.Attack.Neutral are arbitrary.") };
	Found.Table.Warnings = { TEXT("A is shadowed by B.") };
	const FGSTN_LoadoutRuleResult Result = LRRun<UGSTN_LoadoutRule_NoAmbiguousBindings>(Found);
	TestEqual(TEXT("- a table error is an error"), Result.Errors.Num(), 1);
	TestEqual(TEXT("a shadowed slot is only a warning"), Result.Warnings.Num(), 1);

	// With the real validators: the same bindings are rejected without an exclusive context group and accepted with one
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNCombatTest::MakeSettings();
	TStrongObjectPtr<UGSTN_InputSlotSet> Base(NewObject<UGSTN_InputSlotSet>(GetTransientPackage()));
	for (const FGSTN_InputSlotSpec& Spec : UGSTN_StandardSlots::Build())
	{
		Base->Slots.AddDefaulted_GetRef().Inline = Spec;
	}
	FGSTN_CompileInput Input;
	Input.Kit.BaseSet = Base.Get();
	for (const FGameplayTag& Context : { GSTNInputTags::Context_Grounded.GetTag(), GSTNInputTags::Context_Airborne.GetTag() })
	{
		FGSTN_BindingInput& Binding = Input.Kit.Bindings.AddDefaulted_GetRef();
		Binding.Command = GSTNCommandTags::Attack_Neutral.GetTag();
		Binding.Ability = LRAbility(Context == GSTNInputTags::Context_Grounded.GetTag() ? UStruct::StaticClass() : UEnum::StaticClass());
		Binding.Requirements.RequireTags.AddTag(Context);
	}

	const auto Check = [&]()
	{
		FGSTN_CompileReport Report;
		const FGSTN_CompiledBindingTable Table = FGSTN_BindingTableCompiler::Compile(Input, *Settings, &Report);
		FGSTN_BindingTableCompiler::FindConflicts(Table, *Settings, Report);
		FGSTN_LoadoutRulesView View = LRMakeValidView();
		for (const FText& Error : Report.Errors)
		{
			View.Table.Errors.Add(Error.ToString());
		}
		return LRRun<UGSTN_LoadoutRule_NoAmbiguousBindings>(View);
	};

	Settings->ExclusiveContextGroups.Reset();
	TestEqual(TEXT("- the validator's ambiguity reaches the rule"), Check().Errors.Num(), 1);

	FGSTN_ExclusiveTagGroup Group;
	Group.Tags.AddTag(GSTNInputTags::Context_Grounded.GetTag());
	Group.Tags.AddTag(GSTNInputTags::Context_Airborne.GetTag());
	Settings->ExclusiveContextGroups = { Group };
	TestTrue(TEXT("+ with the exclusive group the same bindings pass"), Check().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RuleTimelineRegistryTest, "GSTNCombat.Core.LoadoutRules.TimelineRegistry", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RuleTimelineRegistryTest::RunTest(const FString& Parameters)
{
	FGSTN_LoadoutRulesView View = LRMakeValidView();
	TestTrue(TEXT("+ nothing required passes"), LRRun<UGSTN_LoadoutRule_TimelineRegistry>(View).IsValid());

	View.Table.RequiredTimelineAbilities = { LRAbility(UStruct::StaticClass()) };
	View.Table.RegistryAbilities = { LRAbility(UEnum::StaticClass()), LRAbility(UStruct::StaticClass()) };
	TestTrue(TEXT("+ a required ability in the registry passes"), LRRun<UGSTN_LoadoutRule_TimelineRegistry>(View).IsValid());

	View.Table.RegistryAbilities = { LRAbility(UEnum::StaticClass()) };
	TestEqual(TEXT("- a required ability missing from the registry is an error"), LRRun<UGSTN_LoadoutRule_TimelineRegistry>(View).Errors.Num(), 1);

	// The compiler puts the kit's timeline abilities into the registry itself
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNCombatTest::MakeSettings();
	FGSTN_CompileInput Input;
	Input.Kit.TriggeredAbilities = { LRAbility(UStruct::StaticClass()) };
	Input.IsTimelineAbility = [](const FSoftObjectPath&) { return true; };
	const FGSTN_CompiledBindingTable Table = FGSTN_BindingTableCompiler::Compile(Input, *Settings);
	FGSTN_LoadoutRulesView Compiled = LRMakeValidView();
	Compiled.Table.RequiredTimelineAbilities = Input.Kit.TriggeredAbilities;
	for (const FGSTN_TimelineRegistryEntry& Entry : Table.Registry)
	{
		Compiled.Table.RegistryAbilities.AddUnique(Entry.Ability);
	}
	TestTrue(TEXT("+ the compiled registry satisfies the rule"), LRRun<UGSTN_LoadoutRule_TimelineRegistry>(Compiled).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_RuleSetTest, "GSTNCombat.Core.LoadoutRules.DefaultSetAndCustomRule", GSTN_CORE_TEST_FLAGS)
bool FGSTN_RuleSetTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_LoadoutRules> Rules(NewObject<UGSTN_LoadoutRules>(GetTransientPackage()));
	TestEqual(TEXT("the default asset lists the seven built-in rules"), Rules->Rules.Num(), 7);
	TestTrue(TEXT("a loadout every rule accepts is valid"), Rules->Validate(LRMakeValidView()).IsValid());

	// Pure: the same view twice is the same result
	FGSTN_LoadoutRulesView Broken = LRMakeValidView();
	Broken.Kit.SuperSlotCount = 0;
	Broken.Weapons[1].bFound = false;
	const FGSTN_LoadoutRuleResult First = Rules->Validate(Broken);
	const FGSTN_LoadoutRuleResult Second = Rules->Validate(Broken);
	TestEqual(TEXT("two findings from two rules"), First.Errors.Num(), 2);
	TestTrue(TEXT("the same view gives the same findings"), First.Errors.Num() == Second.Errors.Num() && First.Errors[0].EqualTo(Second.Errors[0]) && First.Errors[1].EqualTo(Second.Errors[1]));
	TestTrue(TEXT("in the order of the list: weapons before supers"), First.Errors[0].ToString().Contains(TEXT("weapon")) && First.Errors[1].ToString().Contains(TEXT("Super")));

	// A custom rule joins the list like a built-in one, and an empty entry is skipped
	Rules->Rules.Add(TObjectPtr<UGSTN_LoadoutRule>());
	Rules->Rules.Add(NewObject<UGSTN_TestLoadoutRuleNoSkin>(Rules.Get()));
	const FGSTN_LoadoutRuleResult Custom = Rules->Validate(LRMakeValidView());
	TestEqual(TEXT("the custom rule rejects the skin"), Custom.Errors.Num(), 1);
	TestEqual(TEXT("and warns about the extra weapon"), Custom.Warnings.Num(), 1);

	// Removing a rule removes its findings (the custom rule goes too, so only the weapon rule is left to report)
	Rules->Rules.RemoveAll([](const UGSTN_LoadoutRule* Rule) { return Rule && (Rule->IsA<UGSTN_LoadoutRule_Supers>() || Rule->IsA<UGSTN_TestLoadoutRuleNoSkin>()); });
	TestEqual(TEXT("a dropped rule no longer reports"), Rules->Validate(Broken).Errors.Num(), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
