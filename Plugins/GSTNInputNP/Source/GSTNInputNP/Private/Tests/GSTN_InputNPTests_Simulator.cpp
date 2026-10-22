// Automation tests of the input simulator and the command table. They need no world: the simulator is a function of the
// command, the previous state and a host, and the host here is a mock.

#include "Abilities/GameplayAbility.h"
#include "Bindings/GSTN_CommandBindingSet.h"
#include "Tests/GSTN_InputNPTestObjects.h"
#include "Events/GSTN_SimEvent.h"
#include "Input/GSTN_CommandLocks.h"
#include "Input/GSTN_ButtonMap.h"
#include "Input/GSTN_InputConditions.h"
#include "Input/GSTN_InputPattern.h"
#include "Input/GSTN_InputSettings.h"
#include "Input/GSTN_InputTags.h"
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"
#include "Simulation/GSTN_CommandSourceInterface.h"
#include "Simulation/GSTN_InputHostUtils.h"
#include "Simulation/GSTN_InputSimComponent.h"
#include "Simulation/GSTN_InputSimulator.h"
#include "Sources/GSTN_InputSource.h"
#include "Context/GSTN_InputContextProvider.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	// Declared here so the tests do not depend on the game's tags; the input plugin has no game vocabulary of its own
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TestButtonA, "Input.Button.NPTest.A", "Test-only tag of the GSTNInputNP automation tests");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TestCommandAttack, "Command.NPTest.Attack", "Test-only tag of the GSTNInputNP automation tests");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TestCommandBlock, "Command.NPTest.Block", "Test-only tag of the GSTNInputNP automation tests");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TestButtonB, "Input.Button.NPTest.B", "Test-only tag of the GSTNInputNP automation tests");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TestCommandDash, "Command.NPTest.Dash", "Test-only tag of the GSTNInputNP automation tests");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TestCommandAlpha, "Command.NPTest.Alpha", "Test-only tag of the GSTNInputNP automation tests");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(TestLockAttack, "Input.Lock.Command.NPTest.Attack", "Test-only lock tag of the GSTNInputNP automation tests: locks Command.NPTest.Attack.*");

	class FMockHost : public IGSTN_InputSimHost
	{
	public:
		FGameplayTagContainer OwnerTags;
		FGameplayTagContainer ContextTags;
		bool bCanExecute = true;

		virtual float GetFacingYaw() const override { return 0.f; }
		virtual void GetOwnerTags(FGameplayTagContainer& OutTags) const override { OutTags = OwnerTags; }
		virtual void AppendContextTags(FGameplayTagContainer& OutTags) const override { OutTags.AppendTags(ContextTags); }
		virtual bool CanExecuteBinding(int32 BindingIndex) const override { return bCanExecute; }
	};

	FGSTN_InputSlotEntry MakePressSlot(const FGameplayTag& Command, const FGameplayTag& Button)
	{
		FGSTN_InputCondition_Press Press;
		Press.Buttons.AddTag(Button);

		FGSTN_InputStep Step;
		Step.AllOf.Add(TInstancedStruct<FGSTN_InputCondition>::Make<FGSTN_InputCondition_Press>(Press));

		FGSTN_InputSlotEntry Entry;
		Entry.Inline.Command = Command;
		Entry.Inline.Pattern.Steps.Add(Step);
		return Entry;
	}

	/**
	 * Slots Attack (a press of A) and Block (also a press of A, unbound: it must never reserve a press), declared Block first
	 * so the sort by name shows. Attack has an airborne binding declared before the unconditional one.
	 */
	TStrongObjectPtr<UGSTN_CommandBindingSet> MakeBindingSet()
	{
		TStrongObjectPtr<UGSTN_CommandBindingSet> Set(NewObject<UGSTN_CommandBindingSet>(GetTransientPackage()));
		Set->Slots.Add(MakePressSlot(TestCommandBlock, TestButtonA));
		Set->Slots.Add(MakePressSlot(TestCommandAttack, TestButtonA));

		FGSTN_CommandBinding Airborne;
		Airborne.Command = TestCommandAttack;
		Airborne.Requirements.RequireTags.AddTag(GSTNInputTags::Context_Airborne);
		Airborne.Ability = TSoftClassPtr<UGameplayAbility>(UGameplayAbility::StaticClass());
		Set->Bindings.Add(Airborne);

		FGSTN_CommandBinding Plain;
		Plain.Command = TestCommandAttack;
		Plain.Ability = TSoftClassPtr<UGameplayAbility>(UGameplayAbility::StaticClass());
		Set->Bindings.Add(Plain);
		return Set;
	}

	TStrongObjectPtr<UGSTN_InputSettings> MakeNPSettings()
	{
		TStrongObjectPtr<UGSTN_InputSettings> Settings(NewObject<UGSTN_InputSettings>(GetTransientPackage()));
		Settings->DefaultBufferFrames = 8;
		Settings->bHoldBufferByDefault = true;
		Settings->MaxHoldBufferFrames = 0;
		Settings->DefaultSimultaneityFrames = 3;
		Settings->SimultaneousFrames = 2;
		Settings->bChordGraceByDefault = false;
		Settings->PriorityCriteria = UGSTN_InputSettings::MakeDefaultPriorityCriteria();
		Settings->CommandTiers.Empty();
		Settings->ExclusiveButtons.Empty();
		return Settings;
	}

	/** Runs one frame of the simulator on State, with the buttons held as given */
	FGSTN_InputTickResult RunFrame(FGSTN_InputSimState& State, int32 Frame, uint32 Held, const FMockHost& Host, const FGSTN_CommandTable& Table,
		const FGSTN_ButtonMap& Buttons, const UGSTN_InputSettings& Settings, uint16 ForcedCommand = 0, FGSTN_SimEventOutbox* Outbox = nullptr)
	{
		FGSTN_InputCmd Cmd;
		Cmd.HeldButtons = Held;
		Cmd.ForcedCommand = ForcedCommand;

		FGSTN_InputSimState Out = State;
		FGSTN_InputTickArgs Args;
		Args.Frame = Frame;
		Args.Cmd = &Cmd;
		Args.In = &State;
		Args.Out = &Out;
		Args.Table = &Table;
		Args.Buttons = &Buttons;
		Args.Settings = &Settings;
		Args.Outbox = Outbox;

		const FGSTN_InputTickResult Result = FGSTN_InputSimulator::Tick(Args, Host);
		State = Out;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPCommandTableTest, "GSTNInput.NP.Bindings.Table", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPCommandTableTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CommandBindingSet> Set = MakeBindingSet();
	const FGSTN_CommandTable Table = Set->BuildTable();

	if (!TestEqual(TEXT("both slots are commands"), Table.Commands.Num(), 2))
	{
		return false;
	}

	// "Command.NPTest.Attack" sorts before "Command.NPTest.Block", whatever the declaration order
	TestTrue(TEXT("commands sorted by tag name"), Table.Commands[0].Tag == TestCommandAttack.GetTag());
	TestEqual(TEXT("the attack has both bindings"), Table.Commands[0].NumBindings, 2);
	TestEqual(TEXT("the block has none"), Table.Commands[1].NumBindings, 0);
	TestEqual(TEXT("bindings keep declaration order"), Table.Bindings.Num(), 2);
	TestFalse(TEXT("the first binding is the conditional one"), Table.Bindings[0].Requirements.IsEmpty());
	TestTrue(TEXT("the second binding is the plain one"), Table.Bindings[1].Requirements.IsEmpty());

	TestEqual(TEXT("only the bound command is a candidate"), Table.CandidateSlots.Num(), 1);
	TestEqual(TEXT("the candidate maps back to its command"), Table.CandidateCommands[0], 0);

	TestEqual(TEXT("forced command value is 1 + index"), static_cast<int32>(Table.ToForcedCommand(TestCommandBlock)), 2);
	TestEqual(TEXT("forced command 0 names nothing"), Table.FromForcedCommand(0), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("forced command round trip"), Table.FromForcedCommand(Table.ToForcedCommand(TestCommandBlock)), 1);
	TestEqual(TEXT("unknown command"), static_cast<int32>(Table.ToForcedCommand(GSTNInputTags::Command)), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPServerOnlyBindingTest, "GSTNInput.NP.Bindings.ServerOnlyAbilityRejected", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPServerOnlyBindingTest::RunTest(const FString& Parameters)
{
	// The press consumption follows CanActivateAbility, which reads the net role of a server only ability: client and server
	// would answer differently, so the table must not contain such a binding.
	TestTrue(TEXT("a plain ability is bindable"), UGSTN_CommandBindingSet::IsBindableAbility(GetDefault<UGameplayAbility>()));
	FString Reason;
	TestFalse(TEXT("ServerOnly is refused"), UGSTN_CommandBindingSet::IsBindableAbility(GetDefault<UGSTN_TestServerOnlyAbility>(), &Reason));
	TestTrue(TEXT("the reason names the policy"), Reason.Contains(TEXT("server only")));
	TestFalse(TEXT("ServerOnlyExecution is refused"), UGSTN_CommandBindingSet::IsBindableAbility(GetDefault<UGSTN_TestServerOnlyExecutionAbility>()));

	const TStrongObjectPtr<UGSTN_CommandBindingSet> Set = MakeBindingSet();
	FGSTN_CommandBinding ServerOnly;
	ServerOnly.Command = TestCommandBlock;
	ServerOnly.Ability = TSoftClassPtr<UGameplayAbility>(UGSTN_TestServerOnlyAbility::StaticClass());
	Set->Bindings.Add(ServerOnly);
	FGSTN_CommandBinding ServerOnlyExecution = ServerOnly;
	ServerOnlyExecution.Ability = TSoftClassPtr<UGameplayAbility>(UGSTN_TestServerOnlyExecutionAbility::StaticClass());
	Set->Bindings.Add(ServerOnlyExecution);

	AddExpectedError(TEXT("server only net security policy"), EAutomationExpectedErrorFlags::Contains, 2);
	const FGSTN_CommandTable Table = Set->BuildTable();

	TestEqual(TEXT("the server only bindings are left out"), Table.Bindings.Num(), 2);
	TestEqual(TEXT("so the command has none"), Table.Commands[1].NumBindings, 0);
	TestEqual(TEXT("and is not a candidate"), Table.CandidateSlots.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPSimulatorRequestTest, "GSTNInput.NP.Simulator.Request", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPSimulatorRequestTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CommandBindingSet> Set = MakeBindingSet();
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeNPSettings();
	const FGSTN_CommandTable Table = Set->BuildTable();
	const FGSTN_ButtonMap Buttons(TArray<FGameplayTag>{ TestButtonA.GetTag() });
	FMockHost Host;

	FGSTN_InputSimState State;
	for (int32 Frame = 0; Frame < 10; ++Frame)
	{
		TestFalse(TEXT("idle frames request nothing"), RunFrame(State, Frame, 0, Host, Table, Buttons, *Settings).HasRequest());
	}

	// A press on the ground starts the plain binding (index 1) and is stamped with its frame and button
	const FGSTN_InputTickResult Press = RunFrame(State, 10, 1, Host, Table, Buttons, *Settings);
	TestTrue(TEXT("the press requests"), Press.HasRequest());
	TestEqual(TEXT("the plain binding"), Press.BindingIndex, 1);
	TestEqual(TEXT("request stores binding + 1"), static_cast<int32>(State.RequestedBinding), 2);
	TestEqual(TEXT("request frame"), State.RequestFrame, 10);
	TestEqual(TEXT("request button is 1 + the pressed button"), static_cast<int32>(State.RequestButton), 1);
	TestEqual(TEXT("last activated"), State.LastActivatedFrame, 10);

	// Held on: the press was consumed with the request, the hold buffer must not use it again
	for (int32 Frame = 11; Frame < 20; ++Frame)
	{
		TestFalse(TEXT("a held button does not repeat"), RunFrame(State, Frame, 1, Host, Table, Buttons, *Settings).HasRequest());
	}
	RunFrame(State, 20, 0, Host, Table, Buttons, *Settings);

	// In the air the airborne binding is the more demanding one and wins
	Host.ContextTags.AddTag(GSTNInputTags::Context_Airborne);
	const FGSTN_InputTickResult Air = RunFrame(State, 21, 1, Host, Table, Buttons, *Settings);
	TestEqual(TEXT("the airborne binding in the air"), Air.BindingIndex, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPSimulatorBufferTest, "GSTNInput.NP.Simulator.Buffer", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPSimulatorBufferTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CommandBindingSet> Set = MakeBindingSet();
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeNPSettings();
	const FGSTN_CommandTable Table = Set->BuildTable();
	const FGSTN_ButtonMap Buttons(TArray<FGameplayTag>{ TestButtonA.GetTag() });
	FMockHost Host;
	Host.bCanExecute = false;

	// The press is made while the ability cannot start: nothing happens, and the press waits in the buffer
	FGSTN_InputSimState State;
	TestFalse(TEXT("unavailable at the press"), RunFrame(State, 0, 1, Host, Table, Buttons, *Settings).HasRequest());
	RunFrame(State, 1, 0, Host, Table, Buttons, *Settings);
	RunFrame(State, 2, 0, Host, Table, Buttons, *Settings);

	Host.bCanExecute = true;
	const FGSTN_InputTickResult Late = RunFrame(State, 3, 0, Host, Table, Buttons, *Settings);
	TestTrue(TEXT("the buffered press comes out when the ability can start"), Late.HasRequest());

	// The same press does not come out twice
	TestFalse(TEXT("consumed"), RunFrame(State, 4, 0, Host, Table, Buttons, *Settings).HasRequest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPSimulatorForcedTest, "GSTNInput.NP.Simulator.Forced", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPSimulatorForcedTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CommandBindingSet> Set = MakeBindingSet();
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeNPSettings();
	const FGSTN_CommandTable Table = Set->BuildTable();
	const FGSTN_ButtonMap Buttons(TArray<FGameplayTag>{ TestButtonA.GetTag() });
	FMockHost Host;

	FGSTN_InputSimState State;
	RunFrame(State, 0, 0, Host, Table, Buttons, *Settings);

	const FGSTN_InputTickResult Forced = RunFrame(State, 1, 0, Host, Table, Buttons, *Settings, Table.ToForcedCommand(TestCommandAttack));
	TestTrue(TEXT("a forced command needs no input"), Forced.HasRequest());
	TestEqual(TEXT("the forced command"), Forced.CommandIndex, 0);
	TestEqual(TEXT("a forced command has no trigger button"), static_cast<int32>(State.RequestButton), 0);

	// A command with no binding cannot be forced into starting anything
	const FGSTN_InputTickResult Unbound = RunFrame(State, 2, 0, Host, Table, Buttons, *Settings, Table.ToForcedCommand(TestCommandBlock));
	TestFalse(TEXT("an unbound command starts nothing"), Unbound.HasRequest());
	return true;
}

namespace
{
	/**
	 * Alpha (a press of A, declared first and sorting first, unbound), Attack (a press of A, bound) and Dash (a press of B,
	 * bound). The attack's lock tag is Input.Lock.Command.NPTest.Attack; the dash lives under no lock of it.
	 */
	TStrongObjectPtr<UGSTN_CommandBindingSet> MakeLockSet()
	{
		TStrongObjectPtr<UGSTN_CommandBindingSet> Set(NewObject<UGSTN_CommandBindingSet>(GetTransientPackage()));
		Set->Slots.Add(MakePressSlot(TestCommandAlpha, TestButtonA));
		Set->Slots.Add(MakePressSlot(TestCommandAttack, TestButtonA));
		Set->Slots.Add(MakePressSlot(TestCommandDash, TestButtonB));

		FGSTN_CommandBinding Attack;
		Attack.Command = TestCommandAttack;
		Attack.Ability = TSoftClassPtr<UGameplayAbility>(UGameplayAbility::StaticClass());
		Set->Bindings.Add(Attack);

		FGSTN_CommandBinding Dash;
		Dash.Command = TestCommandDash;
		Dash.Ability = TSoftClassPtr<UGameplayAbility>(UGameplayAbility::StaticClass());
		Set->Bindings.Add(Dash);
		return Set;
	}

	FGSTN_ButtonMap MakeTwoButtons()
	{
		return FGSTN_ButtonMap(TArray<FGameplayTag>{ TestButtonA.GetTag(), TestButtonB.GetTag() });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPUnboundCommandTest, "GSTNInput.NP.Simulator.UnboundCommands", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPUnboundCommandTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CommandBindingSet> Set = MakeLockSet();
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeNPSettings();
	const FGSTN_CommandTable Table = Set->BuildTable();
	const FGSTN_ButtonMap Buttons = MakeTwoButtons();
	FMockHost Host;

	TestEqual(TEXT("three commands"), Table.Commands.Num(), 3);
	TestEqual(TEXT("the unbound one is not a candidate"), Table.CandidateSlots.Num(), 2);
	TestEqual(TEXT("candidates keep command order (Attack)"), Table.CandidateCommands[0], 1);
	TestEqual(TEXT("candidates keep command order (Dash)"), Table.CandidateCommands[1], 2);

	// Alpha has the same pattern as Attack and sorts first: were it a candidate, it would be unavailable and would reserve the
	// press, and the attack would never come out
	FGSTN_InputSimState State;
	RunFrame(State, 0, 0, Host, Table, Buttons, *Settings);
	const FGSTN_InputTickResult Press = RunFrame(State, 1, 1, Host, Table, Buttons, *Settings);
	TestTrue(TEXT("the press is not stolen by the unbound command"), Press.HasRequest());
	TestEqual(TEXT("the attack comes out"), Press.CommandIndex, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPGlobalLockTest, "GSTNInput.NP.Simulator.Locks.Global", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPGlobalLockTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CommandBindingSet> Set = MakeLockSet();
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeNPSettings();
	const FGSTN_CommandTable Table = Set->BuildTable();
	const FGSTN_ButtonMap Buttons = MakeTwoButtons();
	FMockHost Host;
	Host.OwnerTags.AddTag(GSTNInputTags::Lock_Command);

	// Nothing starts under the global lock, but the press is not lost: it waits in the buffer
	FGSTN_InputSimState State;
	TestFalse(TEXT("locked at the press"), RunFrame(State, 0, 1, Host, Table, Buttons, *Settings).HasRequest());
	RunFrame(State, 1, 0, Host, Table, Buttons, *Settings);
	RunFrame(State, 2, 0, Host, Table, Buttons, *Settings);
	TestFalse(TEXT("a forced command is refused too"), RunFrame(State, 3, 0, Host, Table, Buttons, *Settings, Table.ToForcedCommand(TestCommandDash)).HasRequest());

	Host.OwnerTags.Reset();
	const FGSTN_InputTickResult Late = RunFrame(State, 4, 0, Host, Table, Buttons, *Settings);
	TestTrue(TEXT("the buffered press comes out when the lock ends"), Late.HasRequest());
	TestEqual(TEXT("it is the attack"), Late.CommandIndex, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPRootLockTest, "GSTNInput.NP.Simulator.Locks.Root", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPRootLockTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CommandBindingSet> Set = MakeLockSet();
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeNPSettings();
	const FGSTN_CommandTable Table = Set->BuildTable();
	const FGSTN_ButtonMap Buttons = MakeTwoButtons();
	FMockHost Host;

	TestTrue(TEXT("the table worked out the attack's lock tags"), Table.Commands[1].LockTags.HasTagExact(TestLockAttack));
	TestFalse(TEXT("the dash is not under the attack's lock"), Table.Commands[2].LockTags.HasTagExact(TestLockAttack));

	Host.OwnerTags.AddTag(TestLockAttack);
	TestTrue(TEXT("the attack is locked"), Table.IsCommandLocked(1, Host.OwnerTags));
	TestFalse(TEXT("the dash is free"), Table.IsCommandLocked(2, Host.OwnerTags));
	TestFalse(TEXT("an index outside the table is not locked"), Table.IsCommandLocked(7, Host.OwnerTags));

	// A stance: the attack press does nothing, the dash still starts, and the attack press waits for the stance to end
	FGSTN_InputSimState State;
	TestFalse(TEXT("attack press under the lock"), RunFrame(State, 0, 1, Host, Table, Buttons, *Settings).HasRequest());
	const FGSTN_InputTickResult Dash = RunFrame(State, 1, 2, Host, Table, Buttons, *Settings);
	TestTrue(TEXT("the dash starts under the attack lock"), Dash.HasRequest());
	TestEqual(TEXT("it is the dash"), Dash.CommandIndex, 2);
	RunFrame(State, 2, 0, Host, Table, Buttons, *Settings);
	TestFalse(TEXT("still locked, nothing comes out"), RunFrame(State, 3, 0, Host, Table, Buttons, *Settings).HasRequest());

	Host.OwnerTags.Reset();
	const FGSTN_InputTickResult Late = RunFrame(State, 4, 0, Host, Table, Buttons, *Settings);
	TestTrue(TEXT("released within the buffer: the attack comes out"), Late.HasRequest());
	TestEqual(TEXT("it is the attack"), Late.CommandIndex, 1);

	// Too late: a press older than the buffer is gone even though the lock held it back
	FGSTN_InputSimState Stale;
	Host.OwnerTags.AddTag(TestLockAttack);
	RunFrame(Stale, 0, 1, Host, Table, Buttons, *Settings);
	for (int32 Frame = 1; Frame < 20; ++Frame)
	{
		RunFrame(Stale, Frame, 0, Host, Table, Buttons, *Settings);
	}
	Host.OwnerTags.Reset();
	TestFalse(TEXT("a press older than the buffer does not come out"), RunFrame(Stale, 20, 0, Host, Table, Buttons, *Settings).HasRequest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPCommandEventTest, "GSTNInput.NP.Simulator.CommandExecutedEvent", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPCommandEventTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CommandBindingSet> Set = MakeLockSet();
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeNPSettings();
	const FGSTN_CommandTable Table = Set->BuildTable();
	const FGSTN_ButtonMap Buttons = MakeTwoButtons();
	FMockHost Host;
	Host.ContextTags.AddTag(GSTNInputTags::Context_Airborne);

	// Frame 3 presses A (held to frame 7), frame 8 releases it while the abilities cannot start, frame 9 presses B (refused),
	// frame 10 forces the dash with the abilities available again
	auto Run = [&](FGSTN_SimEventOutbox& Outbox)
	{
		Host.bCanExecute = true;
		FGSTN_InputSimState State;
		for (int32 Frame = 0; Frame < 3; ++Frame)
		{
			RunFrame(State, Frame, 0, Host, Table, Buttons, *Settings, 0, &Outbox);
		}
		TestTrue(TEXT("idle frames publish nothing"), Outbox.IsEmpty());

		for (int32 Frame = 3; Frame < 8; ++Frame)
		{
			RunFrame(State, Frame, 1, Host, Table, Buttons, *Settings, 0, &Outbox);
		}
		TestEqual(TEXT("one event for the request, none while the button stays held"), Outbox.Events.Num(), 1);

		Host.bCanExecute = false;
		RunFrame(State, 8, 0, Host, Table, Buttons, *Settings, 0, &Outbox);
		RunFrame(State, 9, 2, Host, Table, Buttons, *Settings, 0, &Outbox);
		TestEqual(TEXT("an unavailable command leaves no event"), Outbox.Events.Num(), 1);

		Host.bCanExecute = true;
		RunFrame(State, 10, 0, Host, Table, Buttons, *Settings, Table.ToForcedCommand(TestCommandDash), &Outbox);
	};

	FGSTN_SimEventOutbox First;
	Run(First);

	// The attack on frame 3 and the forced dash on frame 10; the refused press of frame 9 stays in the buffer and has no event yet
	if (!TestTrue(TEXT("the attack and the dash left events"), First.Events.Num() == 2))
	{
		return false;
	}

	const FGSTN_SimEvent& Attack = First.Events[0];
	TestTrue(TEXT("event type"), Attack.EventType == GSTNInputTags::Event_CommandExecuted.GetTag());
	TestEqual(TEXT("frame of the request"), Attack.Frame, 3);
	TestTrue(TEXT("the command tag"), Attack.Tags.HasTagExact(TestCommandAttack));
	TestTrue(TEXT("the context tags"), Attack.Tags.HasTagExact(GSTNInputTags::Context_Airborne));
	TestFalse(TEXT("not another command"), Attack.Tags.HasTagExact(TestCommandDash));
	TestTrue(TEXT("the second event names the dash"), First.Events[1].Tags.HasTagExact(TestCommandDash));

	// Deterministic: the same inputs make the same events, which is what lets a re-simulation rewrite them harmlessly
	FGSTN_SimEventOutbox Second;
	Run(Second);
	if (TestEqual(TEXT("replay: same number of events"), Second.Events.Num(), First.Events.Num()))
	{
		for (int32 Index = 0; Index < First.Events.Num(); ++Index)
		{
			TestEqual(*FString::Printf(TEXT("replay: frame of event %d"), Index), Second.Events[Index].Frame, First.Events[Index].Frame);
			TestTrue(*FString::Printf(TEXT("replay: tags of event %d"), Index), Second.Events[Index].Tags == First.Events[Index].Tags);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPActivationContextTest, "GSTNInput.NP.Context.ActivationContext", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPActivationContextTest::RunTest(const FString& Parameters)
{
	const TStrongObjectPtr<UGSTN_CommandBindingSet> Set = MakeLockSet();
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeNPSettings();
	const FGSTN_CommandTable Table = Set->BuildTable();
	const FGSTN_ButtonMap Buttons = MakeTwoButtons();
	FMockHost Host;

	// A press of B on the frame it is made
	FGSTN_InputSimState State;
	RunFrame(State, 9, 0, Host, Table, Buttons, *Settings);
	RunFrame(State, 10, 2, Host, Table, Buttons, *Settings);
	FGSTN_ActivationContext Context = FGSTN_ActivationContext::FromRequest(State, Table, 10);
	TestTrue(TEXT("valid on the request's frame"), Context.IsValid());
	TestTrue(TEXT("the command"), Context.Command == TestCommandDash.GetTag());
	TestTrue(TEXT("started by the command"), Context.Source == EGSTN_ActivationSource::Command);
	TestEqual(TEXT("the trigger button is B"), Context.Button, 1);
	TestEqual(TEXT("the press frame"), Context.PressFrame, 10);

	// Held on: a later frame has no request of its own
	RunFrame(State, 11, 2, Host, Table, Buttons, *Settings);
	Context = FGSTN_ActivationContext::FromRequest(State, Table, 11);
	TestFalse(TEXT("invalid on a later frame"), Context.IsValid());
	TestTrue(TEXT("with no source"), Context.Source == EGSTN_ActivationSource::Other);
	TestEqual(TEXT("and no button"), Context.Button, static_cast<int32>(INDEX_NONE));
	TestTrue(TEXT("the button of the state is still held"), FGSTN_InputHostUtils::IsButtonHeld(State, 1));
	TestFalse(TEXT("the other button is not"), FGSTN_InputHostUtils::IsButtonHeld(State, 0));
	TestFalse(TEXT("a negative button is not"), FGSTN_InputHostUtils::IsButtonHeld(State, -1));
	TestFalse(TEXT("a button past the mask is not"), FGSTN_InputHostUtils::IsButtonHeld(State, 32));
	RunFrame(State, 12, 0, Host, Table, Buttons, *Settings);
	TestFalse(TEXT("released"), FGSTN_InputHostUtils::IsButtonHeld(State, 1));

	// A buffered press: the context carries the frame of the press, not of the request
	FGSTN_InputSimState Buffered;
	Host.bCanExecute = false;
	RunFrame(Buffered, 5, 1, Host, Table, Buttons, *Settings);
	RunFrame(Buffered, 6, 0, Host, Table, Buttons, *Settings);
	Host.bCanExecute = true;
	RunFrame(Buffered, 7, 0, Host, Table, Buttons, *Settings);
	Context = FGSTN_ActivationContext::FromRequest(Buffered, Table, 7);
	TestTrue(TEXT("the buffered press starts the attack"), Context.Command == TestCommandAttack.GetTag());
	TestEqual(TEXT("its trigger button is A"), Context.Button, 0);
	TestEqual(TEXT("the press is from frame 5"), Context.PressFrame, 5);

	// A forced command has no input behind it
	FGSTN_InputSimState Forced;
	RunFrame(Forced, 0, 0, Host, Table, Buttons, *Settings);
	RunFrame(Forced, 1, 0, Host, Table, Buttons, *Settings, Table.ToForcedCommand(TestCommandDash));
	Context = FGSTN_ActivationContext::FromRequest(Forced, Table, 1);
	TestTrue(TEXT("forced"), Context.Source == EGSTN_ActivationSource::Forced);
	TestTrue(TEXT("the forced command"), Context.Command == TestCommandDash.GetTag());
	TestEqual(TEXT("no button"), Context.Button, static_cast<int32>(INDEX_NONE));

	// A command whose match has no press says so
	FGSTN_InputSimState NoButton = State;
	NoButton.RequestFrame = 20;
	NoButton.RequestedBinding = 1;
	NoButton.RequestButton = FGSTN_InputSimState::CommandWithoutButton;
	Context = FGSTN_ActivationContext::FromRequest(NoButton, Table, 20);
	TestTrue(TEXT("a command"), Context.Source == EGSTN_ActivationSource::Command);
	TestEqual(TEXT("without a trigger button"), Context.Button, static_cast<int32>(INDEX_NONE));

	// Garbage in the state does not index outside the table
	NoButton.RequestedBinding = 200;
	TestFalse(TEXT("an unknown binding gives no context"), FGSTN_ActivationContext::FromRequest(NoButton, Table, 20).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPStandaloneComponentTest, "GSTNInput.NP.Component.Standalone", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPStandaloneComponentTest::RunTest(const FString& Parameters)
{
	// No world: the component is only constructed, which is what a Blueprint or a pawn's default subobject does
	const TStrongObjectPtr<UGSTN_InputSimComponent> Component(NewObject<UGSTN_InputSimComponent>(GetTransientPackage()));

	TestTrue(TEXT("it is a command source"), Component->GetClass()->ImplementsInterface(UGSTN_CommandSourceInterface::StaticClass()));
	TestNotNull(TEXT("the default input source is an input source"), Cast<IGSTN_InputSource>(Component->InputSource.Get()));
	TestTrue(TEXT("the default context provider is the Mover one"), Component->ContextProvider && Component->ContextProvider->IsA<UGSTN_MoverInputContextProvider>());
	TestFalse(TEXT("not inert before BeginPlay"), Component->IsInert());
	TestTrue(TEXT("no commands before BeginPlay"), Component->GetCommandTable().IsEmpty());

	// Before the model is registered there is no state: the queries answer "nothing", they do not crash
	TestFalse(TEXT("no activation context without a state"), Component->GetActivationContext().IsValid());
	TestFalse(TEXT("no button held without a state"), Component->IsCommandButtonHeld(0));
	TestTrue(TEXT("a debug string without a state"), Component->GetDebugString().Contains(TEXT("no input state")));

	// The standalone model's state is the embedded input state and nothing else: it survives the wire
	const TStrongObjectPtr<UGSTN_CommandBindingSet> Set = MakeLockSet();
	const TStrongObjectPtr<UGSTN_InputSettings> Settings = MakeNPSettings();
	const FGSTN_CommandTable Table = Set->BuildTable();
	const FGSTN_ButtonMap Buttons = MakeTwoButtons();
	FMockHost Host;

	FGSTN_InputModelSyncState Sync;
	RunFrame(Sync.Input, 0, 0, Host, Table, Buttons, *Settings);
	RunFrame(Sync.Input, 1, 1, Host, Table, Buttons, *Settings);
	TestEqual(TEXT("a request was made"), static_cast<int32>(Sync.Input.RequestedBinding), 1);

	// The consumed stick anchor (a stick-only command that fired) is part of the history on the wire
	Sync.Input.History.ConsumeSticksUpTo(1);

	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	Sync.Input.NetSerialize(Writer);
	FGSTN_InputModelSyncState Loaded;
	FMemoryReader Reader(Bytes);
	Loaded.Input.NetSerialize(Reader);
	TestFalse(TEXT("reads back without error"), Reader.IsError());
	TestEqual(TEXT("request binding"), static_cast<int32>(Loaded.Input.RequestedBinding), static_cast<int32>(Sync.Input.RequestedBinding));
	TestEqual(TEXT("request button"), static_cast<int32>(Loaded.Input.RequestButton), static_cast<int32>(Sync.Input.RequestButton));
	TestEqual(TEXT("request frame"), Loaded.Input.RequestFrame, Sync.Input.RequestFrame);
	TestEqual(TEXT("last activated"), Loaded.Input.LastActivatedFrame, Sync.Input.LastActivatedFrame);
	TestTrue(TEXT("history"), Loaded.Input.History == Sync.Input.History);
	TestEqual(TEXT("consumed stick anchor"), Loaded.Input.History.GetLastConsumedStickFrame(), 1);

	// Interpolated proxies take the newer state
	FGSTN_InputModelSyncState Interpolated;
	Interpolated.Interpolate(&Loaded, &Sync, 0.5f);
	TestEqual(TEXT("interpolation takes the newer state"), Interpolated.Input.RequestFrame, Sync.Input.RequestFrame);

	// The input state is authoritative: the standalone model reconciles on any difference of it, and only then
	FGSTN_InputModelSyncState Authority = Loaded;
	TestFalse(TEXT("equal input states do not reconcile"), Loaded.ShouldReconcile(Authority));
	Authority.Input.RequestFrame += 1;
	TestTrue(TEXT("a different request frame reconciles"), Loaded.ShouldReconcile(Authority));
	Authority = Loaded;
	Authority.Input.LastActivatedFrame += 1;
	TestTrue(TEXT("a different last activated frame reconciles"), Loaded.ShouldReconcile(Authority));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
