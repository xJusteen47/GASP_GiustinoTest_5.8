#include "Display/GSTN_InputDisplayLayout.h"
#include "Display/GSTN_InputDisplayScript.h"
#include "GameFramework/Pawn.h"
#include "GSTNCombatDebugModule.h"
#include "HAL/IConsoleManager.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_CombatButtonTags.h"
#include "Input/GSTN_CombatCommandTags.h"
#include "Input/GSTN_InputScript.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_StandardSlots.h"
#include "Misc/AutomationTest.h"
#include "Sources/GSTN_CombatInputSource.h"
#include "Sources/GSTN_InputSourceRegistry.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

// The combat side of the input display. The display, its layout and its script player live in GSTNInputDebug (GSTNInput.Debug.*); what
// stays here is what only the combat plugin knows: the standard command set the scripted box resolves against, and the finder that reads
// the combat component.

/** Flags of every combat input display test: pure logic, runs in the editor and in headless commandlets */
#define GSTN_DEBUG_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNCombatInputDisplayTest
{
	/** The six standard buttons in bit order, the letters A to F of the notation */
	TArray<FGameplayTag> ButtonTags()
	{
		return FGSTN_CombatInputSource::GetCombatButtonTags();
	}

	/** Settings with the recommended combat values written out, so the result does not move with a designer's config (as the Core tests do) */
	TStrongObjectPtr<UGSTN_InputSettings> MakeSettings()
	{
		TStrongObjectPtr<UGSTN_InputSettings> Settings(NewObject<UGSTN_InputSettings>(GetTransientPackage()));
		Settings->DefaultBufferFrames = 8;
		Settings->bHoldBufferByDefault = true;
		Settings->MaxHoldBufferFrames = 0;
		Settings->DefaultSimultaneityFrames = 3;
		Settings->SimultaneousFrames = 2;
		Settings->BufferConflictPolicy = EGSTN_BufferConflictPolicy::MostRecent;
		Settings->bChordGraceByDefault = false;
		Settings->ChordGraceFrames = 3;
		Settings->StickDeadZone = 0.2f;
		Settings->AimZone = 0.5f;
		Settings->FlickFrames = 3;
		Settings->DiagonalHalfAngle = 22.5f;
		Settings->WorldReferenceYaw = 0.f;
		Settings->CommandTiers.Empty();
		Settings->CommandTiers.Add(GSTNCommandTags::Attack, 100);
		Settings->CommandTiers.Add(GSTNCommandTags::Strong, 200);
		Settings->CommandTiers.Add(GSTNCommandTags::Special, 300);
		Settings->CommandTiers.Add(GSTNCommandTags::Combo, 350);
		Settings->CommandTiers.Add(GSTNCommandTags::Super, 400);
		Settings->ExclusiveButtons = { GSTNCombatButtonTags::Button_Attack.GetTag(), GSTNCombatButtonTags::Button_Strong.GetTag(), GSTNCombatButtonTags::Button_Special.GetTag() };
		return Settings;
	}

	/** Plays a notation into display data the way the display would get it from a pawn, without resolving a command */
	bool Play(FAutomationTestBase& Test, const TCHAR* Notation, const UGSTN_InputSettings& Settings, FGSTN_InputDisplayData& OutData)
	{
		FGSTN_InputScript Script(ButtonTags());
		FString Error;
		const bool bOk = Script.Parse(Notation, Error) && Script.Advance(OutData.History, Settings, Script.GetLastFrame());
		Test.TestTrue(*FString::Printf(TEXT("script plays: %s (%s)"), Notation, *Error), bOk);
		return bOk;
	}

	/** Resolves a notation against a command set */
	FGSTN_InputDisplayScriptResult ResolveStandard(const FString& Notation, const UGSTN_InputSettings& Settings, const TArray<FGSTN_InputSlotSpec>& Slots)
	{
		TArray<const FGSTN_InputSlotSpec*> Pointers;
		for (const FGSTN_InputSlotSpec& Spec : Slots)
		{
			Pointers.Add(&Spec);
		}
		return FGSTN_InputDisplayScript::Resolve(Notation, ButtonTags(), Pointers, Settings);
	}
}

using namespace GSTNCombatInputDisplayTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDisplayConsumedTest, "GSTNCombat.Debug.InputDisplay.ConsumedPressGreyed", GSTN_DEBUG_TEST_FLAGS)
bool FGSTN_InputDisplayConsumedTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = UGSTN_StandardSlots::Build();

	// A pressed on frame 0 and released on 4: the neutral attack comes out on frame 4, which consumes the press
	const FGSTN_InputDisplayScriptResult Result = ResolveStandard(TEXT("0:+A 4:-A"), *Settings, Slots);
	if (!TestTrue(TEXT("the script resolves"), Result.bOk))
	{
		return false;
	}
	TestEqual(TEXT("the command"), Result.Command.ToString(), GSTNCommandTags::Attack_Neutral.GetTag().ToString());

	const FGSTN_InputDisplayLayout Layout = FGSTN_InputDisplayLayout::Build(Result.Data);
	if (!TestEqual(TEXT("columns"), Layout.NumColumns, 5))
	{
		return false;
	}
	const FGSTN_InputDisplayCell& Press = Layout.Buttons[0][0];
	TestTrue(TEXT("the press is a press"), Press.Kind == EGSTN_InputCellKind::Press);
	TestTrue(TEXT("the press the command used is greyed"), Press.bConsumed);
	TestTrue(TEXT("its hold is greyed"), Layout.Buttons[0][2].Kind == EGSTN_InputCellKind::Hold && Layout.Buttons[0][2].bConsumed);
	TestTrue(TEXT("so is the release"), Layout.Buttons[0][4].Kind == EGSTN_InputCellKind::Release && Layout.Buttons[0][4].bConsumed);

	// The command is a marker on its frame, and the greyed press knows which command used it (the hover text)
	TestEqual(TEXT("one marker"), Layout.Markers.Num(), 1);
	TestEqual(TEXT("marker column"), Layout.ColumnMarker[4], 0);
	TestEqual(TEXT("the other columns have none"), Layout.ColumnMarker[3], (int32)INDEX_NONE);
	if (TestTrue(TEXT("the press names its command"), Layout.Markers.IsValidIndex(Press.MarkerIndex)))
	{
		TestEqual(TEXT("which command"), Layout.Markers[Press.MarkerIndex].Command.ToString(), GSTNCommandTags::Attack_Neutral.GetTag().ToString());
	}

	// The same history before any command used it is not greyed
	FGSTN_InputDisplayData Raw;
	if (Play(*this, TEXT("0:+A 4:-A"), *Settings, Raw))
	{
		const FGSTN_InputDisplayLayout Plain = FGSTN_InputDisplayLayout::Build(Raw);
		TestFalse(TEXT("an unused press is not greyed"), Plain.Buttons[0][0].bConsumed);
		TestEqual(TEXT("and has no marker"), Plain.Markers.Num(), 0);
	}

	// A marker too far after the press is not blamed on it: the press stays greyed, with no command named
	FGSTN_InputDisplayData Far;
	if (Play(*this, TEXT("0:+A"), *Settings, Far))
	{
		Far.History.Consume(Far.History.GetButtonSlot(0));
		Far.Markers.Add({ 30, GSTNCommandTags::Attack_Neutral.GetTag() });
		FGSTN_InputDisplayLayoutParams Params;
		Params.NowFrame = 40;
		const FGSTN_InputDisplayLayout Late = FGSTN_InputDisplayLayout::Build(Far, Params);
		TestTrue(TEXT("a consumed press whose command is far away stays greyed"), Late.Buttons[0][0].bConsumed);
		TestEqual(TEXT("without a command name"), Late.Buttons[0][0].MarkerIndex, (int32)INDEX_NONE);

		Far.Markers[0].Frame = 5;
		const FGSTN_InputDisplayLayout Near = FGSTN_InputDisplayLayout::Build(Far, Params);
		TestEqual(TEXT("a marker within reach names its command"), Near.Buttons[0][0].MarkerIndex, 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDisplayScriptedTest, "GSTNCombat.Debug.InputDisplay.ScriptedHistoryResolvesCommand", GSTN_DEBUG_TEST_FLAGS)
bool FGSTN_InputDisplayScriptedTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	const TArray<FGSTN_InputSlotSpec> Slots = UGSTN_StandardSlots::Build();

	const auto Expect = [&](const TCHAR* Notation, const FNativeGameplayTag& Expected)
	{
		const FGSTN_InputDisplayScriptResult Result = ResolveStandard(Notation, *Settings, Slots);
		TestTrue(*FString::Printf(TEXT("[%s] plays"), Notation), Result.bOk);
		TestEqual(*FString::Printf(TEXT("[%s] command"), Notation), Result.Command.ToString(), Expected.GetTag().ToString());
	};
	Expect(TEXT("5:+A"), GSTNCommandTags::Attack_Neutral);
	Expect(TEXT("0:8t 5:+A"), GSTNCommandTags::Attack_Directional);
	Expect(TEXT("0:+D 5:+A"), GSTNCommandTags::Attack_CrouchNeutral);
	Expect(TEXT("0:+D 0:8t 5:+C"), GSTNCommandTags::Special_CrouchDirectional);

	// The result carries the history, the consumed press and the marker the strip draws
	const FGSTN_InputDisplayScriptResult Result = ResolveStandard(TEXT("5:+A"), *Settings, Slots);
	TestEqual(TEXT("last frame"), Result.LastFrame, 5);
	TestTrue(TEXT("a slot index"), Slots.IsValidIndex(Result.SlotIndex));
	if (TestEqual(TEXT("one marker"), Result.Data.Markers.Num(), 1))
	{
		TestEqual(TEXT("on the last frame"), Result.Data.Markers[0].Frame, 5);
	}
	TestTrue(TEXT("the strip greys the press"), FGSTN_InputDisplayLayout::Build(Result.Data).Buttons[0][5].bConsumed);

	// Input that no command of the set takes: legal, no command, nothing greyed
	const FGSTN_InputDisplayScriptResult Nothing = ResolveStandard(TEXT("0:6t"), *Settings, Slots);
	TestTrue(TEXT("plays"), Nothing.bOk);
	TestFalse(TEXT("no command"), Nothing.Command.IsValid());
	TestEqual(TEXT("no slot"), Nothing.SlotIndex, (int32)INDEX_NONE);
	TestEqual(TEXT("no marker"), Nothing.Data.Markers.Num(), 0);

	// A kit without the command: the same notation resolves to what is left (or to nothing)
	TArray<FGSTN_InputSlotSpec> Reduced = Slots;
	Reduced.RemoveAll([](const FGSTN_InputSlotSpec& Spec) { return Spec.Command == GSTNCommandTags::Attack_Neutral.GetTag(); });
	TestFalse(TEXT("a set without the neutral attack takes the press as nothing"), ResolveStandard(TEXT("5:+A"), *Settings, Reduced).Command.IsValid());

	// A typo is an answer, not a crash or a log error: the syntax errors come back in Error
	const FGSTN_InputDisplayScriptResult Bad = ResolveStandard(TEXT("banana"), *Settings, Slots);
	TestFalse(TEXT("not a notation"), Bad.bOk);
	TestFalse(TEXT("with a reason"), Bad.Error.IsEmpty());
	TestEqual(TEXT("and no data"), Bad.Data.History.GetLatestFrame(), (int32)INDEX_NONE);
	TestFalse(TEXT("empty notation"), ResolveStandard(TEXT("  "), *Settings, Slots).bOk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDisplayCombatRegistrationTest, "GSTNCombat.Debug.InputDisplay.CombatSourceIsRegisteredWithTheInputDisplay", GSTN_DEBUG_TEST_FLAGS)
bool FGSTN_InputDisplayCombatRegistrationTest::RunTest(const FString& Parameters)
{
	FGSTN_InputSourceRegistry& Registry = FGSTN_InputSourceRegistry::Get();

	// This module's finder is in the input display's registry, and a pawn without the combat component has nothing for it to read
	TestTrue(TEXT("a finder is registered (the combat one)"), Registry.GetNumFinders() >= 1);
	const TStrongObjectPtr<APawn> Pawn(NewObject<APawn>(GetTransientPackage()));
	TestFalse(TEXT("a pawn with no combat component has no source"), FGSTN_CombatInputSource::Find(*Pawn).IsValid());
	TestFalse(TEXT("so the registry finds none either"), Registry.FindSource(*Pawn).IsValid());

	// The default profile of the display is the combat one: the six buttons in the order of the notation, and the standard command set
	const FGSTN_InputDisplayProfile Profile = Registry.GetDefaultProfile();
	if (TestEqual(TEXT("six combat buttons"), Profile.ButtonTags.Num(), 6))
	{
		TestTrue(TEXT("A is the normal attack"), Profile.ButtonTags[0] == GSTNCombatButtonTags::Button_Attack.GetTag());
		TestTrue(TEXT("F is the jump"), Profile.ButtonTags[5] == GSTNCombatButtonTags::Button_Jump.GetTag());
	}
	TestEqual(TEXT("the standard command set"), Profile.Slots.Num(), UGSTN_StandardSlots::Build().Num());

	// With that profile "what does this input do" works before any play session
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeSettings();
	TArray<const FGSTN_InputSlotSpec*> Slots;
	for (const FGSTN_InputSlotSpec& Slot : Profile.Slots)
	{
		Slots.Add(&Slot);
	}
	const FGSTN_InputDisplayScriptResult Result = FGSTN_InputDisplayScript::Resolve(TEXT("5:+A"), Profile.ButtonTags, Slots, *Settings);
	TestEqual(TEXT("the neutral attack"), Result.Command.ToString(), GSTNCommandTags::Attack_Neutral.GetTag().ToString());

	// The module's own entry point reaches the display of the input plugin (null without Slate, which is a legal answer)
	if (IGSTN_CombatDebugModule* Module = IGSTN_CombatDebugModule::Get())
	{
		Module->CreateInputDisplay();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InputDisplayAliasVariableTest, "GSTNCombat.Debug.InputDisplay.OldConsoleVariableSetsTheNewOne", GSTN_DEBUG_TEST_FLAGS)
bool FGSTN_InputDisplayAliasVariableTest::RunTest(const FString& Parameters)
{
	IConsoleVariable* Old = IConsoleManager::Get().FindConsoleVariable(TEXT("gstn.Combat.Debug.InputDisplay"));
	IConsoleVariable* New = IConsoleManager::Get().FindConsoleVariable(TEXT("gstn.Input.Debug.InputDisplay"));
	if (!TestNotNull(TEXT("the old name still exists"), Old) || !TestNotNull(TEXT("the new name exists"), New))
	{
		return false;
	}
	const int32 OldValue = Old->GetInt();
	const int32 NewValue = New->GetInt();

	Old->Set(1, ECVF_SetByConsole);
	TestEqual(TEXT("setting the old name turns the display on"), New->GetInt(), 1);
	Old->Set(0, ECVF_SetByConsole);
	TestEqual(TEXT("and off"), New->GetInt(), 0);

	Old->Set(OldValue, ECVF_SetByConsole);
	New->Set(NewValue, ECVF_SetByConsole);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
