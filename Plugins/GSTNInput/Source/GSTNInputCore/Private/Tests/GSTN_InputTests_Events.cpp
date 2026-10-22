#include "Tests/GSTN_TestUtils.h"
#include "Events/GSTN_SimEvent.h"
#include "Events/GSTN_SimEventSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGSTN_SimEvent MakeEvent(const FGameplayTag& Command, int32 Frame, std::initializer_list<FGameplayTag> Context = {})
	{
		FGSTN_SimEvent Event;
		Event.EventType = GSTNInputTags::Event_CommandExecuted.GetTag();
		Event.Tags.AddTag(Command);
		for (const FGameplayTag& Tag : Context)
		{
			Event.Tags.AddTag(Tag);
		}
		Event.Frame = Frame;
		return Event;
	}

	FGameplayTagQuery AnyOf(std::initializer_list<FGameplayTag> Tags)
	{
		FGameplayTagContainer Container;
		for (const FGameplayTag& Tag : Tags)
		{
			Container.AddTag(Tag);
		}
		return FGameplayTagQuery::MakeQuery_MatchAnyTags(Container);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EventOutboxTest, "GSTNInput.Core.Input.Events.Outbox", GSTN_CORE_TEST_FLAGS)
bool FGSTN_EventOutboxTest::RunTest(const FString& Parameters)
{
	FGSTN_SimEventOutbox Outbox;
	Outbox.Add(MakeEvent(GSTNTestTags::Attack_Neutral, 1));
	Outbox.Add(MakeEvent(GSTNTestTags::Strong_Neutral, 2));

	// The authority publishes in the order the events were made, and the outbox is empty after
	TArray<int32> Published;
	TestEqual(TEXT("two events published"), Outbox.Flush(true, [&Published](FGSTN_SimEvent& Event) { Published.Add(Event.Frame); }), 2);
	TestEqual(TEXT("in order (first)"), Published[0], 1);
	TestEqual(TEXT("in order (second)"), Published[1], 2);
	TestTrue(TEXT("emptied by the flush"), Outbox.IsEmpty());

	// Anywhere else (a predicting client, a re-simulation) the same events are dropped unseen
	Outbox.Add(MakeEvent(GSTNTestTags::Attack_Neutral, 3));
	int32 Calls = 0;
	TestEqual(TEXT("nothing published"), Outbox.Flush(false, [&Calls](FGSTN_SimEvent&) { ++Calls; }), 0);
	TestEqual(TEXT("the publisher is not called"), Calls, 0);
	TestTrue(TEXT("but the outbox is emptied, so a re-simulation cannot publish it later"), Outbox.IsEmpty());

	// A flush with nothing to publish is a no-op
	TestEqual(TEXT("empty flush"), Outbox.Flush(true, [&Calls](FGSTN_SimEvent&) { ++Calls; }), 0);

	// Capped: the oldest events go first
	for (int32 Frame = 0; Frame < FGSTN_SimEventOutbox::MaxEvents + 5; ++Frame)
	{
		Outbox.Add(MakeEvent(GSTNTestTags::Attack_Neutral, Frame));
	}
	TestEqual(TEXT("capped"), Outbox.Events.Num(), FGSTN_SimEventOutbox::MaxEvents);
	TestEqual(TEXT("the oldest were dropped"), Outbox.Events[0].Frame, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EventQueryTest, "GSTNInput.Core.Input.Events.Query", GSTN_CORE_TEST_FLAGS)
bool FGSTN_EventQueryTest::RunTest(const FString& Parameters)
{
	const FGSTN_SimEvent Neutral = MakeEvent(GSTNTestTags::Attack_Neutral, 1, { GSTNInputTags::Context_Airborne.GetTag() });

	TestTrue(TEXT("an empty query matches every event"), FGSTN_SimEventDispatcher::Matches(FGameplayTagQuery(), Neutral));
	TestTrue(TEXT("the command tag matches"), FGSTN_SimEventDispatcher::Matches(AnyOf({ GSTNTestTags::Attack_Neutral.GetTag() }), Neutral));
	TestTrue(TEXT("a parent of the command matches (hierarchical)"), FGSTN_SimEventDispatcher::Matches(AnyOf({ GSTNTestTags::Attack.GetTag() }), Neutral));
	TestFalse(TEXT("another command does not"), FGSTN_SimEventDispatcher::Matches(AnyOf({ GSTNTestTags::Strong_Neutral.GetTag() }), Neutral));
	TestTrue(TEXT("the event type is matched together with the tags"), FGSTN_SimEventDispatcher::Matches(AnyOf({ GSTNInputTags::Event_CommandExecuted.GetTag() }), Neutral));

	// "this command, in the air": one query over the command and the context
	FGameplayTagContainer Both;
	Both.AddTag(GSTNTestTags::Attack_Neutral.GetTag());
	Both.AddTag(GSTNInputTags::Context_Airborne.GetTag());
	const FGameplayTagQuery InAir = FGameplayTagQuery::MakeQuery_MatchAllTags(Both);
	TestTrue(TEXT("command and context both present"), FGSTN_SimEventDispatcher::Matches(InAir, Neutral));
	TestFalse(TEXT("same command on the ground"), FGSTN_SimEventDispatcher::Matches(InAir, MakeEvent(GSTNTestTags::Attack_Neutral, 2)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EventDispatcherTest, "GSTNInput.Core.Input.Events.Dispatcher", GSTN_CORE_TEST_FLAGS)
bool FGSTN_EventDispatcherTest::RunTest(const FString& Parameters)
{
	FGSTN_SimEventDispatcher Dispatcher;
	int32 AttackCount = 0;
	int32 AllCount = 0;
	const FGSTN_SimEventListenerHandle AttackHandle = Dispatcher.AddListener(AnyOf({ GSTNTestTags::Attack.GetTag() }), [&AttackCount](const FGSTN_SimEvent&) { ++AttackCount; });
	Dispatcher.AddListener(FGameplayTagQuery(), [&AllCount](const FGSTN_SimEvent&) { ++AllCount; });
	TestTrue(TEXT("a handle is valid"), AttackHandle.IsValid());
	TestFalse(TEXT("a default handle is not"), FGSTN_SimEventListenerHandle().IsValid());

	Dispatcher.Broadcast(MakeEvent(GSTNTestTags::Attack_Neutral, 1));
	Dispatcher.Broadcast(MakeEvent(GSTNTestTags::Strong_Neutral, 2));
	TestEqual(TEXT("the filtered listener sees one"), AttackCount, 1);
	TestEqual(TEXT("the unfiltered one sees both"), AllCount, 2);

	TestTrue(TEXT("removal"), Dispatcher.RemoveListener(AttackHandle));
	TestFalse(TEXT("removing twice"), Dispatcher.RemoveListener(AttackHandle));
	TestFalse(TEXT("removing an invalid handle"), Dispatcher.RemoveListener(FGSTN_SimEventListenerHandle()));
	Dispatcher.Broadcast(MakeEvent(GSTNTestTags::Attack_Neutral, 3));
	TestEqual(TEXT("a removed listener is not called"), AttackCount, 1);
	TestEqual(TEXT("the other still is"), AllCount, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EventReentrancyTest, "GSTNInput.Core.Input.Events.Reentrancy", GSTN_CORE_TEST_FLAGS)
bool FGSTN_EventReentrancyTest::RunTest(const FString& Parameters)
{
	FGSTN_SimEventDispatcher Dispatcher;
	int32 SecondCount = 0;
	int32 AddedCount = 0;
	FGSTN_SimEventListenerHandle SecondHandle;

	// The first listener removes the second and adds a third while the broadcast is running
	Dispatcher.AddListener(FGameplayTagQuery(), [&](const FGSTN_SimEvent&)
	{
		Dispatcher.RemoveListener(SecondHandle);
		Dispatcher.AddListener(FGameplayTagQuery(), [&AddedCount](const FGSTN_SimEvent&) { ++AddedCount; });
	});
	SecondHandle = Dispatcher.AddListener(FGameplayTagQuery(), [&SecondCount](const FGSTN_SimEvent&) { ++SecondCount; });

	Dispatcher.Broadcast(MakeEvent(GSTNTestTags::Attack_Neutral, 1));
	TestEqual(TEXT("a listener removed by an earlier callback is skipped"), SecondCount, 0);
	TestEqual(TEXT("a listener added during the broadcast waits for the next event"), AddedCount, 0);

	Dispatcher.Broadcast(MakeEvent(GSTNTestTags::Attack_Neutral, 2));
	TestEqual(TEXT("the added listener sees the next one"), AddedCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EventSubsystemTest, "GSTNInput.Core.Input.Events.Subsystem", GSTN_CORE_TEST_FLAGS)
bool FGSTN_EventSubsystemTest::RunTest(const FString& Parameters)
{
	// The subsystem needs no world to broadcast: it only owns a dispatcher
	const TStrongObjectPtr<UGSTN_SimEventSubsystem> Subsystem(NewObject<UGSTN_SimEventSubsystem>(GetTransientPackage()));

	TMap<FGameplayTag, int32> CountPerCommand;
	const FGSTN_SimEventListenerHandle Handle = Subsystem->ListenNative(AnyOf({ GSTNInputTags::Event_CommandExecuted.GetTag() }),
		[&CountPerCommand](const FGSTN_SimEvent& Event)
		{
			for (const FGameplayTag& Tag : Event.Tags)
			{
				++CountPerCommand.FindOrAdd(Tag);
			}
		});

	Subsystem->Broadcast(MakeEvent(GSTNTestTags::Attack_Neutral, 1));
	Subsystem->Broadcast(MakeEvent(GSTNTestTags::Attack_Neutral, 2));
	Subsystem->Broadcast(MakeEvent(GSTNTestTags::Strong_Neutral, 3));

	TestEqual(TEXT("counted per command"), CountPerCommand.FindRef(GSTNTestTags::Attack_Neutral.GetTag()), 2);
	TestEqual(TEXT("the other command"), CountPerCommand.FindRef(GSTNTestTags::Strong_Neutral.GetTag()), 1);

	TestTrue(TEXT("stop listening"), Subsystem->StopListening(Handle));
	Subsystem->Broadcast(MakeEvent(GSTNTestTags::Attack_Neutral, 4));
	TestEqual(TEXT("nothing after it"), CountPerCommand.FindRef(GSTNTestTags::Attack_Neutral.GetTag()), 2);

	TestNull(TEXT("no world, no subsystem"), UGSTN_SimEventSubsystem::Get(nullptr));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
