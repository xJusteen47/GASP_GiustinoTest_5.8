// Automation tests of per-entry timelines (M11 L5b): a registry entry with a TimelineOverride (a binding's own timeline asset, or a kit
// mod's derived transient timeline) runs that data in the simulation, not its class default's. Pure: the class loader and the timeline
// loader are injected, the timeline logic is the one the combat tick calls.

#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_CombatSettings.h"
#include "Input/GSTN_InputSettings.h"
#include "Loadout/GSTN_EntrySourceMask.h"
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "Simulation/GSTN_CombatTableSet.h"
#include "Simulation/GSTN_CombatTimelineLogic.h"
#include "Tests/GSTN_ReviewTestTypes.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ModTlCommandBase, "Command.NPModTimeline.Base", "Test-only tag of the GSTNCombatNP mod timeline automation tests");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(ModTlCommandModded, "Command.NPModTimeline.Modded", "Test-only tag of the GSTNCombatNP mod timeline automation tests");

	FSoftObjectPath TimelineClassPath() { return FSoftObjectPath(UGSTN_TestMontageTimelineAbility::StaticClass()); }

	UClass* ResolveModTimelineClass(const FSoftObjectPath& Path)
	{
		return Cast<UClass>(Path.ResolveObject());
	}

	/** Frames scaled like a "slower" mod: total, first actionable frame and the override window all by Scale */
	UGSTN_CombatTimeline* MakeTimelineAsset(const TCHAR* Name, float Scale)
	{
		UGSTN_CombatTimeline* Asset = NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), MakeUniqueObjectName(GetTransientPackage(), UGSTN_CombatTimeline::StaticClass(), Name), RF_Transient);
		Asset->Data.TotalFrames = FMath::RoundToInt(30 * Scale);
		Asset->Data.FirstActionableFrame = FMath::RoundToInt(20 * Scale);
		Asset->Data.OverrideFrames = FMath::RoundToInt(2 * Scale);
		return Asset;
	}

	FGSTN_CompiledEntry MakeEntry(const FGameplayTag& Command, int32 SlotIndex, int32 TimelineIndex)
	{
		FGSTN_CompiledEntry Entry;
		Entry.Source = EGSTN_EntrySource::Kit;
		Entry.Command = Command;
		Entry.Ability = TimelineClassPath();
		Entry.SlotIndex = SlotIndex;
		Entry.TimelineIndex = static_cast<int16>(TimelineIndex);
		return Entry;
	}

	/** Two commands, one ability class: registry 0 = (class, Base), registry 1 = (class, Modded), as the compiler makes them for a modded kit */
	FGSTN_CompiledBindingTable MakeCompiled(const FSoftObjectPath& Base, const FSoftObjectPath& Modded)
	{
		FGSTN_CompiledBindingTable Compiled;
		Compiled.Revision = 5;
		FGSTN_InputSlotSpec& BaseSlot = Compiled.Slots.AddDefaulted_GetRef();
		BaseSlot.Command = ModTlCommandBase;
		FGSTN_InputSlotSpec& ModdedSlot = Compiled.Slots.AddDefaulted_GetRef();
		ModdedSlot.Command = ModTlCommandModded;
		Compiled.Entries.Add(MakeEntry(ModTlCommandBase, 0, 0));
		Compiled.Entries.Add(MakeEntry(ModTlCommandModded, 1, 1));
		FGSTN_TimelineRegistryEntry& First = Compiled.Registry.AddDefaulted_GetRef();
		First.Ability = TimelineClassPath();
		First.TimelineOverride = Base;
		FGSTN_TimelineRegistryEntry& Second = Compiled.Registry.AddDefaulted_GetRef();
		Second.Ability = TimelineClassPath();
		Second.TimelineOverride = Modded;
		return Compiled;
	}

	struct FModTlSettings
	{
		TStrongObjectPtr<UGSTN_InputSettings> Input{ NewObject<UGSTN_InputSettings>(GetTransientPackage()) };
		TStrongObjectPtr<UGSTN_CombatSettings> Combat{ NewObject<UGSTN_CombatSettings>(GetTransientPackage()) };
	};

	TStrongObjectPtr<UGSTN_CombatTableSet> Convert(const FGSTN_CompiledBindingTable& Compiled, FModTlSettings& Settings, FGSTN_CompileReport* Report = nullptr)
	{
		FGSTN_CombatTableConvertInput Input(&ResolveModTimelineClass);
		// In memory only: the test timelines are transient objects, and a missing one must not try a disk load
		Input.LoadTimeline = [](const FSoftObjectPath& Path) { return Cast<UGSTN_CombatTimeline>(Path.ResolveObject()); };
		Input.InputSettings = Settings.Input.Get();
		Input.CombatSettings = Settings.Combat.Get();
		return TStrongObjectPtr<UGSTN_CombatTableSet>(FGSTN_CombatTableConverter::Convert(GetTransientPackage(), Compiled, Input, Report));
	}

	struct FRunResult
	{
		int32 CommitFrame = INDEX_NONE;
		int32 ActionableFrame = INDEX_NONE;
		int32 EndFrame = INDEX_NONE;
	};

	/** Starts registry entry Entry of the set at StartFrame and runs the combat tick's timeline steps on the data the sim reads for it */
	FRunResult RunEntry(const UGSTN_CombatTableSet& Set, int32 Entry, int32 StartFrame)
	{
		FRunResult Result;
		const FGSTN_CombatTimelineData* Data = Set.GetTimelineData(Entry);
		if (!Data)
		{
			return Result;
		}
		FGSTN_CombatSyncCore Core;
		FGSTN_CombatTimelineLogic::Start(Core, StartFrame, Entry, EGSTN_ActivationSource::Command);
		FGSTN_TimelineOutput Output;
		Output.Reserve(*Data);
		FGSTN_CombatTimelineLogic::EvaluateStart(Core, StartFrame, *Data, Set.TimelineDefaultOverride[Entry], Output);
		for (int32 Frame = StartFrame + 1; Frame < StartFrame + 200; ++Frame)
		{
			Core.Requests.PruneBefore(Frame);
			const bool bRunning = FGSTN_CombatTimelineLogic::AdvanceClock(Core, Frame, Set.GetTimelineData(Core.ActiveTimeline), Set.TimelineDefaultOverride[Entry], true, Output);
			if (Result.CommitFrame == INDEX_NONE && Core.Requests.Find(EGSTN_RequestType::CommitTimeline, Frame))
			{
				Result.CommitFrame = Frame - StartFrame;
			}
			if (Result.ActionableFrame == INDEX_NONE && FGSTN_CombatTimelineLogic::IsCommandAllowed(Core, Data, Output, GSTNCommandTags::Special, 0, 0))
			{
				Result.ActionableFrame = Frame - StartFrame;
			}
			if (!bRunning)
			{
				Result.EndFrame = Frame - StartFrame;
				break;
			}
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPModTimelineRegistryTest, "GSTNCombat.NP.ModTimeline.EntryRunsItsOverride", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPModTimelineRegistryTest::RunTest(const FString& Parameters)
{
	FModTlSettings Settings;
	TStrongObjectPtr<UGSTN_CombatTimeline> Base(MakeTimelineAsset(TEXT("ModTlBase"), 1.f));
	TStrongObjectPtr<UGSTN_CombatTimeline> Modded(MakeTimelineAsset(TEXT("ModTlModded"), 1.5f));

	FGSTN_CompileReport Report;
	const TStrongObjectPtr<UGSTN_CombatTableSet> Set = Convert(MakeCompiled(FSoftObjectPath(Base.Get()), FSoftObjectPath(Modded.Get())), Settings, &Report);
	TestEqual(TEXT("no error"), Report.Errors.Num(), 0);
	if (!TestEqual(TEXT("two registry entries"), Set->TimelineRegistry.Num(), 2))
	{
		return false;
	}
	TestTrue(TEXT("of the same class"), Set->TimelineRegistry[0] == Set->TimelineRegistry[1]);
	TestTrue(TEXT("each with its own override"), Set->TimelineOverrides[0] == Base.Get() && Set->TimelineOverrides[1] == Modded.Get());
	TestEqual(TEXT("entry 0 runs the base data"), Set->GetTimelineData(0)->TotalFrames, 30);
	TestEqual(TEXT("entry 1 runs the modded data"), Set->GetTimelineData(1)->TotalFrames, 45);
	TestTrue(TEXT("the overrides are kept alive by the set"), Set->TimelineContent.Contains(Base.Get()) && Set->TimelineContent.Contains(Modded.Get()));
	TestEqual(TEXT("the bindings point at their own entry"), static_cast<int32>(Set->BindingTimeline[0]) + static_cast<int32>(Set->BindingTimeline[1]), 1);
	TestTrue(TEXT("an index out of the registry has no data"), Set->GetTimelineData(2) == nullptr && Set->GetTimelineData(INDEX_NONE) == nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPModTimelineSimTest, "GSTNCombat.NP.ModTimeline.ModdedMoveRunsDifferently", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPModTimelineSimTest::RunTest(const FString& Parameters)
{
	// A "slower" mod (time x1.5) changes when the move commits, when the combatant can act again and when the move ends, in the sim
	FModTlSettings Settings;
	TStrongObjectPtr<UGSTN_CombatTimeline> Base(MakeTimelineAsset(TEXT("ModTlSimBase"), 1.f));
	TStrongObjectPtr<UGSTN_CombatTimeline> Modded(MakeTimelineAsset(TEXT("ModTlSimModded"), 1.5f));
	const TStrongObjectPtr<UGSTN_CombatTableSet> Set = Convert(MakeCompiled(FSoftObjectPath(Base.Get()), FSoftObjectPath(Modded.Get())), Settings);
	if (!TestEqual(TEXT("two registry entries"), Set->TimelineRegistry.Num(), 2))
	{
		return false;
	}

	const FRunResult BaseRun = RunEntry(*Set, 0, 100);
	const FRunResult ModdedRun = RunEntry(*Set, 1, 100);
	TestEqual(TEXT("the base move ends on its 30th frame"), BaseRun.EndFrame, 30);
	TestEqual(TEXT("the modded move ends on its 45th frame"), ModdedRun.EndFrame, 45);
	TestEqual(TEXT("the base commits after its 2 frame window"), BaseRun.CommitFrame, 2);
	TestEqual(TEXT("the modded commits after its 3 frame window"), ModdedRun.CommitFrame, 3);
	TestTrue(TEXT("the modded move becomes actionable later"), BaseRun.ActionableFrame != INDEX_NONE && ModdedRun.ActionableFrame > BaseRun.ActionableFrame);

	// Deterministic: the same loadout converted again runs the same frames and has the same checksum
	const TStrongObjectPtr<UGSTN_CombatTableSet> Again = Convert(MakeCompiled(FSoftObjectPath(Base.Get()), FSoftObjectPath(Modded.Get())), Settings);
	const FRunResult AgainRun = RunEntry(*Again, 1, 100);
	TestTrue(TEXT("a second conversion runs the same frames"), AgainRun.EndFrame == ModdedRun.EndFrame && AgainRun.CommitFrame == ModdedRun.CommitFrame && AgainRun.ActionableFrame == ModdedRun.ActionableFrame);
	TestEqual(TEXT("and has the same checksum"), Again->Checksum, Set->Checksum);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPModTimelineChecksumTest, "GSTNCombat.NP.ModTimeline.OverrideInChecksum", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPModTimelineChecksumTest::RunTest(const FString& Parameters)
{
	FModTlSettings Settings;
	TStrongObjectPtr<UGSTN_CombatTimeline> Base(MakeTimelineAsset(TEXT("ModTlSumBase"), 1.f));
	TStrongObjectPtr<UGSTN_CombatTimeline> Modded(MakeTimelineAsset(TEXT("ModTlSumModded"), 1.5f));
	TStrongObjectPtr<UGSTN_CombatTimeline> Other(MakeTimelineAsset(TEXT("ModTlSumOther"), 2.f));
	const uint32 WithModded = Convert(MakeCompiled(FSoftObjectPath(Base.Get()), FSoftObjectPath(Modded.Get())), Settings)->Checksum;
	const uint32 WithOther = Convert(MakeCompiled(FSoftObjectPath(Base.Get()), FSoftObjectPath(Other.Get())), Settings)->Checksum;
	TestNotEqual(TEXT("another override is another table: two machines with different mods disagree"), WithModded, WithOther);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPModTimelineMissingTest, "GSTNCombat.NP.ModTimeline.MissingOverrideFallsBack", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPModTimelineMissingTest::RunTest(const FString& Parameters)
{
	// An override that does not load keeps the move on its class default's data, with an error in the report
	FModTlSettings Settings;
	TStrongObjectPtr<UGSTN_CombatTimeline> Base(MakeTimelineAsset(TEXT("ModTlMissBase"), 1.f));
	FGSTN_CompileReport Report;
	const TStrongObjectPtr<UGSTN_CombatTableSet> Set = Convert(MakeCompiled(FSoftObjectPath(Base.Get()), FSoftObjectPath(TEXT("/Engine/Transient.GSTN_ModTlDoesNotExist"))), Settings, &Report);
	if (!TestEqual(TEXT("both entries are kept"), Set->TimelineRegistry.Num(), 2))
	{
		return false;
	}
	TestNull(TEXT("the missing one has no override"), Set->TimelineOverrides[1].Get());
	const UGSTN_GA_BaseCombat* Default = UGSTN_TestMontageTimelineAbility::StaticClass()->GetDefaultObject<UGSTN_GA_BaseCombat>();
	TestTrue(TEXT("and runs the class default's data"), Set->GetTimelineData(1) == &Default->GetTimelineData());
	TestEqual(TEXT("one error"), Report.Errors.Num(), 1);
	return true;
}

#endif
