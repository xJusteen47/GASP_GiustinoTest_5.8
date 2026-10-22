// Automation tests of the compiled binding table. Pure: abilities and timelines are opaque paths (engine classes stand in as
// keys, nothing is loaded), so no world, no ability system and no content asset is involved.

#include "Tests/GSTN_CombatTestUtils.h"
#include "Input/GSTN_StandardSlots.h"
#include "Kits/GSTN_SuperInputScheme.h"
#include "Loadout/GSTN_CompiledBindingTable.h"
#include "Input/GSTN_InputTags.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Opaque keys: the compiler never loads them, so any class path serves. Abilities and timeline overrides are told apart by
	// their paths only, which sort as: Enum < Function < Object < Struct and Class < Package
	FSoftObjectPath TblAbilityA() { return FSoftObjectPath(UObject::StaticClass()); }
	FSoftObjectPath TblAbilityB() { return FSoftObjectPath(UFunction::StaticClass()); }
	FSoftObjectPath TblAbilityC() { return FSoftObjectPath(UStruct::StaticClass()); }
	FSoftObjectPath TblAbilityD() { return FSoftObjectPath(UEnum::StaticClass()); }
	FSoftObjectPath TblOverride1() { return FSoftObjectPath(UClass::StaticClass()); }
	FSoftObjectPath TblOverride2() { return FSoftObjectPath(UPackage::StaticClass()); }

	FGSTN_BindingInput TblBind(const FGameplayTag& Command, const FSoftObjectPath& Ability, const FGameplayTag& Require = FGameplayTag(), int32 PriorityOffset = 0, const FSoftObjectPath& Override = FSoftObjectPath())
	{
		FGSTN_BindingInput Binding;
		Binding.Command = Command;
		Binding.Ability = Ability;
		Binding.TimelineOverride = Override;
		Binding.PriorityOffset = PriorityOffset;
		if (Require.IsValid())
		{
			Binding.Requirements.RequireTags.AddTag(Require);
		}
		return Binding;
	}

	FGameplayTag TblGrounded() { return GSTNInputTags::Context_Grounded.GetTag(); }
	FGameplayTag TblAirborne() { return GSTNInputTags::Context_Airborne.GetTag(); }

	/** The plugin's standard command set as a set asset, to be the kit's base */
	TStrongObjectPtr<UGSTN_InputSlotSet> TblMakeStandardSet()
	{
		TStrongObjectPtr<UGSTN_InputSlotSet> Set(NewObject<UGSTN_InputSlotSet>(GetTransientPackage()));
		for (const FGSTN_InputSlotSpec& Spec : UGSTN_StandardSlots::Build())
		{
			FGSTN_InputSlotEntry& Entry = Set->Slots.AddDefaulted_GetRef();
			Entry.Inline = Spec;
		}
		return Set;
	}

	FGSTN_InputSlotEntry TblMakeSlot(const FGameplayTag& Command, int32 PriorityOffset = 0)
	{
		FGSTN_InputSlotEntry Entry;
		Entry.Inline.Command = Command;
		Entry.Inline.PriorityOffset = PriorityOffset;
		return Entry;
	}

	/** "Source/Index Command" of each row, to compare the order in one line */
	TArray<FString> TblDescribe(const FGSTN_CompiledBindingTable& Table)
	{
		TArray<FString> Rows;
		for (const FGSTN_CompiledEntry& Entry : Table.Entries)
		{
			const TCHAR* Source = Entry.Source == EGSTN_EntrySource::Kit ? TEXT("Kit") : (Entry.Source == EGSTN_EntrySource::Weapon ? TEXT("Weapon") : TEXT("Super"));
			Rows.Add(FString::Printf(TEXT("%s/%d %s"), Source, Entry.SourceIndex, *Entry.Command.ToString()));
		}
		return Rows;
	}

	FString TblRow(const TCHAR* Source, int32 Index, const FGameplayTag& Command)
	{
		return FString::Printf(TEXT("%s/%d %s"), Source, Index, *Command.ToString());
	}

	int32 TblCountContaining(const TArray<FText>& Messages, const TCHAR* Needle)
	{
		int32 Count = 0;
		for (const FText& Message : Messages)
		{
			Count += Message.ToString().Contains(Needle) ? 1 : 0;
		}
		return Count;
	}

	/** A compile input with the standard set as the kit's base */
	FGSTN_CompileInput TblMakeInput(const UGSTN_InputSlotSet* Base)
	{
		FGSTN_CompileInput Input;
		Input.Kit.BaseSet = Base;
		return Input;
	}

	/** One input that touches every part of the table: both sources of rows, timeline rows, customisation and super slots */
	FGSTN_CompileInput TblMakeFullInput(const UGSTN_InputSlotSet* Base, const UGSTN_SuperInputScheme* Scheme)
	{
		using namespace GSTNCommandTags;
		FGSTN_CompileInput Input = TblMakeInput(Base);
		Input.Revision = 4;
		Input.Kit.SuperSlotCount = 2;
		Input.SuperScheme = Scheme;
		Input.Kit.Bindings = { TblBind(Strong_Neutral, TblAbilityB()), TblBind(Attack_Neutral, TblAbilityA(), TblGrounded(), 0, TblOverride1()), TblBind(Attack_Neutral, TblAbilityC(), TblAirborne(), 2) };
		Input.Kit.TriggeredAbilities = { TblAbilityD() };
		Input.Weapons.SetNum(2);
		Input.Weapons[0].Bindings = { TblBind(Special_Neutral, TblAbilityB(), FGameplayTag(), 1) };
		Input.Weapons[1].Bindings = { TblBind(Attack_Directional, TblAbilityC(), FGameplayTag(), 0, TblOverride2()), TblBind(Attack_Neutral, TblAbilityA()) };
		FGSTN_SlotCustomisation Customisation;
		Customisation.WeaponIndex = 1;
		Customisation.Command = Attack_Neutral.GetTag();
		Customisation.AlternativeAbility = TSoftClassPtr<UObject>(TblAbilityD());
		Input.Customisations = { Customisation };
		Input.Supers.SetNum(2);
		Input.Supers[1].bPresent = true;
		Input.Supers[1].Ability = TblAbilityB();
		Input.Supers[1].Requirements.RequireTags.AddTag(TblGrounded());
		return Input;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TableCompileOrderTest, "GSTNCombat.Core.Table.CompileOrder", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TableCompileOrderTest::RunTest(const FString& Parameters)
{
	using namespace GSTNCommandTags;
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNCombatTest::MakeSettings();
	const TStrongObjectPtr<UGSTN_InputSlotSet> Base = TblMakeStandardSet();

	// Declared out of order on purpose; the same command twice keeps its listed order
	FGSTN_CompileInput Input = TblMakeInput(Base.Get());
	Input.Kit.Bindings = { TblBind(Strong_Neutral, TblAbilityA()), TblBind(Attack_Neutral, TblAbilityA(), TblGrounded()), TblBind(Attack_Neutral, TblAbilityB(), TblAirborne()), TblBind(Attack_Directional, TblAbilityA()) };
	Input.Weapons.SetNum(2);
	Input.Weapons[0].Bindings = { TblBind(Special_Neutral, TblAbilityB()) };
	Input.Weapons[1].Bindings = { TblBind(Attack_Directional, TblAbilityC()), TblBind(Attack_CrouchNeutral, TblAbilityC()) };

	FGSTN_CompileReport Report;
	const FGSTN_CompiledBindingTable Table = FGSTN_BindingTableCompiler::Compile(Input, *Settings, &Report);
	TestEqual(TEXT("a clean compile reports no error"), Report.Errors.Num(), 0);

	// Kit rows, then weapon 0, then weapon 1; by command name inside each ("Attack.Directional" < "Attack.Neutral" < "Strong.Neutral")
	const TArray<FString> Expected = {
		TblRow(TEXT("Kit"), 0, Attack_Directional.GetTag()), TblRow(TEXT("Kit"), 0, Attack_Neutral.GetTag()), TblRow(TEXT("Kit"), 0, Attack_Neutral.GetTag()), TblRow(TEXT("Kit"), 0, Strong_Neutral.GetTag()),
		TblRow(TEXT("Weapon"), 0, Special_Neutral.GetTag()),
		TblRow(TEXT("Weapon"), 1, Attack_CrouchNeutral.GetTag()), TblRow(TEXT("Weapon"), 1, Attack_Directional.GetTag()) };
	TestTrue(TEXT("kit, weapon 0, weapon 1, each by command name"), TblDescribe(Table) == Expected);

	if (TestEqual(TEXT("seven rows"), Table.Entries.Num(), 7))
	{
		TestTrue(TEXT("rows of one command keep their listed order: grounded first"), Table.Entries[1].Requirements.RequireTags.HasTagExact(TblGrounded()));
		TestTrue(TEXT("then airborne"), Table.Entries[2].Requirements.RequireTags.HasTagExact(TblAirborne()));
		TestEqual(TEXT("rows of one command share its slot"), Table.Entries[1].SlotIndex, Table.Entries[2].SlotIndex);
		TestEqual(TEXT("so does the same command from another source"), Table.Entries[0].SlotIndex, Table.Entries[6].SlotIndex);
	}
	TestEqual(TEXT("one pooled slot per command: five commands"), Table.Slots.Num(), 5);
	TestEqual(TEXT("no timeline rows without overrides or a predicate"), Table.Registry.Num(), 0);
	TestEqual(TEXT("the table carries the weapon count"), Table.WeaponCount, 2);

	// Priority is the effective slot's rank, from the settings
	if (Table.Entries.Num() == 7)
	{
		TestTrue(TEXT("the stored rank is the effective slot's"), Table.Entries[0].Priority == Table.MakeEffectiveSlot(Table.Entries[0]).GetPriority(*Settings));
		TestTrue(TEXT("a strong outranks an attack (tiers)"), Table.Entries[3].Priority.Compare(Table.Entries[0].Priority) > 0);
	}

	// A binding that binds nothing yields no row (it only hides a parent's)
	Input.Kit.Bindings.Add(TblBind(Special_Directional, FSoftObjectPath()));
	TestEqual(TEXT("an empty binding makes no row"), FGSTN_BindingTableCompiler::Compile(Input, *Settings).Entries.Num(), 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TableCommandSetTest, "GSTNCombat.Core.Table.CommandSetBaseMinusRemovedPlusAdded", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TableCommandSetTest::RunTest(const FString& Parameters)
{
	using namespace GSTNCommandTags;
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNCombatTest::MakeSettings();
	const TStrongObjectPtr<UGSTN_InputSlotSet> Base = TblMakeStandardSet();
	const FGameplayTag Added = FGSTN_BindingTableCompiler::GetSuperSlotCommand(0);
	const FGameplayTag NotInSet = FGSTN_BindingTableCompiler::GetSuperSlotCommand(1);

	// The kit drops the directional attack, adds a command of its own and replaces the special's slot with its version
	FGSTN_CompileInput Input = TblMakeInput(Base.Get());
	Input.Kit.CommandSet.RemovedCommands.AddTag(Attack_Directional.GetTag());
	Input.Kit.CommandSet.AddedSlots = { TblMakeSlot(Added, 7), TblMakeSlot(Special_Neutral.GetTag(), 9) };
	Input.Kit.Bindings = { TblBind(Attack_Directional, TblAbilityA()), TblBind(Attack_Neutral, TblAbilityA()), TblBind(Added, TblAbilityB()), TblBind(Special_Neutral, TblAbilityC(), FGameplayTag(), 5) };

	// A weapon speaks the kit's commands: a removed one, one the set lacks, and the same one with its own slot
	Input.Weapons.SetNum(1);
	FGSTN_BindingInput WithOverride = TblBind(NotInSet, TblAbilityD());
	WithOverride.bHasSlotOverride = true;
	WithOverride.SlotOverride = TblMakeSlot(NotInSet, 3).Inline;
	Input.Weapons[0].Bindings = { TblBind(Attack_Directional, TblAbilityB()), TblBind(NotInSet, TblAbilityB()), WithOverride };

	FGSTN_CompileReport Report;
	const FGSTN_CompiledBindingTable Table = FGSTN_BindingTableCompiler::Compile(Input, *Settings, &Report);

	TestEqual(TEXT("both bindings to the removed command are reported"), TblCountContaining(Report.Errors, TEXT("removes")), 2);
	TestEqual(TEXT("the weapon binding with no slot is reported"), TblCountContaining(Report.Errors, TEXT("not in the kit's command set")), 1);
	TestEqual(TEXT("three problems in all"), Report.Errors.Num(), 3);

	// Rows: kit Attack.Neutral, Command.Super.Slot1 (added), Special.Neutral (replaced); the weapon's override row
	const TArray<FString> Expected = { TblRow(TEXT("Kit"), 0, Attack_Neutral.GetTag()), TblRow(TEXT("Kit"), 0, Special_Neutral.GetTag()), TblRow(TEXT("Kit"), 0, Added), TblRow(TEXT("Weapon"), 0, NotInSet) };
	TestTrue(TEXT("removed and unplaceable bindings make no row"), TblDescribe(Table) == Expected);
	if (Table.Entries.Num() == 4)
	{
		TestEqual(TEXT("the replaced slot is the kit's version, plus the binding's own offset"), Table.MakeEffectiveSlot(Table.Entries[1]).PriorityOffset, 9 + 5);
		TestEqual(TEXT("the added slot is in the set"), Table.MakeEffectiveSlot(Table.Entries[2]).PriorityOffset, 7);
		TestEqual(TEXT("the override slot is used as is"), Table.MakeEffectiveSlot(Table.Entries[3]).PriorityOffset, 3);
	}

	// A set with no base is only what the kit adds
	FGSTN_CompileInput NoBase = TblMakeInput(nullptr);
	NoBase.Kit.CommandSet.AddedSlots = { TblMakeSlot(Added, 7) };
	NoBase.Kit.Bindings = { TblBind(Attack_Neutral, TblAbilityA()), TblBind(Added, TblAbilityB()) };
	FGSTN_CompileReport NoBaseReport;
	const FGSTN_CompiledBindingTable NoBaseTable = FGSTN_BindingTableCompiler::Compile(NoBase, *Settings, &NoBaseReport);
	TestEqual(TEXT("without a base only the added command has a row"), NoBaseTable.Entries.Num(), 1);
	TestEqual(TEXT("and the other binding is reported"), NoBaseReport.Errors.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TableRegistryTest, "GSTNCombat.Core.Table.RegistryDedupeAndSort", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TableRegistryTest::RunTest(const FString& Parameters)
{
	using namespace GSTNCommandTags;
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNCombatTest::MakeSettings();
	const TStrongObjectPtr<UGSTN_InputSlotSet> Base = TblMakeStandardSet();

	// Without a predicate a row with an override is a timeline row. Listed out of order, with a repeated pair
	FGSTN_CompileInput Input = TblMakeInput(Base.Get());
	Input.Kit.Bindings = {
		TblBind(Special_Neutral, TblAbilityC(), FGameplayTag(), 0, TblOverride1()),
		TblBind(Strong_Neutral, TblAbilityA(), FGameplayTag(), 0, TblOverride2()),
		TblBind(Attack_Neutral, TblAbilityA(), TblGrounded(), 0, TblOverride1()),
		TblBind(Attack_Neutral, TblAbilityA(), TblAirborne(), 0, TblOverride1()),
		TblBind(Attack_Directional, TblAbilityB()) };

	const FGSTN_CompiledBindingTable Table = FGSTN_BindingTableCompiler::Compile(Input, *Settings);
	if (TestEqual(TEXT("three distinct pairs"), Table.Registry.Num(), 3))
	{
		TestTrue(TEXT("sorted by ability, then override"),
			Table.Registry[0].Ability == TblAbilityA() && Table.Registry[0].TimelineOverride == TblOverride1()
			&& Table.Registry[1].Ability == TblAbilityA() && Table.Registry[1].TimelineOverride == TblOverride2()
			&& Table.Registry[2].Ability == TblAbilityC() && Table.Registry[2].TimelineOverride == TblOverride1());
	}
	for (const FGSTN_CompiledEntry& Entry : Table.Entries)
	{
		const int32 Expected = Entry.Ability == TblAbilityB() ? INDEX_NONE : Table.FindRegistryIndex(Entry.Ability, Entry.Command == Strong_Neutral.GetTag() ? TblOverride2() : TblOverride1());
		TestEqual(*FString::Printf(TEXT("the row of %s carries its registry index"), *Entry.Command.ToString()), static_cast<int32>(Entry.TimelineIndex), Expected);
	}
	const FGSTN_CompiledEntry* FirstAttack = Table.Entries.FindByPredicate([](const FGSTN_CompiledEntry& E) { return E.Command == GSTNCommandTags::Attack_Neutral.GetTag(); });
	TestTrue(TEXT("the two rows of one pair share an index"), FirstAttack && FirstAttack->TimelineIndex == 0);

	// With a predicate the kit's triggered abilities and passives join, without an override, deduplicated
	FGSTN_CompileInput WithPredicate = Input;
	WithPredicate.IsTimelineAbility = [](const FSoftObjectPath& Path) { return Path != TblAbilityB(); };
	WithPredicate.Kit.TriggeredAbilities = { TblAbilityD(), TblAbilityD(), TblAbilityB() };
	WithPredicate.Kit.Passives = { TblAbilityD() };
	const FGSTN_CompiledBindingTable Predicated = FGSTN_BindingTableCompiler::Compile(WithPredicate, *Settings);
	if (TestEqual(TEXT("the triggered timeline ability joins once"), Predicated.Registry.Num(), 4))
	{
		TestTrue(TEXT("and sorts first (Enum before Object)"), Predicated.Registry[0].Ability == TblAbilityD() && !Predicated.Registry[0].TimelineOverride.IsValid());
	}
	TestTrue(TEXT("a plain ability stays out"), Predicated.FindRegistryIndex(TblAbilityB(), FSoftObjectPath()) == INDEX_NONE);
	TestTrue(TEXT("the grant list holds the rows' abilities and the kit's, once each, sorted"), Predicated.GrantedAbilities == TArray<FSoftObjectPath>({ TblAbilityD(), TblAbilityB(), TblAbilityA(), TblAbilityC() }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TableCustomisationTest, "GSTNCombat.Core.Table.Customisation", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TableCustomisationTest::RunTest(const FString& Parameters)
{
	using namespace GSTNCommandTags;
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNCombatTest::MakeSettings();
	const TStrongObjectPtr<UGSTN_InputSlotSet> Base = TblMakeStandardSet();

	FGSTN_CompileInput Input = TblMakeInput(Base.Get());
	Input.Weapons.SetNum(2);
	Input.Weapons[0].Bindings = { TblBind(Attack_Neutral, TblAbilityA()) };
	Input.Weapons[1].Bindings = { TblBind(Attack_Neutral, TblAbilityA(), TblGrounded(), 0, TblOverride1()), TblBind(Attack_Neutral, TblAbilityA(), TblAirborne(), 0, TblOverride1()), TblBind(Strong_Neutral, TblAbilityC()) };

	const auto Customise = [](int32 Weapon, const FGameplayTag& Command, const FSoftObjectPath& Alternative)
	{
		FGSTN_SlotCustomisation Customisation;
		Customisation.WeaponIndex = Weapon;
		Customisation.Command = Command;
		Customisation.AlternativeAbility = TSoftClassPtr<UObject>(Alternative);
		return Customisation;
	};

	// Weapon 1's attack is swapped in both its contexts; weapon 0's and the strong stay. Three customisations are of nothing
	Input.Customisations = { Customise(1, Attack_Neutral.GetTag(), TblAbilityB()), Customise(7, Attack_Neutral.GetTag(), TblAbilityB()), Customise(0, Special_Neutral.GetTag(), TblAbilityB()), Customise(0, Attack_Neutral.GetTag(), FSoftObjectPath()) };

	FGSTN_CompileReport Report;
	const FGSTN_CompiledBindingTable Table = FGSTN_BindingTableCompiler::Compile(Input, *Settings, &Report);
	TestEqual(TEXT("customisations of a missing weapon, of an unbound command and of no ability are warnings"), Report.Warnings.Num(), 3);
	TestEqual(TEXT("and no error"), Report.Errors.Num(), 0);

	if (TestEqual(TEXT("four rows"), Table.Entries.Num(), 4))
	{
		// Weapon 0 first, then weapon 1 by command: Attack.Neutral x2, Strong.Neutral
		TestTrue(TEXT("weapon 0 keeps its ability"), Table.Entries[0].Ability == TblAbilityA());
		TestTrue(TEXT("weapon 1's grounded attack is swapped"), Table.Entries[1].Ability == TblAbilityB());
		TestTrue(TEXT("and its airborne attack"), Table.Entries[2].Ability == TblAbilityB());
		TestTrue(TEXT("the strong is untouched"), Table.Entries[3].Ability == TblAbilityC());
		TestTrue(TEXT("the slot, requirements and override of a swapped row stay"), Table.Entries[1].SlotIndex == Table.Entries[0].SlotIndex && Table.Entries[1].Requirements.RequireTags.HasTagExact(TblGrounded()));
		TestTrue(TEXT("the registry follows the swap: the pair is (alternative, override)"), Table.Registry.Num() == 1 && Table.Registry[0].Ability == TblAbilityB() && Table.Registry[0].TimelineOverride == TblOverride1());
		TestEqual(TEXT("the swapped rows point at it"), static_cast<int32>(Table.Entries[1].TimelineIndex), 0);
		TestEqual(TEXT("an untouched plain row has no timeline"), static_cast<int32>(Table.Entries[0].TimelineIndex), static_cast<int32>(INDEX_NONE));
	}
	TestTrue(TEXT("the replaced ability is still granted through weapon 0"), Table.GrantedAbilities.Contains(TblAbilityA()));
	TestTrue(TEXT("the alternative is granted"), Table.GrantedAbilities.Contains(TblAbilityB()));

	// Without the customisation the swap does not happen: the loadout, not the weapon, chose it
	Input.Customisations.Reset();
	const FGSTN_CompiledBindingTable Plain = FGSTN_BindingTableCompiler::Compile(Input, *Settings);
	TestTrue(TEXT("no customisation, no swap"), Plain.Entries.Num() == 4 && Plain.Entries[1].Ability == TblAbilityA());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TableSuperCodeSlotsTest, "GSTNCombat.Core.Table.SuperCodeSlots", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TableSuperCodeSlotsTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNCombatTest::MakeSettings();
	const FGameplayTag Slot1 = FGSTN_BindingTableCompiler::GetSuperSlotCommand(0);
	const FGameplayTag Slot2 = FGSTN_BindingTableCompiler::GetSuperSlotCommand(1);
	TestTrue(TEXT("the slot commands are command tags under the super root"), Slot1.IsValid() && Slot1.MatchesTag(GSTNCommandTags::Super.GetTag()) && Slot2 != Slot1);
	TestFalse(TEXT("there is no ninth slot command"), FGSTN_BindingTableCompiler::GetSuperSlotCommand(FGSTN_BindingTableCompiler::MaxSuperSlots).IsValid());

	const auto MakeSuper = [](const FSoftObjectPath& Ability, int32 CodeSteps)
	{
		FGSTN_SuperCompileInput Super;
		Super.bPresent = true;
		Super.Ability = Ability;
		Super.InputCode.Steps.AddDefaulted(CodeSteps);
		Super.Requirements.RequireTags.AddTag(TblGrounded());
		return Super;
	};

	// Fixed slots: the scheme's slot is the super slot's input. The first names no command, so the slot's own is used
	{
		TStrongObjectPtr<UGSTN_SuperInputScheme> Scheme(NewObject<UGSTN_SuperInputScheme>(GetTransientPackage()));
		Scheme->Mode = EGSTN_SuperInputMode::FixedSlots;
		Scheme->FixedSlots.SetNum(2);
		Scheme->FixedSlots[0].PriorityOffset = 11;
		Scheme->FixedSlots[1].Command = Slot2;

		FGSTN_CompileInput Input;
		Input.Kit.SuperSlotCount = 3;
		Input.SuperScheme = Scheme.Get();
		Input.Supers = { MakeSuper(TblAbilityA(), 0), FGSTN_SuperCompileInput(), MakeSuper(TblAbilityB(), 0) };

		FGSTN_CompileReport Report;
		const FGSTN_CompiledBindingTable Table = FGSTN_BindingTableCompiler::Compile(Input, *Settings, &Report);
		TestEqual(TEXT("the third super has no fixed slot: one error"), Report.Errors.Num(), 1);
		if (TestEqual(TEXT("one row: the empty slot and the unplaceable one make none"), Table.Entries.Num(), 1))
		{
			const FGSTN_CompiledEntry& Entry = Table.Entries[0];
			TestTrue(TEXT("a super row"), Entry.Source == EGSTN_EntrySource::Super && Entry.SourceIndex == 0);
			TestTrue(TEXT("the slot's command defaults to the super slot's"), Entry.Command == Slot1);
			TestEqual(TEXT("the fixed slot's input is used"), Table.MakeEffectiveSlot(Entry).PriorityOffset, 11);
			TestTrue(TEXT("the super's requirements are the row's"), Entry.Requirements.RequireTags.HasTagExact(TblGrounded()));
			TestTrue(TEXT("its ability"), Entry.Ability == TblAbilityA());
		}
	}

	// Per super code: the scheme's entry steps, then the super's own, under the super slot's command
	{
		TStrongObjectPtr<UGSTN_SuperInputScheme> Scheme(NewObject<UGSTN_SuperInputScheme>(GetTransientPackage()));
		Scheme->Mode = EGSTN_SuperInputMode::PerSuperCode;
		Scheme->CodeEntry.PriorityOffset = 3;
		Scheme->CodeEntry.Command = GSTNCommandTags::Attack_Neutral.GetTag();
		Scheme->CodeEntry.Pattern.Steps.AddDefaulted(1);

		FGSTN_CompileInput Input;
		Input.Kit.SuperSlotCount = 2;
		Input.SuperScheme = Scheme.Get();
		Input.Supers = { MakeSuper(TblAbilityA(), 2), MakeSuper(TblAbilityB(), 1), MakeSuper(TblAbilityC(), 1) };

		FGSTN_CompileReport Report;
		const FGSTN_CompiledBindingTable Table = FGSTN_BindingTableCompiler::Compile(Input, *Settings, &Report);
		TestEqual(TEXT("the super beyond the kit's slots is a warning"), Report.Warnings.Num(), 1);
		if (TestEqual(TEXT("two rows"), Table.Entries.Num(), 2))
		{
			TestTrue(TEXT("slot 1's command is Slot1, not the entry template's"), Table.Entries[0].Command == Slot1);
			TestTrue(TEXT("slot 2's"), Table.Entries[1].Command == Slot2);
			const FGSTN_InputSlotSpec First = Table.MakeEffectiveSlot(Table.Entries[0]);
			const FGSTN_InputSlotSpec Second = Table.MakeEffectiveSlot(Table.Entries[1]);
			TestEqual(TEXT("template step + two code steps"), First.Pattern.Steps.Num(), 3);
			TestEqual(TEXT("template step + one code step"), Second.Pattern.Steps.Num(), 2);
			TestEqual(TEXT("the template's offset is kept"), First.PriorityOffset, 3);
			TestTrue(TEXT("both commands are in the slot pool, one slot each"), First.Command == Slot1 && Second.Command == Slot2 && Table.Slots.Num() == 2);
		}
	}

	// A filled super with no scheme cannot be entered
	{
		FGSTN_CompileInput Input;
		Input.Kit.SuperSlotCount = 1;
		Input.Supers = { MakeSuper(TblAbilityA(), 1) };
		FGSTN_CompileReport Report;
		TestEqual(TEXT("no scheme, no row"), FGSTN_BindingTableCompiler::Compile(Input, *Settings, &Report).Entries.Num(), 0);
		TestEqual(TEXT("and an error"), Report.Errors.Num(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TableDeterministicBytesTest, "GSTNCombat.Core.Table.DeterministicBytes", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TableDeterministicBytesTest::RunTest(const FString& Parameters)
{
	using namespace GSTNCommandTags;
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNCombatTest::MakeSettings();
	const TStrongObjectPtr<UGSTN_InputSlotSet> Base = TblMakeStandardSet();
	TStrongObjectPtr<UGSTN_SuperInputScheme> Scheme(NewObject<UGSTN_SuperInputScheme>(GetTransientPackage()));
	Scheme->Mode = EGSTN_SuperInputMode::PerSuperCode;
	Scheme->CodeEntry.Pattern.Steps.AddDefaulted(1);

	const FGSTN_CompileInput Input = TblMakeFullInput(Base.Get(), Scheme.Get());

	// The same input twice is the same bytes
	TArray<uint8> First;
	TArray<uint8> Second;
	FGSTN_BindingTableCompiler::Compile(Input, *Settings).WriteBytes(First);
	FGSTN_BindingTableCompiler::Compile(Input, *Settings).WriteBytes(Second);
	TestTrue(TEXT("two compiles of one input are byte-identical"), First.Num() > 0 && First == Second);
	AddInfo(FString::Printf(TEXT("the full table is %d bytes"), First.Num()));

	// The order of bindings of different commands does not change the table (rows of one command keep their listed order)
	FGSTN_CompileInput Reordered = Input;
	Reordered.Kit.Bindings = { Input.Kit.Bindings[1], Input.Kit.Bindings[2], Input.Kit.Bindings[0] };
	TArray<uint8> ReorderedBytes;
	FGSTN_BindingTableCompiler::Compile(Reordered, *Settings).WriteBytes(ReorderedBytes);
	TestTrue(TEXT("bindings of different commands in another order compile to the same bytes"), ReorderedBytes == First);

	// Requirements are sets: the order their tags were added in does not change the bytes
	FGSTN_CompileInput TagsForward = Input;
	TagsForward.Kit.Bindings[0].Requirements.RequireTags.AddTag(TblGrounded());
	TagsForward.Kit.Bindings[0].Requirements.RequireTags.AddTag(TblAirborne());
	FGSTN_CompileInput TagsBackward = Input;
	TagsBackward.Kit.Bindings[0].Requirements.RequireTags.AddTag(TblAirborne());
	TagsBackward.Kit.Bindings[0].Requirements.RequireTags.AddTag(TblGrounded());
	TArray<uint8> ForwardBytes;
	TArray<uint8> BackwardBytes;
	FGSTN_BindingTableCompiler::Compile(TagsForward, *Settings).WriteBytes(ForwardBytes);
	FGSTN_BindingTableCompiler::Compile(TagsBackward, *Settings).WriteBytes(BackwardBytes);
	TestTrue(TEXT("tags added in another order are the same bytes"), ForwardBytes == BackwardBytes);
	TestTrue(TEXT("and a requirement is part of the bytes"), ForwardBytes != First);

	// And a different input is different bytes
	FGSTN_CompileInput Revised = Input;
	Revised.Revision += 1;
	TArray<uint8> RevisedBytes;
	FGSTN_BindingTableCompiler::Compile(Revised, *Settings).WriteBytes(RevisedBytes);
	TestTrue(TEXT("another revision is other bytes"), RevisedBytes != First);

	FGSTN_CompileInput Swapped = Input;
	Swapped.Customisations.Reset();
	TArray<uint8> SwappedBytes;
	FGSTN_BindingTableCompiler::Compile(Swapped, *Settings).WriteBytes(SwappedBytes);
	TestTrue(TEXT("another customisation is other bytes"), SwappedBytes != First);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TableAmbiguousConflictTest, "GSTNCombat.Core.Table.AmbiguousConflict", GSTN_CORE_TEST_FLAGS)
bool FGSTN_TableAmbiguousConflictTest::RunTest(const FString& Parameters)
{
	using namespace GSTNCommandTags;
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = GSTNCombatTest::MakeSettings();
	const TStrongObjectPtr<UGSTN_InputSlotSet> Base = TblMakeStandardSet();

	// Bindings: grounded and airborne versions of one command. Without the exclusive group they could hold together and
	// require the same number of tags, which is the M1 validator's "arbitrary" error
	FGSTN_CompileInput Input = TblMakeInput(Base.Get());
	Input.Kit.Bindings = { TblBind(Attack_Neutral, TblAbilityA(), TblGrounded()), TblBind(Attack_Neutral, TblAbilityB(), TblAirborne()) };

	Settings->ExclusiveContextGroups.Reset();
	{
		const FGSTN_CompiledBindingTable Table = FGSTN_BindingTableCompiler::Compile(Input, *Settings);
		FGSTN_CompileReport Report;
		FGSTN_BindingTableCompiler::FindConflicts(Table, *Settings, Report);
		TestEqual(TEXT("without a group the pair is ambiguous"), TblCountContaining(Report.Errors, TEXT("arbitrary")), 1);
	}

	FGSTN_ExclusiveTagGroup Group;
	Group.Tags.AddTag(TblGrounded());
	Group.Tags.AddTag(TblAirborne());
	Settings->ExclusiveContextGroups = { Group };
	{
		const FGSTN_CompiledBindingTable Table = FGSTN_BindingTableCompiler::Compile(Input, *Settings);
		FGSTN_CompileReport Report;
		FGSTN_BindingTableCompiler::FindConflicts(Table, *Settings, Report);
		TestEqual(TEXT("with the group Grounded and Airborne never apply together"), Report.Errors.Num(), 0);
	}

	// A binding that needs both members of the group can never apply
	{
		FGSTN_CompileInput Impossible = Input;
		Impossible.Kit.Bindings[0].Requirements.RequireTags.AddTag(TblAirborne());
		FGSTN_CompileReport Report;
		FGSTN_BindingTableCompiler::FindConflicts(FGSTN_BindingTableCompiler::Compile(Impossible, *Settings), *Settings, Report);
		TestEqual(TEXT("a binding requiring two members of a group is an error"), TblCountContaining(Report.Errors, TEXT("exclusive context group")), 1);
	}

	// Slots: two commands with the same input at the same rank are ambiguous even when each is bound once
	{
		FGSTN_CompileInput Slots = TblMakeInput(Base.Get());
		FGSTN_InputSlotEntry Twin;
		Twin.Inline = UGSTN_StandardSlots::Build()[0];
		Twin.Inline.Command = Attack.GetTag();
		Slots.Kit.CommandSet.AddedSlots = { Twin };
		Slots.Kit.Bindings = { TblBind(Attack_Neutral, TblAbilityA()), TblBind(Attack.GetTag(), TblAbilityB()) };
		FGSTN_CompileReport Report;
		FGSTN_BindingTableCompiler::FindConflicts(FGSTN_BindingTableCompiler::Compile(Slots, *Settings), *Settings, Report);
		TestEqual(TEXT("two slots with one input and one rank are ambiguous"), TblCountContaining(Report.Errors, TEXT("ambiguous")), 1);
	}

	// A table with a weapon is checked once per weapon context, and a repeated finding is reported once
	{
		FGSTN_CompileInput TwoWeapons = Input;
		Settings->ExclusiveContextGroups.Reset();
		TwoWeapons.Weapons.SetNum(2);
		TwoWeapons.Weapons[0].Bindings = { TblBind(Strong_Neutral, TblAbilityA()) };
		TwoWeapons.Weapons[1].Bindings = { TblBind(Special_Neutral, TblAbilityA()) };
		FGSTN_CompileReport Report;
		FGSTN_BindingTableCompiler::FindConflicts(FGSTN_BindingTableCompiler::Compile(TwoWeapons, *Settings), *Settings, Report);
		TestEqual(TEXT("the kit's conflict is found in both weapon contexts but reported with each context's own index"), TblCountContaining(Report.Errors, TEXT("arbitrary")), 2);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
