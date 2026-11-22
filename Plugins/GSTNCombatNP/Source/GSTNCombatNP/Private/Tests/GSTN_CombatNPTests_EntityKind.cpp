// Automation tests of the entity kind dispatch (M8 G1, X5): the minion id range of GSTNEntityKinds, FindEntity on the kind bit, inert without a
// provider of the kind, and the generation check on delivery. No world: the subsystem is a plain object, the provider is a stub.

#include "Misc/AutomationTest.h"
#include "Simulation/GSTN_HitInbox.h"
#include "World/GSTN_CombatWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	struct FKindFakeSource : IGSTN_SnapshotSource
	{
		int32 Posted = 0;
		virtual bool ReadSnapshotEntry(int32 Frame, FGSTN_SnapshotEntry& Out) override { return true; }
		virtual bool PostToInbox(const FGSTN_InboxEntry& Entry) override { ++Posted; return true; }
	};

	/** A store of two slots: slot index -> current generation; a post is taken only when the generation matches */
	struct FKindStubProvider : IGSTN_HittableProvider
	{
		int32 ProviderId = 1;
		uint8 Kind = GSTNEntityKinds::Minion;
		uint16 Generations[2] = { 3, 7 };
		TArray<int32> Taken;

		virtual int32 GetProviderId() const override { return ProviderId; }
		virtual uint8 GetEntityKind() const override { return Kind; }

		virtual void AppendSnapshotEntities(int32 Frame, TArray<FGSTN_SnapshotEntry>& OutEntries) override
		{
			for (uint16 Index = 0; Index < 2; ++Index)
			{
				FGSTN_SnapshotEntry& Entry = OutEntries.AddDefaulted_GetRef();
				Entry.EntityId = GSTNEntityKinds::MakeMinionEntityId(Index);
				Entry.Generation = Generations[Index];
			}
		}

		virtual bool PostToEntityInbox(int32 EntityId, uint16 Generation, const FGSTN_InboxEntry& Entry) override
		{
			const uint16 Index = GSTNEntityKinds::MinionIndexOf(EntityId);
			if (Index >= 2 || Generations[Index] != Generation)
			{
				return false;
			}
			Taken.Add(EntityId);
			return true;
		}
	};

	FGSTN_InboxEntry MakeKindHit(int32 Frame)
	{
		FGSTN_HitInboxEntry Hit;
		Hit.Frame = Frame;
		Hit.AttackerId = 4;
		return Hit.ToInboxEntry();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EntityKindIdRangeTest, "GSTNCombat.NP.EntityKind.IdRange", GSTN_NP_TEST_FLAGS)
bool FGSTN_EntityKindIdRangeTest::RunTest(const FString& Parameters)
{
	using namespace GSTNEntityKinds;
	TestEqual(TEXT("the minion base is bit 30"), MinionEntityBase, 0x40000000);
	TestTrue(TEXT("slot 0 is a minion id"), IsMinionEntityId(MakeMinionEntityId(0)));
	TestEqual(TEXT("the slot index round trips"), static_cast<int32>(MinionIndexOf(MakeMinionEntityId(65535))), 65535);
	TestFalse(TEXT("an NP proxy id is not"), IsMinionEntityId(12345));
	TestEqual(TEXT("an NP proxy id is a fighter"), KindOfEntityId(12345), Fighter);
	TestEqual(TEXT("a minion id is a minion"), KindOfEntityId(MakeMinionEntityId(9)), Minion);
	TestEqual(TEXT("INDEX_NONE is no kind"), KindOfEntityId(INDEX_NONE), None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_EntityKindDispatchTest, "GSTNCombat.NP.EntityKind.Dispatch", GSTN_NP_TEST_FLAGS)
bool FGSTN_EntityKindDispatchTest::RunTest(const FString& Parameters)
{
	UGSTN_CombatWorldSubsystem* Subsystem = NewObject<UGSTN_CombatWorldSubsystem>(GetTransientPackage());
	FKindFakeSource Fighter;
	Subsystem->RegisterEntity(5, Fighter, nullptr, FGenericTeamId(1), nullptr);
	const int32 Slot1 = GSTNEntityKinds::MakeMinionEntityId(1);

	// Inert with no provider of the kind
	TestFalse(TEXT("a minion id is not found without a provider"), Subsystem->FindEntity(Slot1).IsFound());
	TestFalse(TEXT("and takes no post"), Subsystem->PostToInbox(Slot1, MakeKindHit(1)));
	FKindStubProvider Untyped;
	Untyped.ProviderId = 0;
	Untyped.Kind = GSTNEntityKinds::None;
	Subsystem->RegisterProvider(&Untyped);
	TestFalse(TEXT("a provider of no kind owns no range"), Subsystem->FindEntity(Slot1).IsFound());
	Subsystem->UnregisterProvider(&Untyped);

	FKindStubProvider Store;
	Subsystem->RegisterProvider(&Store);
	const FGSTN_FoundEntity Found = Subsystem->FindEntity(Slot1);
	TestTrue(TEXT("the minion id goes to the provider of the kind"), Found.Provider == &Store && Found.Source == nullptr);
	TestEqual(TEXT("found as a minion"), Found.Kind, GSTNEntityKinds::Minion);
	const FGSTN_FoundEntity FoundFighter = Subsystem->FindEntity(5);
	TestTrue(TEXT("a fighter id still goes to the registry"), FoundFighter.Source == &Fighter && FoundFighter.Kind == GSTNEntityKinds::Fighter);
	TestFalse(TEXT("an unknown fighter id is not found"), Subsystem->FindEntity(6).IsFound());

	// Delivery against the held snapshot carries the generation the sender saw
	const FGSTN_StepSnapshot& Snapshot = Subsystem->EnsureSnapshot(1);
	TestEqual(TEXT("the snapshot holds the fighter and both slots"), Snapshot.Entries.Num(), 3);
	TestTrue(TEXT("a hit on the live slot is taken"), Subsystem->PostToInbox(Slot1, MakeKindHit(1)));
	TestTrue(TEXT("a fighter hit is taken by its source"), Subsystem->PostToInbox(5, MakeKindHit(1)) && Fighter.Posted == 1);

	// The slot is freed and reused before the hit arrives: the stamp no longer matches
	Store.Generations[1] = 8;
	TestFalse(TEXT("a hit stamped for the old generation is dropped"), Subsystem->PostToInbox(Slot1, 7, MakeKindHit(1)));
	TestTrue(TEXT("one stamped for the new one is taken"), Subsystem->PostToInbox(Slot1, 8, MakeKindHit(1)));
	TestEqual(TEXT("two posts reached the store"), Store.Taken.Num(), 2);

	Subsystem->UnregisterProvider(&Store);
	TestFalse(TEXT("unregistered, the range is inert again"), Subsystem->FindEntity(Slot1).IsFound());
	return true;
}

#endif
