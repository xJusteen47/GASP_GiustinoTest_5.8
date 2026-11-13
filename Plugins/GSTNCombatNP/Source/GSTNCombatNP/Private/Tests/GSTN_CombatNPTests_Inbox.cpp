// Automation tests of the open combat inbox (M6 F1): the order of mixed kinds, an unregistered kind, the rollback trim of every kind and
// where an entry is queued at all. The hit kind itself is covered by GSTN_CombatNPTests_HitInbox.cpp.

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Simulation/GSTN_CombatInbox.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_HitInbox.h"
#include "Sync/GSTN_EntityRef.h"
#include "Tests/GSTN_CombatNPTests_InboxHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** A kind of the test range whose payload is an FGSTN_EntityRef (a reflected struct is all an instanced struct needs); Id is the order key, Serial marks the arrival */
	FGSTN_InboxEntry MakeTestEntry(uint8 Kind, int32 Frame, int32 SenderId, uint16 Key, uint8 Marker = 0)
	{
		FGSTN_InboxEntry Entry;
		Entry.Kind = Kind;
		Entry.Frame = Frame;
		Entry.SenderId = SenderId;
		Entry.Payload = FInstancedStruct::Make<FGSTN_EntityRef>(FGSTN_EntityRef(GSTNEntityKinds::FirstTest, Key, Marker));
		return Entry;
	}

	FGSTN_HitInboxEntry MakeHit(int32 Frame, int32 AttackerId, uint8 Hitbox = 0)
	{
		FGSTN_HitInboxEntry Hit;
		Hit.Frame = Frame;
		Hit.AttackerId = AttackerId;
		Hit.AttackerHitboxIndex = Hitbox;
		return Hit;
	}

	/** Registers the two test kinds for the life of a test and removes them, so no other test sees them */
	struct FTestKindsScope
	{
		TArray<FGSTN_InboxEntry> Handled;

		FTestKindsScope()
		{
			FGSTN_InboxKindRegistry& Registry = FGSTN_InboxKindRegistry::Get();
			const auto Handler = [this](const FGSTN_InboxEntry& Entry, const FGSTN_InboxDispatchContext& Context)
			{
				Handled.Add(Entry);
			};
			const auto Key = [](const FGSTN_InboxEntry& Entry) -> uint32
			{
				const FGSTN_EntityRef* Ref = Entry.Payload.GetPtr<FGSTN_EntityRef>();
				return Ref ? Ref->Id : 0u;
			};
			Registry.Register(GSTNInboxKinds::TestA, TEXT("TestA"), Handler, Key);
			Registry.Register(GSTNInboxKinds::TestB, TEXT("TestB"), Handler, Key);
		}

		~FTestKindsScope()
		{
			FGSTN_InboxKindRegistry::Get().Unregister(GSTNInboxKinds::TestA);
			FGSTN_InboxKindRegistry::Get().Unregister(GSTNInboxKinds::TestB);
		}
	};

	uint16 KeyOf(const FGSTN_InboxEntry& Entry)
	{
		const FGSTN_EntityRef* Ref = Entry.Payload.GetPtr<FGSTN_EntityRef>();
		return Ref ? Ref->Id : 0;
	}

	uint8 MarkerOf(const FGSTN_InboxEntry& Entry)
	{
		const FGSTN_EntityRef* Ref = Entry.Payload.GetPtr<FGSTN_EntityRef>();
		return Ref ? Ref->Serial : 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InboxKindOrderStableTest, "GSTNCombat.NP.Inbox.KindOrderStable", GSTN_NP_TEST_FLAGS)
bool FGSTN_InboxKindOrderStableTest::RunTest(const FString& Parameters)
{
	FTestKindsScope Kinds;
	FGSTN_CombatInbox Inbox;

	// Written in an order no receiver should depend on: the drain sorts by sender, then kind, then the kind's key, then arrival
	Inbox.Add(MakeTestEntry(GSTNInboxKinds::TestB, 10, 3, 1));
	Inbox.Add(MakeTestEntry(GSTNInboxKinds::TestA, 10, 3, 9));
	Inbox.Add(MakeTestEntry(GSTNInboxKinds::TestA, 10, 3, 2, 1));
	GSTNInboxTestUtils::AddHit(Inbox, MakeHit(10, 3, 0));
	Inbox.Add(MakeTestEntry(GSTNInboxKinds::TestB, 10, 2, 5));
	Inbox.Add(MakeTestEntry(GSTNInboxKinds::TestA, 10, 3, 2, 2));
	Inbox.Add(MakeTestEntry(GSTNInboxKinds::TestA, 11, 1, 0));

	TArray<FGSTN_InboxEntry> Drained;
	Inbox.Drain(11, Drained);
	TestEqual(TEXT("the six entries of frame 10 are drained at frame 11"), Drained.Num(), 6);
	TestEqual(TEXT("the entry of frame 11 stays"), Inbox.Num(), 1);
	if (Drained.Num() == 6)
	{
		TestEqual(TEXT("first: sender 2"), Drained[0].SenderId, 2);
		TestEqual(TEXT("then sender 3, kind Hit first"), static_cast<int32>(Drained[1].Kind), static_cast<int32>(GSTNInboxKinds::Hit));
		TestEqual(TEXT("then kind TestA with the lowest key"), KeyOf(Drained[2]), static_cast<uint16>(2));
		TestEqual(TEXT("equal keys keep the arrival order (first written)"), MarkerOf(Drained[2]), static_cast<uint8>(1));
		TestEqual(TEXT("equal keys keep the arrival order (second written)"), MarkerOf(Drained[3]), static_cast<uint8>(2));
		TestEqual(TEXT("then the higher key"), KeyOf(Drained[4]), static_cast<uint16>(9));
		TestEqual(TEXT("last: kind TestB of sender 3"), static_cast<int32>(Drained[5].Kind), static_cast<int32>(GSTNInboxKinds::TestB));
	}

	// The same entries in another arrival order give the same drain: nothing but the keys decides
	FGSTN_CombatInbox Reversed;
	Reversed.Add(MakeTestEntry(GSTNInboxKinds::TestA, 10, 3, 2, 2));
	Reversed.Add(MakeTestEntry(GSTNInboxKinds::TestB, 10, 2, 5));
	GSTNInboxTestUtils::AddHit(Reversed, MakeHit(10, 3, 0));
	Reversed.Add(MakeTestEntry(GSTNInboxKinds::TestA, 10, 3, 9));
	Reversed.Add(MakeTestEntry(GSTNInboxKinds::TestB, 10, 3, 1));
	TArray<FGSTN_InboxEntry> Again;
	Reversed.Drain(11, Again);
	TestEqual(TEXT("five entries"), Again.Num(), 5);
	if (Again.Num() == 5)
	{
		TestEqual(TEXT("sender 2 first"), Again[0].SenderId, 2);
		TestEqual(TEXT("the hit next"), static_cast<int32>(Again[1].Kind), static_cast<int32>(GSTNInboxKinds::Hit));
		TestEqual(TEXT("TestA key 2"), KeyOf(Again[2]), static_cast<uint16>(2));
		TestEqual(TEXT("TestA key 9"), KeyOf(Again[3]), static_cast<uint16>(9));
		TestEqual(TEXT("TestB last"), static_cast<int32>(Again[4].Kind), static_cast<int32>(GSTNInboxKinds::TestB));
	}

	// The handlers see the entries in the drain order
	FGSTN_InboxDispatchContext Context;
	Context.Frame = 11;
	for (const FGSTN_InboxEntry& Entry : Drained)
	{
		TestTrue(TEXT("a registered kind is dispatched"), FGSTN_InboxKindRegistry::Get().Dispatch(Entry, Context));
	}
	TestEqual(TEXT("the hit kind has no handler, the other five entries each reach theirs"), Kinds.Handled.Num(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InboxUnknownKindIgnoredTest, "GSTNCombat.NP.Inbox.UnknownKindIgnoredWithWarning", GSTN_NP_TEST_FLAGS)
bool FGSTN_InboxUnknownKindIgnoredTest::RunTest(const FString& Parameters)
{
	FGSTN_InboxKindRegistry& Registry = FGSTN_InboxKindRegistry::Get();
	Registry.ResetWarnings();
	constexpr uint8 UnknownKind = 250;
	TestNull(TEXT("nothing registered under the test id"), Registry.Find(UnknownKind));

	// One warning per kind, not one per entry
	AddExpectedMessagePlain(TEXT("no handler is registered for entry kind 250"), ELogVerbosity::Warning, EAutomationExpectedMessageFlags::Contains, 1);

	FGSTN_InboxDispatchContext Context;
	Context.Frame = 5;
	TestFalse(TEXT("an unknown kind is ignored"), Registry.Dispatch(MakeTestEntry(UnknownKind, 4, 7, 1), Context));
	TestFalse(TEXT("and again"), Registry.Dispatch(MakeTestEntry(UnknownKind, 4, 8, 1), Context));

	// It does not hurt the entries of known kinds around it
	FTestKindsScope Kinds;
	TestTrue(TEXT("a known kind still dispatches"), Registry.Dispatch(MakeTestEntry(GSTNInboxKinds::TestA, 4, 7, 1), Context));
	TestEqual(TEXT("its handler ran once"), Kinds.Handled.Num(), 1);

	// An unknown kind rides through the container like any other and is dropped only by the dispatch
	FGSTN_CombatInbox Inbox;
	Inbox.Add(MakeTestEntry(UnknownKind, 4, 7, 1));
	TArray<FGSTN_InboxEntry> Drained;
	Inbox.Drain(5, Drained);
	TestEqual(TEXT("the container drains it"), Drained.Num(), 1);

	// A kind id is registered once
	TestFalse(TEXT("a second registration of a taken id is refused"), Registry.Register(GSTNInboxKinds::TestA, TEXT("Other"), FGSTN_InboxKindRegistry::FHandler()));
	TestFalse(TEXT("the hit kind is taken by the combat plugin"), Registry.Register(GSTNInboxKinds::Hit, TEXT("Other"), FGSTN_InboxKindRegistry::FHandler()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InboxTrimOnRollbackAllKindsTest, "GSTNCombat.NP.Inbox.TrimOnRollbackAllKinds", GSTN_NP_TEST_FLAGS)
bool FGSTN_InboxTrimOnRollbackAllKindsTest::RunTest(const FString& Parameters)
{
	FTestKindsScope Kinds;
	FGSTN_CombatInbox Inbox;
	for (int32 Frame = 8; Frame <= 12; ++Frame)
	{
		GSTNInboxTestUtils::AddHit(Inbox, MakeHit(Frame, 4));
		Inbox.Add(MakeTestEntry(GSTNInboxKinds::TestA, Frame, 4, 1));
		Inbox.Add(MakeTestEntry(GSTNInboxKinds::TestB, Frame, 5, 1));
	}
	Inbox.TrimFrom(10);
	TestEqual(TEXT("the entries of every kind from the first re-simulated frame on are dropped"), Inbox.Num(), 6);
	for (const FGSTN_InboxEntry& Entry : Inbox.GetEntries())
	{
		TestTrue(TEXT("the ones that stay are older"), Entry.Frame < 10);
	}
	Inbox.TrimFrom(0);
	TestTrue(TEXT("a trim from the start empties it"), Inbox.IsEmpty());

	// The component restores through RestoreFrame: the state is the end of frame 9, the re-simulation starts at 10
	AActor* Owner = NewObject<AActor>(GetTransientPackage());
	Owner->SetRole(ROLE_Authority);
	UGSTN_CombatSimComponent* Receiver = NewObject<UGSTN_CombatSimComponent>(Owner);
	for (int32 Frame = 8; Frame <= 12; ++Frame)
	{
		Receiver->PostToInbox(MakeTestEntry(GSTNInboxKinds::TestA, Frame, 4, 1));
		Receiver->PostHit(MakeHit(Frame, 4));
	}
	FGSTN_CombatSyncState Restored;
	FGSTN_InputSample Sample;
	Sample.Frame = 9;
	Restored.Core.Input.History.Append(Sample);
	Receiver->RestoreFrame(&Restored, nullptr);
	TestEqual(TEXT("the component keeps the entries of both kinds up to the restored frame"), Receiver->GetInbox().Num(), 4);

	Receiver->RestoreFrame(nullptr, nullptr);
	TestTrue(TEXT("a restore without a state empties it"), Receiver->GetInbox().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_InboxPostRefusedWhereNotTickingTest, "GSTNCombat.NP.Inbox.PostToInboxRefusedWhereNotTicking", GSTN_NP_TEST_FLAGS)
bool FGSTN_InboxPostRefusedWhereNotTickingTest::RunTest(const FString& Parameters)
{
	FTestKindsScope Kinds;
	AActor* Owner = NewObject<AActor>(GetTransientPackage());
	UGSTN_CombatSimComponent* Receiver = NewObject<UGSTN_CombatSimComponent>(Owner);

	Owner->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("an entry of an open kind is not queued for an interpolated proxy"), Receiver->PostToInbox(MakeTestEntry(GSTNInboxKinds::TestA, 5, 1, 1)));
	TestFalse(TEXT("nor a hit wrapped as an entry"), Receiver->PostToInbox(MakeHit(5, 1).ToInboxEntry()));
	TestTrue(TEXT("nothing is there"), Receiver->GetInbox().IsEmpty());

	Owner->SetRole(ROLE_AutonomousProxy);
	TestTrue(TEXT("the locally controlled pawn takes it"), Receiver->PostToInbox(MakeTestEntry(GSTNInboxKinds::TestA, 5, 1, 1)));
	Owner->SetRole(ROLE_Authority);
	TestTrue(TEXT("the authority takes it"), Receiver->PostToInbox(MakeTestEntry(GSTNInboxKinds::TestB, 5, 2, 1)));
	TestTrue(TEXT("PostHit is the same call for the hit kind"), Receiver->PostHit(MakeHit(5, 3)));
	TestEqual(TEXT("three entries wait, of three kinds"), Receiver->GetInbox().Num(), 3);

	// A component with no owner ticks nowhere
	UGSTN_CombatSimComponent* Orphan = NewObject<UGSTN_CombatSimComponent>(GetTransientPackage());
	TestFalse(TEXT("a component without an owner takes nothing"), Orphan->PostToInbox(MakeTestEntry(GSTNInboxKinds::TestA, 5, 1, 1)));

	// The sender side: with no combat world to find the target in, nothing is posted
	TestFalse(TEXT("an unknown target takes nothing"), Receiver->PostToInbox(42, MakeTestEntry(GSTNInboxKinds::TestA, 5, 1, 1)));

	// The bound is shared by every kind
	FGSTN_CombatInbox Full;
	for (int32 Index = 0; Index < FGSTN_CombatInbox::MaxEntries; ++Index)
	{
		TestTrue(TEXT("room"), Full.Add(MakeTestEntry(Index % 2 ? GSTNInboxKinds::TestA : GSTNInboxKinds::TestB, 1, Index, 1)));
	}
	TestFalse(TEXT("the entry over the bound is dropped, whatever its kind"), Full.Add(MakeHit(1, 1000).ToInboxEntry()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
