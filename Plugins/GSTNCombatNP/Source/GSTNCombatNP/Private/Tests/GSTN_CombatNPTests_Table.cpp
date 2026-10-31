// Automation tests of the Core table to combat table set conversion. Pure: the class loader is injected, so no content, world or
// ability system instance is involved. Classes are the hidden test abilities of this module and the plugin's own.

#include "Abilities/GSTN_GA_PlatformJump.h"
#include "Effects/GSTN_GE_WeaponStats.h"
#include "Kits/GSTN_WeaponDefinition.h"
#include "Loadout/GSTN_LoadoutRules.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Input/GSTN_CombatSettings.h"
#include "Input/GSTN_InputSettings.h"
#include "Loadout/GSTN_EntrySourceMask.h"
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "Tests/GSTN_ReviewTestTypes.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TblCommandAlpha, "Command.NPTable.Alpha", "Test-only tag of the GSTNCombatNP table automation tests");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TblCommandZeta, "Command.NPTable.Zeta", "Test-only tag of the GSTNCombatNP table automation tests");

	FSoftObjectPath PlainPath() { return FSoftObjectPath(UGSTN_TestCommitAbility::StaticClass()); }
	FSoftObjectPath OtherPlainPath() { return FSoftObjectPath(UGSTN_TestRefusedCommitAbility::StaticClass()); }
	FSoftObjectPath TimelinePath() { return FSoftObjectPath(UGSTN_TestMontageTimelineAbility::StaticClass()); }
	FSoftObjectPath JumpPath() { return FSoftObjectPath(UGSTN_GA_PlatformJump::StaticClass()); }

	UClass* ResolveClass(const FSoftObjectPath& Path)
	{
		return Cast<UClass>(Path.ResolveObject());
	}

	FGSTN_CompiledEntry MakeEntry(EGSTN_EntrySource Source, int32 SourceIndex, const FGameplayTag& Command, const FSoftObjectPath& Ability, int32 SlotIndex, int32 TimelineIndex = INDEX_NONE)
	{
		FGSTN_CompiledEntry Entry;
		Entry.Source = Source;
		Entry.SourceIndex = static_cast<uint8>(SourceIndex);
		Entry.Command = Command;
		Entry.Ability = Ability;
		Entry.SlotIndex = SlotIndex;
		Entry.TimelineIndex = static_cast<int16>(TimelineIndex);
		return Entry;
	}

	/**
	 * Entries in the Core order (kit, weapon 0, then more): [0] kit Zeta (plain), [1] kit Alpha (timeline, registry 0),
	 * [2] weapon 0 Alpha (plain), [3] weapon 1 Alpha (jump). Slots: Alpha, Zeta
	 */
	FGSTN_CompiledBindingTable MakeCompiled(int32 Revision = 3)
	{
		FGSTN_CompiledBindingTable Compiled;
		Compiled.Revision = Revision;
		Compiled.WeaponCount = 2;
		FGSTN_InputSlotSpec& Alpha = Compiled.Slots.AddDefaulted_GetRef();
		Alpha.Command = TblCommandAlpha;
		FGSTN_InputSlotSpec& Zeta = Compiled.Slots.AddDefaulted_GetRef();
		Zeta.Command = TblCommandZeta;
		Compiled.Entries.Add(MakeEntry(EGSTN_EntrySource::Kit, 0, TblCommandZeta, PlainPath(), 1));
		Compiled.Entries.Add(MakeEntry(EGSTN_EntrySource::Kit, 0, TblCommandAlpha, TimelinePath(), 0, 0));
		Compiled.Entries.Add(MakeEntry(EGSTN_EntrySource::Weapon, 0, TblCommandAlpha, PlainPath(), 0));
		Compiled.Entries.Add(MakeEntry(EGSTN_EntrySource::Weapon, 1, TblCommandAlpha, JumpPath(), 0));
		FGSTN_TimelineRegistryEntry& Registry = Compiled.Registry.AddDefaulted_GetRef();
		Registry.Ability = TimelinePath();
		return Compiled;
	}

	struct FTblSettings
	{
		TStrongObjectPtr<UGSTN_InputSettings> Input{ NewObject<UGSTN_InputSettings>(GetTransientPackage()) };
		TStrongObjectPtr<UGSTN_CombatSettings> Combat{ NewObject<UGSTN_CombatSettings>(GetTransientPackage()) };
	};

	TStrongObjectPtr<UGSTN_CombatTableSet> Convert(const FGSTN_CompiledBindingTable& Compiled, FTblSettings& Settings, FGSTN_CompileReport* Report = nullptr)
	{
		FGSTN_CombatTableConvertInput Input(&ResolveClass);
		Input.InputSettings = Settings.Input.Get();
		Input.CombatSettings = Settings.Combat.Get();
		return TStrongObjectPtr<UGSTN_CombatTableSet>(FGSTN_CombatTableConverter::Convert(GetTransientPackage(), Compiled, Input, Report));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTableConvertOrderTest, "GSTNCombat.NP.Table.ConvertOrder", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTableConvertOrderTest::RunTest(const FString& Parameters)
{
	FTblSettings Settings;
	const TStrongObjectPtr<UGSTN_CombatTableSet> Set = Convert(MakeCompiled(), Settings);
	const FGSTN_CommandTable& Table = Set->Table;

	if (!TestEqual(TEXT("two commands"), Table.Commands.Num(), 2) || !TestEqual(TEXT("four bindings"), Table.Bindings.Num(), 4))
	{
		return false;
	}
	TestTrue(TEXT("commands are sorted by tag name (Alpha first)"), Table.Commands[0].Tag == TblCommandAlpha.GetTag());
	TestTrue(TEXT("then Zeta"), Table.Commands[1].Tag == TblCommandZeta.GetTag());
	TestEqual(TEXT("Alpha owns the first three bindings"), Table.Commands[0].NumBindings, 3);
	TestEqual(TEXT("and Zeta the last"), Table.Commands[1].FirstBinding, 3);

	// Entry order inside a command: the kit's, then weapon 0, then weapon 1
	TestEqual(TEXT("kit binding first"), Table.Bindings[0].SourceMask, GSTNEntrySourceMask::Bit(EGSTN_EntrySource::Kit, 0));
	TestEqual(TEXT("weapon 0 next"), Table.Bindings[1].SourceMask, GSTNEntrySourceMask::Bit(EGSTN_EntrySource::Weapon, 0));
	TestEqual(TEXT("weapon 1 last"), Table.Bindings[2].SourceMask, GSTNEntrySourceMask::Bit(EGSTN_EntrySource::Weapon, 1));
	TestEqual(TEXT("the plain ability class"), Table.Bindings[3].Ability.Get(), UGSTN_TestCommitAbility::StaticClass());
	TestEqual(TEXT("both commands are candidates"), Table.CandidateSlots.Num(), 2);
	TestTrue(TEXT("the mask makes the table masked"), Table.HasSourceMasks());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTableSlotPointersTest, "GSTNCombat.NP.Table.ConvertSlotPointersStable", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTableSlotPointersTest::RunTest(const FString& Parameters)
{
	FTblSettings Settings;
	FGSTN_CompiledBindingTable Compiled = MakeCompiled();
	Compiled.Entries[3].PriorityOffset = 5;
	const TStrongObjectPtr<UGSTN_CombatTableSet> Set = Convert(Compiled, Settings);

	// The commands' slots are the set's own copy of the compiled slots, not the caller's, so the caller may free its table
	TestTrue(TEXT("Alpha's slot is in the set's slots"), Set->Table.Commands[0].Slot == &Set->Compiled.Slots[0]);
	TestTrue(TEXT("Zeta's slot is in the set's slots"), Set->Table.Commands[1].Slot == &Set->Compiled.Slots[1]);
	TestTrue(TEXT("the slot is the right command's"), Set->Table.Commands[0].Slot->Command == TblCommandAlpha.GetTag());

	// A binding with its own offset has its own slot; the others use the command's
	TestTrue(TEXT("weapon 1's binding has an own slot"), Set->Table.Bindings[2].Slot != nullptr);
	TestEqual(TEXT("with the offset applied"), Set->Table.Bindings[2].Slot->PriorityOffset, 5);
	TestTrue(TEXT("the kit's binding uses the command's"), Set->Table.Bindings[0].Slot == nullptr);

	const FGSTN_InputSlotSpec* Before = Set->Table.Commands[0].Slot;
	Compiled.Slots.Reset();
	TestTrue(TEXT("mutating the source changes nothing"), Set->Table.Commands[0].Slot == Before && Before->Command == TblCommandAlpha.GetTag());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTableRegistryTest, "GSTNCombat.NP.Table.ConvertRegistryMatchesCompiled", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTableRegistryTest::RunTest(const FString& Parameters)
{
	FTblSettings Settings;
	FGSTN_CompiledBindingTable Compiled = MakeCompiled();
	FGSTN_TimelineRegistryEntry& Second = Compiled.Registry.AddDefaulted_GetRef();
	Second.Ability = PlainPath();	// not a combat timeline: dropped with an error, the entries that refer to it become plain
	Compiled.Entries[2].TimelineIndex = 1;

	FGSTN_CompileReport Report;
	const TStrongObjectPtr<UGSTN_CombatTableSet> Set = Convert(Compiled, Settings, &Report);
	TestEqual(TEXT("one registry entry, the combat timeline"), Set->TimelineRegistry.Num(), 1);
	TestEqual(TEXT("it is the compiled one"), Set->TimelineRegistry[0].Get(), UGSTN_TestMontageTimelineAbility::StaticClass());
	TestEqual(TEXT("the kit's Alpha binding points at registry 0"), static_cast<int32>(Set->BindingTimeline[0]), 0);
	TestEqual(TEXT("the binding of the dropped entry is a plain ability"), static_cast<int32>(Set->BindingTimeline[1]), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("the plain Zeta binding has no timeline"), static_cast<int32>(Set->BindingTimeline[3]), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("the dropped registry entry is reported"), Report.Errors.Num(), 1);
	TestEqual(TEXT("all arrays are per binding"), Set->BindingTimeline.Num(), Set->Table.Bindings.Num());

	// The adoptable timelines follow the compiled ones
	const TSubclassOf<UGSTN_GA_BaseCombat> Adoptable[] = { UGSTN_TestMontageTimelineAbility::StaticClass() };
	Set->AppendAdoptable(Adoptable);
	TestEqual(TEXT("an adoptable class that is already registered is not added twice"), Set->TimelineRegistry.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTableTierJumpTest, "GSTNCombat.NP.Table.ConvertTierAndJumpRole", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTableTierJumpTest::RunTest(const FString& Parameters)
{
	FTblSettings Settings;
	const TStrongObjectPtr<UGSTN_CombatTableSet> Set = Convert(MakeCompiled(), Settings);

	const int32 Tier = Settings.Input->GetCommandTier(TblCommandAlpha.GetTag());
	TestEqual(TEXT("the registry entry takes the tier of the command bound to it"), Set->TimelineTier[0], Tier);
	TestEqual(TEXT("and its default window follows the tier"), Set->TimelineDefaultOverride[0], Settings.Combat->GetDefaultOverrideFrames(Tier));

	const UGSTN_GA_PlatformJump* Default = GetDefault<UGSTN_GA_PlatformJump>();
	TestEqual(TEXT("the jump binding carries its role"), static_cast<int32>(Set->BindingJumpRole[2]), static_cast<int32>(Default->Role) + 1);
	TestEqual(TEXT("a plain binding has none"), static_cast<int32>(Set->BindingJumpRole[0]), 0);
	TestEqual(TEXT("the table's jump data is set exactly when the role is Jump"), Set->TableJumpData != nullptr, Default->Role == EGSTN_PlatformJumpRole::Jump);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTableChecksumDeterministicTest, "GSTNCombat.NP.Table.ChecksumDeterministic", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTableChecksumDeterministicTest::RunTest(const FString& Parameters)
{
	FTblSettings Settings;
	const TStrongObjectPtr<UGSTN_CombatTableSet> A = Convert(MakeCompiled(), Settings);
	const TStrongObjectPtr<UGSTN_CombatTableSet> B = Convert(MakeCompiled(), Settings);
	TestEqual(TEXT("two conversions of the same table have the same checksum"), A->Checksum, B->Checksum);
	TestEqual(TEXT("and the stored one is the computed one"), A->Checksum, A->ComputeChecksum());
	TestNotEqual(TEXT("it is not trivially zero"), A->Checksum, 0u);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTableChecksumDiffersTest, "GSTNCombat.NP.Table.ChecksumDiffers", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTableChecksumDiffersTest::RunTest(const FString& Parameters)
{
	FTblSettings Settings;
	const uint32 Base = Convert(MakeCompiled(), Settings)->Checksum;

	FGSTN_CompiledBindingTable Source = MakeCompiled();
	Source.Entries[2].Source = EGSTN_EntrySource::Kit;
	Source.Entries[2].SourceIndex = 0;
	TestNotEqual(TEXT("another source"), Convert(Source, Settings)->Checksum, Base);

	FGSTN_CompiledBindingTable Ability = MakeCompiled();
	Ability.Entries[0].Ability = OtherPlainPath();
	TestNotEqual(TEXT("another ability class"), Convert(Ability, Settings)->Checksum, Base);

	TestNotEqual(TEXT("another revision"), Convert(MakeCompiled(4), Settings)->Checksum, Base);

	FGSTN_CombatTableConvertInput Input(&ResolveClass);
	Input.InputSettings = Settings.Input.Get();
	Input.CombatSettings = Settings.Combat.Get();
	Input.ReservedExtension = { 1, 2, 3 };
	const TStrongObjectPtr<UGSTN_CombatTableSet> WithBlock(FGSTN_CombatTableConverter::Convert(GetTransientPackage(), MakeCompiled(), Input));
	TestNotEqual(TEXT("a different reserved extension block"), WithBlock->Checksum, Base);

	FGSTN_CombatTableConvertInput Stats(&ResolveClass);
	Stats.InputSettings = Settings.Input.Get();
	Stats.CombatSettings = Settings.Combat.Get();
	Stats.WeaponStats.AddDefaulted();
	Stats.WeaponStats[0].Stats.AddDefaulted();
	Stats.WeaponStats[0].Stats[0].Value = 2.f;
	const TStrongObjectPtr<UGSTN_CombatTableSet> WithStats(FGSTN_CombatTableConverter::Convert(GetTransientPackage(), MakeCompiled(), Stats));
	TestNotEqual(TEXT("different weapon stats"), WithStats->Checksum, Base);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTableDropsUnloadableTest, "GSTNCombat.NP.Table.DropsUnloadableClass", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTableDropsUnloadableTest::RunTest(const FString& Parameters)
{
	FTblSettings Settings;
	FGSTN_CompiledBindingTable Compiled = MakeCompiled();
	Compiled.Entries[0].Ability = FSoftObjectPath(TEXT("/Script/GSTNCombatNP.GSTN_DoesNotExist"));
	Compiled.Entries[1].Source = EGSTN_EntrySource::Super;
	Compiled.Entries[1].SourceIndex = 9;	// no mask bit

	FGSTN_CompileReport Report;
	const TStrongObjectPtr<UGSTN_CombatTableSet> Set = Convert(Compiled, Settings, &Report);
	TestEqual(TEXT("both bad entries are reported"), Report.Errors.Num(), 2);
	TestEqual(TEXT("two bindings are left"), Set->Table.Bindings.Num(), 2);
	TestEqual(TEXT("and only Alpha is a command: the dropped Zeta binding takes its command with it"), Set->Table.Commands.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTableExtensionOrderTest, "GSTNCombat.NP.Table.ExtensionHookOrder", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTableExtensionOrderTest::RunTest(const FString& Parameters)
{
	FTblSettings Settings;
	TArray<FName> Order;
	FGSTN_CombatTableConverter::RegisterExtension(TEXT("GSTNTest.B"), [&Order](const FGSTN_CompiledBindingTable&, UGSTN_CombatTableSet&) { Order.Add(TEXT("B")); });
	FGSTN_CombatTableConverter::RegisterExtension(TEXT("GSTNTest.A"), [&Order](const FGSTN_CompiledBindingTable&, UGSTN_CombatTableSet& Set)
	{
		Order.Add(TEXT("A"));
		Set.ReservedExtension.Add(7);
	});

	const TStrongObjectPtr<UGSTN_CombatTableSet> Set = Convert(MakeCompiled(), Settings);
	FGSTN_CombatTableConverter::UnregisterExtension(TEXT("GSTNTest.A"));
	FGSTN_CombatTableConverter::UnregisterExtension(TEXT("GSTNTest.B"));

	TestEqual(TEXT("both ran"), Order.Num(), 2);
	TestTrue(TEXT("in id order, whatever the registration order"), Order.Num() == 2 && Order[0] == TEXT("A") && Order[1] == TEXT("B"));
	TestNotEqual(TEXT("what an extension changes is part of the checksum"), Set->Checksum, Convert(MakeCompiled(), Settings)->Checksum);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTableLimitsValidatorTest, "GSTNCombat.NP.Table.LimitsValidatorRefuses", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTableLimitsValidatorTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_LoadoutRules> Rules(NewObject<UGSTN_LoadoutRules>(GetTransientPackage()));
	auto CountLimitErrors = [&Rules](const FGSTN_LoadoutRulesView& View)
	{
		int32 Count = 0;
		for (const FText& Error : Rules->Validate(View).Errors)
		{
			Count += Error.ToString().Contains(TEXT("combat simulation holds")) ? 1 : 0;
		}
		return Count;
	};

	FGSTN_LoadoutRulesView View;
	View.Table.EntryCount = FGSTN_CommandTable::MaxBindings;
	TestEqual(TEXT("a table at the limit is accepted"), CountLimitErrors(View), 0);
	View.Table.EntryCount = FGSTN_CommandTable::MaxBindings + 1;
	TestEqual(TEXT("one binding too many is refused"), CountLimitErrors(View), 1);

	View.Table.EntryCount = 0;
	for (int32 Index = 0; Index <= FGSTN_CompiledBindingTable::MaxRegistryEntries; ++Index)
	{
		View.Table.RegistryAbilities.Add(FSoftObjectPath(FString::Printf(TEXT("/Script/GSTNCombatNP.Timeline%d"), Index)));
	}
	TestEqual(TEXT("a registry past its limit is refused"), CountLimitErrors(View), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPTableWeaponStatsTest, "GSTNCombat.NP.Table.WeaponStatsFromLoadout", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPTableWeaponStatsTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_WeaponDefinition> Primary(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));
	Primary->StatsEffect = TSoftClassPtr<UGSTN_GE_WeaponStats>(UGSTN_GE_WeaponStats::StaticClass());
	Primary->StatModifiers.AddDefaulted();
	Primary->StatModifiers[0].Value = 300.f;
	const TStrongObjectPtr<UGSTN_WeaponDefinition> Support(NewObject<UGSTN_WeaponDefinition>(GetTransientPackage()));

	FGSTN_ResolvedLoadout Resolved;
	Resolved.Weapons = { Primary.Get(), Support.Get(), nullptr };
	const TArray<FGSTN_WeaponStatsEntry> Entries = UGSTN_CombatSimComponent::BuildWeaponStats(Resolved);

	if (!TestEqual(TEXT("one entry per weapon, an unresolved one included"), Entries.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("the primary weapon's effect class"), Entries[0].Effect.Get(), UGSTN_GE_WeaponStats::StaticClass());
	TestEqual(TEXT("and its stat magnitudes"), Entries[0].Stats.Num(), 1);
	TestTrue(TEXT("a support weapon ships without a stats effect"), Entries[1].Effect == nullptr && Entries[1].Stats.IsEmpty());
	TestTrue(TEXT("an unresolved weapon is an empty entry"), Entries[2].Effect == nullptr);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
