// Automation tests of the victim's hit inbox (combat-m4-hits 7, M4 H6): drain order, rollback trim and where a hit is queued at all.

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "Simulation/GSTN_HitInbox.h"
#include "Tests/GSTN_CombatNPTests_InboxHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	FGSTN_HitInboxEntry InboxMakeEntry(int32 Frame, int32 AttackerId, uint8 Hitbox, uint8 Hurtbox = 0, float Damage = 0.f)
	{
		FGSTN_HitInboxEntry Entry;
		Entry.Frame = Frame;
		Entry.AttackerId = AttackerId;
		Entry.AttackerHitboxIndex = Hitbox;
		Entry.HurtboxIndex = Hurtbox;
		Entry.Hit.Damage = Damage;
		return Entry;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitInboxDrainOrderTest, "GSTNCombat.NP.Hits.Inbox.DrainOrder", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitInboxDrainOrderTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatInbox Inbox;
	// Written in an order no victim should depend on: the drain sorts by attacker id, then hitbox, then hurtbox
	GSTNInboxTestUtils::AddHit(Inbox, InboxMakeEntry(10, 5, 1));
	GSTNInboxTestUtils::AddHit(Inbox, InboxMakeEntry(10, 3, 2));
	GSTNInboxTestUtils::AddHit(Inbox, InboxMakeEntry(10, 3, 0, 1));
	GSTNInboxTestUtils::AddHit(Inbox, InboxMakeEntry(10, 3, 0, 0));
	GSTNInboxTestUtils::AddHit(Inbox, InboxMakeEntry(11, 1, 0));
	GSTNInboxTestUtils::AddHit(Inbox, InboxMakeEntry(10, 3, 0, 0, 7.f));

	TArray<FGSTN_HitInboxEntry> Drained;
	GSTNInboxTestUtils::DrainHits(Inbox, 11, Drained);
	TestEqual(TEXT("the entries of frame 10 are drained at frame 11"), Drained.Num(), 5);
	TestEqual(TEXT("the entry of frame 11 stays: it is delivered at 12"), Inbox.Num(), 1);
	if (Drained.Num() == 5)
	{
		TestEqual(TEXT("first: attacker 3, hitbox 0, hurtbox 0"), Drained[0].AttackerId * 100 + Drained[0].AttackerHitboxIndex * 10 + Drained[0].HurtboxIndex, 300);
		TestEqual(TEXT("equal keys keep the arrival order"), Drained[0].Hit.Damage, 0.f);
		TestEqual(TEXT("second: the same keys, written later"), Drained[1].Hit.Damage, 7.f);
		TestEqual(TEXT("third: attacker 3, hitbox 0, hurtbox 1"), Drained[2].HurtboxIndex, static_cast<uint8>(1));
		TestEqual(TEXT("fourth: attacker 3, hitbox 2"), Drained[3].AttackerHitboxIndex, static_cast<uint8>(2));
		TestEqual(TEXT("fifth: attacker 5"), Drained[4].AttackerId, 5);
	}

	TArray<FGSTN_HitInboxEntry> Later;
	GSTNInboxTestUtils::DrainHits(Inbox, 11, Later);
	TestTrue(TEXT("a second drain at the same frame finds nothing"), Later.IsEmpty());
	GSTNInboxTestUtils::DrainHits(Inbox, 12, Later);
	TestEqual(TEXT("the entry of frame 11 comes at frame 12"), Later.Num(), 1);
	TestTrue(TEXT("and the inbox is empty"), Inbox.IsEmpty());

	// A victim that missed a frame (its tick did not run) still gets the older entries first, in the same order
	GSTNInboxTestUtils::AddHit(Inbox, InboxMakeEntry(20, 9, 0));
	GSTNInboxTestUtils::AddHit(Inbox, InboxMakeEntry(21, 2, 0));
	TArray<FGSTN_HitInboxEntry> Behind;
	GSTNInboxTestUtils::DrainHits(Inbox, 23, Behind);
	TestEqual(TEXT("both entries are older than the frame"), Behind.Num(), 2);
	TestEqual(TEXT("ordered by attacker, not by frame"), Behind.Num() == 2 ? Behind[0].AttackerId : INDEX_NONE, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitInboxRollbackTrimTest, "GSTNCombat.NP.Hits.Inbox.RollbackTrim", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitInboxRollbackTrimTest::RunTest(const FString& Parameters)
{
	FGSTN_CombatInbox Inbox;
	for (int32 Frame = 8; Frame <= 12; ++Frame)
	{
		GSTNInboxTestUtils::AddHit(Inbox, InboxMakeEntry(Frame, 4, 0));
	}
	Inbox.TrimFrom(10);
	TestEqual(TEXT("the entries from the first re-simulated frame on are dropped"), Inbox.Num(), 2);
	for (const FGSTN_InboxEntry& Entry : Inbox.GetEntries())
	{
		TestTrue(TEXT("the ones that stay are older"), Entry.Frame < 10);
	}
	Inbox.TrimFrom(0);
	TestTrue(TEXT("a trim from the start empties it"), Inbox.IsEmpty());

	// The component restores through RestoreFrame: the state is the end of frame 9, the re-simulation starts at 10
	AActor* Owner = NewObject<AActor>(GetTransientPackage());
	Owner->SetRole(ROLE_Authority);
	UGSTN_CombatSimComponent* Victim = NewObject<UGSTN_CombatSimComponent>(Owner);
	for (int32 Frame = 8; Frame <= 12; ++Frame)
	{
		Victim->PostHit(InboxMakeEntry(Frame, 4, 0));
	}
	FGSTN_CombatSyncState Restored;
	FGSTN_InputSample Sample;
	Sample.Frame = 9;
	Restored.Core.Input.History.Append(Sample);
	Victim->RestoreFrame(&Restored, nullptr);
	TestEqual(TEXT("the component keeps the entries up to the restored frame"), Victim->GetInbox().Num(), 2);

	Victim->RestoreFrame(nullptr, nullptr);
	TestTrue(TEXT("a restore without a state empties it"), Victim->GetInbox().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitInboxOnlyWhereTicksTest, "GSTNCombat.NP.Hits.Inbox.OnlyWhereInstanceTicks", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitInboxOnlyWhereTicksTest::RunTest(const FString& Parameters)
{
	AActor* Owner = NewObject<AActor>(GetTransientPackage());
	UGSTN_CombatSimComponent* Victim = NewObject<UGSTN_CombatSimComponent>(Owner);

	Owner->SetRole(ROLE_SimulatedProxy);
	TestFalse(TEXT("an interpolated proxy does not tick"), Victim->TicksOnThisMachine());
	TestFalse(TEXT("a hit is not queued for it"), Victim->PostHit(InboxMakeEntry(5, 1, 0)));
	TestTrue(TEXT("nothing is there"), Victim->GetInbox().IsEmpty());

	Owner->SetRole(ROLE_AutonomousProxy);
	TestTrue(TEXT("the locally controlled pawn ticks"), Victim->TicksOnThisMachine());
	TestTrue(TEXT("a hit is queued for it"), Victim->PostHit(InboxMakeEntry(5, 1, 0)));

	Owner->SetRole(ROLE_Authority);
	TestTrue(TEXT("the authority ticks"), Victim->TicksOnThisMachine());
	TestTrue(TEXT("a hit is queued for it"), Victim->PostHit(InboxMakeEntry(5, 2, 0)));
	TestEqual(TEXT("two hits wait"), Victim->GetInbox().Num(), 2);

	// A component with no owner (it ticks nowhere)
	UGSTN_CombatSimComponent* Orphan = NewObject<UGSTN_CombatSimComponent>(GetTransientPackage());
	TestFalse(TEXT("a component without an owner does not tick"), Orphan->PostHit(InboxMakeEntry(5, 1, 0)));

	// The inbox has a bound: a frame full of overlaps cannot grow it without end
	FGSTN_CombatInbox Full;
	for (int32 Index = 0; Index < FGSTN_CombatInbox::MaxEntries; ++Index)
	{
		TestTrue(TEXT("room"), GSTNInboxTestUtils::AddHit(Full, InboxMakeEntry(1, Index, 0)));
	}
	TestFalse(TEXT("the entry over the bound is dropped"), GSTNInboxTestUtils::AddHit(Full, InboxMakeEntry(1, 1000, 0)));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
