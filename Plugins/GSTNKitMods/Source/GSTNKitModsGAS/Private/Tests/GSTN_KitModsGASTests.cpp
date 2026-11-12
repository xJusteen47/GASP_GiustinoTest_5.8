// Automation tests of the kit mod loadout extension: resolution (slots, compatibility, kit without slots), the compile contribution
// (removed and added verbs, passives, derived timelines with stable paths) and the rules view (warnings, unlocked alternatives).
// Transient assets, no world.

#include "Misc/AutomationTest.h"
#include "Abilities/GameplayAbility.h"
#include "GSTN_KitModBaking.h"
#include "GSTN_KitModDefinition.h"
#include "GSTN_KitModLoadout.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Kits/GSTN_PrimaryKitDefinition.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Loadout/GSTN_LoadoutAssembly.h"
#include "Loadout/GSTN_LoadoutExtensions.h"
#include "Loadout/GSTN_LoadoutRules.h"
#include "Timeline/GSTN_CombatTimeline.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EAutomationTestFlags KitModsGASTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	struct FModFixture
	{
		UGSTN_PrimaryKitDefinition* Kit = nullptr;
		UGSTN_WeaponDefinition* Weapon = nullptr;
		UGSTN_KitModDefinition* LongBlade = nullptr;
		UGSTN_KitModDefinition* Picky = nullptr;
		UGSTN_CombatTimeline* Slash = nullptr;
		FGSTN_ResolvedLoadout Resolved;

		FModFixture()
		{
			Kit = NewObject<UGSTN_PrimaryKitDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
			FGSTN_KitModSlots Slots;
			Slots.Slots = { GSTNKitModsTags::Mod_Slot, GSTNKitModsTags::Mod_Slot };
			Slots.Limits.MaxReachScale = 1.25f;
			Kit->Extensions.ExtensionData.Add(FInstancedStruct::Make(Slots));
			Weapon = NewObject<UGSTN_WeaponDefinition>(GetTransientPackage(), NAME_None, RF_Transient);

			Slash = NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transient);
			Slash->Data.TotalFrames = 20;
			Slash->Data.FirstActionableFrame = 16;

			LongBlade = NewObject<UGSTN_KitModDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
			LongBlade->Slot = GSTNKitModsTags::Mod_Slot;
			FGSTN_MoveModifier& Modifier = LongBlade->MoveModifiers.AddDefaulted_GetRef();
			Modifier.TimeScale = 1.5f;
			Modifier.ReachScale = 1.5f;
			LongBlade->RemovedCommands.AddTag(GSTNCommandTags::Strong);
			LongBlade->Passives.Add(TSoftClassPtr<UGameplayAbility>(UGameplayAbility::StaticClass()));
			LongBlade->UnlockedAlternatives.Add(GSTNCommandTags::Special).Abilities.Add(TSoftClassPtr<UGameplayAbility>(UGameplayAbility::StaticClass()));

			// Fits only kits tagged Mod (the test kit is not)
			Picky = NewObject<UGSTN_KitModDefinition>(GetTransientPackage(), NAME_None, RF_Transient);
			Picky->Slot = GSTNKitModsTags::Mod_Slot;
			Picky->CompatibleKits = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(GSTNKitModsTags::Mod));

			Resolved.Kit = Kit;
			Resolved.Weapons = { Weapon };
			FGSTN_LoadoutModsExt Ext;
			FGSTN_KitModConfig& Config = Ext.Configs.AddDefaulted_GetRef();
			Config.Mods.Add({ GSTNKitModsTags::Mod_Slot, FSoftObjectPath(LongBlade) });
			Config.Mods.Add({ GSTNKitModsTags::Mod_Slot, FSoftObjectPath(Picky) });
			Resolved.Loadout.Extensions.Add(FInstancedStruct::Make(Ext));
		}

		FGSTN_CompileInput MakeInput() const
		{
			FGSTN_CompileInput Input;
			FGSTN_BindingInput& Attack = Input.Kit.Bindings.AddDefaulted_GetRef();
			Attack.Command = GSTNCommandTags::Attack;
			Attack.Ability = FSoftObjectPath(UGameplayAbility::StaticClass());
			Attack.TimelineOverride = FSoftObjectPath(Slash);
			FGSTN_WeaponCompileInput& WeaponInput = Input.Weapons.AddDefaulted_GetRef();
			FGSTN_BindingInput& Strong = WeaponInput.Bindings.AddDefaulted_GetRef();
			Strong.Command = GSTNCommandTags::Strong;
			Strong.Ability = FSoftObjectPath(UGameplayAbility::StaticClass());
			return Input;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNKitModsResolveTest, "GSTNKitMods.GAS.Resolve", KitModsGASTestFlags)
bool FGSTNKitModsResolveTest::RunTest(const FString& Parameters)
{
	FModFixture Fixture;
	FGSTN_ResolvedMods Mods;
	FGSTN_KitModLoadout::ResolveMods(Fixture.Resolved, Mods);
	TestEqual(TEXT("Only the compatible mod applies"), Mods.Mods.Num(), 1);
	TestTrue(TEXT("It is the long blade"), Mods.Mods.Num() == 1 && Mods.Mods[0] == Fixture.LongBlade);
	TestEqual(TEXT("The picky mod is a warning, not an error"), Mods.Warnings.Num(), 1);

	Fixture.Kit->Extensions.ExtensionData.Reset();
	FGSTN_KitModLoadout::ResolveMods(Fixture.Resolved, Mods);
	TestEqual(TEXT("A kit without slots takes no mod"), Mods.Mods.Num(), 0);
	TestEqual(TEXT("...and says so once"), Mods.Warnings.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNKitModsCompileTest, "GSTNKitMods.GAS.Compile", KitModsGASTestFlags)
bool FGSTNKitModsCompileTest::RunTest(const FString& Parameters)
{
	FModFixture Fixture;
	FGSTN_CompileInput Input = Fixture.MakeInput();
	FGSTN_LoadoutExtensionContext Context;
	Context.Resolved = &Fixture.Resolved;
	FGSTN_KitModLoadout::ContributeCompileInput(Context, Input);

	TestEqual(TEXT("The removed command is gone from the weapon"), Input.Weapons[0].Bindings.Num(), 0);
	TestTrue(TEXT("The mod's passive is on the kit"), Input.Kit.Passives.Contains(FSoftObjectPath(UGameplayAbility::StaticClass())));
	const FSoftObjectPath Derived = Input.Kit.Bindings[0].TimelineOverride;
	TestNotEqual(TEXT("The attack starts a derived timeline"), Derived, FSoftObjectPath(Fixture.Slash));
	const UGSTN_CombatTimeline* Timeline = Cast<UGSTN_CombatTimeline>(Derived.ResolveObject());
	TestTrue(TEXT("The derived timeline resolves"), Timeline != nullptr);
	if (Timeline)
	{
		TestEqual(TEXT("Its frames are scaled"), Timeline->Data.TotalFrames, 30);
		TestEqual(TEXT("Its name is the stable one"), Timeline->GetFName(), FGSTN_KitModBaking::MakeDerivedName(FSoftObjectPath(Fixture.Slash), { FSoftObjectPath(Fixture.LongBlade) }));
	}

	// A second compile (another machine, a respawn) names the same object
	FGSTN_CompileInput Again = Fixture.MakeInput();
	FGSTN_KitModLoadout::ContributeCompileInput(Context, Again);
	TestEqual(TEXT("Same path on recompile"), Again.Kit.Bindings[0].TimelineOverride, Derived);

	// The cap: no derived timeline allowed, the base stays
	FGSTN_KitModSlots* Slots = Fixture.Kit->Extensions.ExtensionData[0].GetMutablePtr<FGSTN_KitModSlots>();
	Slots->MaxDerivedTimelines = 0;
	FGSTN_CompileInput Capped = Fixture.MakeInput();
	FGSTN_KitModLoadout::ContributeCompileInput(Context, Capped);
	TestEqual(TEXT("Past the cap the base timeline stays"), Capped.Kit.Bindings[0].TimelineOverride, FSoftObjectPath(Fixture.Slash));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNKitModsRulesViewTest, "GSTNKitMods.GAS.RulesView", KitModsGASTestFlags)
bool FGSTNKitModsRulesViewTest::RunTest(const FString& Parameters)
{
	FModFixture Fixture;
	FGSTN_LoadoutRulesView View;
	View.Weapons.AddDefaulted();
	FGSTN_LoadoutExtensionContext Context;
	Context.Resolved = &Fixture.Resolved;
	FGSTN_KitModLoadout::ContributeRulesView(Context, View);
	TestEqual(TEXT("The picky mod's warning reaches the rules"), View.Table.Warnings.Num(), 1);
	const FGSTN_AllowedAlternativeView* Allowed = View.Weapons[0].AllowedAlternatives.FindByPredicate(
		[](const FGSTN_AllowedAlternativeView& Entry) { return Entry.Command == GSTNCommandTags::Special; });
	TestTrue(TEXT("The unlocked special alternative is allowed"), Allowed && Allowed->Abilities.Contains(FSoftObjectPath(UGameplayAbility::StaticClass())));
	TestTrue(TEXT("The extension is registered"), FGSTN_LoadoutExtensionRegistry::IsRegistered(FGSTN_KitModLoadout::ExtensionId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNKitModsCustomisedRowTest, "GSTNKitMods.GAS.CustomisedRowBakesAlternative", KitModsGASTestFlags)
bool FGSTNKitModsCustomisedRowTest::RunTest(const FString& Parameters)
{
	// L5b: the compiler swaps a slot customisation's alternative into a weapon row after the extensions and keeps the row's override,
	// so the mod must bake the alternative's timeline. A combat timeline class of the NP module, found by name (this plugin never links it)
	UClass* Alternative = FindObject<UClass>(nullptr, TEXT("/Script/GSTNCombatNP.GSTN_TestMontageTimelineAbility"));
	if (!Alternative)
	{
		AddInfo(TEXT("GSTNCombatNP is not loaded: nothing to check"));
		return true;
	}
	FModFixture Fixture;
	FGSTN_CompileInput Input = Fixture.MakeInput();
	FGSTN_BindingInput& WeaponAttack = Input.Weapons[0].Bindings.AddDefaulted_GetRef();
	WeaponAttack.Command = GSTNCommandTags::Attack;
	WeaponAttack.Ability = FSoftObjectPath(UGameplayAbility::StaticClass());	// no timeline: alone it would never be modded
	FGSTN_SlotCustomisation& Customisation = Input.Customisations.AddDefaulted_GetRef();
	Customisation.WeaponIndex = 0;
	Customisation.Command = GSTNCommandTags::Attack;
	Customisation.AlternativeAbility = TSoftClassPtr<UObject>(Alternative);

	FGSTN_LoadoutExtensionContext Context;
	Context.Resolved = &Fixture.Resolved;
	FGSTN_KitModLoadout::ContributeCompileInput(Context, Input);
	if (!TestEqual(TEXT("The weapon keeps its attack row"), Input.Weapons[0].Bindings.Num(), 1))
	{
		return false;
	}
	const FGSTN_BindingInput& Row = Input.Weapons[0].Bindings[0];
	TestEqual(TEXT("The row still names its own ability (the compiler swaps it)"), Row.Ability, FSoftObjectPath(UGameplayAbility::StaticClass()));
	TestEqual(TEXT("The derived timeline is baked from the alternative"), Row.TimelineOverride.GetAssetFName(),
		FGSTN_KitModBaking::MakeDerivedName(FSoftObjectPath(Alternative), { FSoftObjectPath(Fixture.LongBlade) }));

	// Another weapon's customisation does not touch this row
	FGSTN_CompileInput Other = Fixture.MakeInput();
	FGSTN_BindingInput& OtherAttack = Other.Weapons[0].Bindings.AddDefaulted_GetRef();
	OtherAttack.Command = GSTNCommandTags::Attack;
	OtherAttack.Ability = FSoftObjectPath(UGameplayAbility::StaticClass());
	FGSTN_SlotCustomisation& Elsewhere = Other.Customisations.AddDefaulted_GetRef();
	Elsewhere.WeaponIndex = 1;
	Elsewhere.Command = GSTNCommandTags::Attack;
	Elsewhere.AlternativeAbility = TSoftClassPtr<UObject>(Alternative);
	FGSTN_KitModLoadout::ContributeCompileInput(Context, Other);
	TestFalse(TEXT("A row customised on another weapon is not modded"), Other.Weapons[0].Bindings.Num() == 1 && Other.Weapons[0].Bindings[0].TimelineOverride.IsValid());
	return true;
}

#endif
