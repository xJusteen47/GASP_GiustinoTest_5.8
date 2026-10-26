// Automation tests of the world subsystem and its step snapshot (combat-m4-hits 5, M4 H6): built once per step, invalidated by
// RestoreFrame, the end-of-previous-frame rule, the provider order and the grid broadphase. They need no world: the subsystem is a
// plain object and the entities are stand-ins that implement the snapshot source interface.

#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Simulation/GSTN_CombatSimComponent.h"
#include "World/GSTN_CombatWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** A stand-in entity: reports the state it holds now, which a test changes to play the part of the tick that writes the next frame */
	struct FSnapshotFakeSource : IGSTN_SnapshotSource
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FVector Up = FVector::UpVector;
		int32 Reads = 0;

		virtual bool ReadSnapshotEntry(int32 Frame, FGSTN_SnapshotEntry& Out) override
		{
			++Reads;
			Out.EntityId = 12345;
			Out.Transform = FTransform(Location);
			Out.Velocity = Velocity;
			Out.Up = Up;
			return true;
		}
	};

	struct FSnapshotFakeProvider : IGSTN_HittableProvider
	{
		int32 ProviderId = 0;
		TArray<int32> EntityIds;

		virtual int32 GetProviderId() const override { return ProviderId; }

		virtual void AppendSnapshotEntities(int32 Frame, TArray<FGSTN_SnapshotEntry>& OutEntries) override
		{
			for (int32 Id : EntityIds)
			{
				FGSTN_SnapshotEntry& Entry = OutEntries.AddDefaulted_GetRef();
				Entry.EntityId = Id;
				// Which provider the entry came from, for the tie on a shared id
				Entry.Velocity = FVector(ProviderId, 0.0, 0.0);
			}
		}
	};

	UGSTN_HurtboxSetDefinition* MakeSphereSet(double Radius)
	{
		UGSTN_HurtboxSetDefinition* Set = NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage());
		FGSTN_Hurtbox Hurtbox;
		Hurtbox.Shape.Type = EGSTN_ShapeType::Sphere;
		Hurtbox.Shape.Extent = FVector(Radius, Radius, Radius);
		Set->Hurtboxes.Add(Hurtbox);
		return Set;
	}

	UGSTN_CombatWorldSubsystem* MakeSubsystem()
	{
		return NewObject<UGSTN_CombatWorldSubsystem>(GetTransientPackage());
	}

	TArray<int32> IdsOf(const FGSTN_StepSnapshot& Snapshot, const TArray<int32>& Indices)
	{
		TArray<int32> Ids;
		for (int32 Index : Indices)
		{
			Ids.Add(Snapshot.Entries[Index].EntityId);
		}
		return Ids;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitSnapshotBuiltOncePerStepTest, "GSTNCombat.NP.Hits.Snapshot.BuiltOncePerStep", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitSnapshotBuiltOncePerStepTest::RunTest(const FString& Parameters)
{
	UGSTN_CombatWorldSubsystem* Subsystem = MakeSubsystem();
	FSnapshotFakeSource A, B, C;
	// Registered out of id order on purpose
	TestTrue(TEXT("5 registers"), Subsystem->RegisterEntity(5, A, nullptr, FGenericTeamId(1), nullptr));
	TestTrue(TEXT("2 registers"), Subsystem->RegisterEntity(2, B, nullptr, FGenericTeamId(2), nullptr));
	TestTrue(TEXT("9 registers"), Subsystem->RegisterEntity(9, C, nullptr, FGenericTeamId(1), nullptr));
	TestFalse(TEXT("an id registers once"), Subsystem->RegisterEntity(5, B, nullptr, FGenericTeamId(1), nullptr));
	TestFalse(TEXT("an invalid id is refused"), Subsystem->RegisterEntity(INDEX_NONE, B, nullptr, FGenericTeamId(1), nullptr));
	TestEqual(TEXT("three entities"), Subsystem->GetNumRegisteredEntities(), 3);

	// Every combat tick of the step asks; the first builds
	for (int32 Attacker = 0; Attacker < 3; ++Attacker)
	{
		Subsystem->EnsureSnapshot(10);
	}
	TestEqual(TEXT("one build for the step"), Subsystem->GetSnapshotBuildCount(), 1);
	TestEqual(TEXT("each entity was read once"), A.Reads + B.Reads + C.Reads, 3);

	const FGSTN_StepSnapshot& Step10 = Subsystem->GetSnapshot();
	TestEqual(TEXT("the snapshot is of frame 10"), Step10.Frame, 10);
	TestEqual(TEXT("three entries"), Step10.Entries.Num(), 3);
	if (Step10.Entries.Num() == 3)
	{
		TestEqual(TEXT("sorted by id: first"), Step10.Entries[0].EntityId, 2);
		TestEqual(TEXT("sorted by id: second"), Step10.Entries[1].EntityId, 5);
		TestEqual(TEXT("sorted by id: third"), Step10.Entries[2].EntityId, 9);
		TestEqual(TEXT("the registry owns the id, not the source"), Step10.Find(5) != nullptr, true);
		TestEqual(TEXT("the team is the registered one"), static_cast<int32>(Step10.Find(2)->Team.GetId()), 2);
	}

	Subsystem->EnsureSnapshot(11);
	TestEqual(TEXT("the next frame builds again"), Subsystem->GetSnapshotBuildCount(), 2);
	Subsystem->EnsureSnapshot(11);
	TestEqual(TEXT("and once"), Subsystem->GetSnapshotBuildCount(), 2);

	// A registration changes who is in the step: the held snapshot is no longer right
	FSnapshotFakeSource D;
	Subsystem->RegisterEntity(7, D, nullptr, FGenericTeamId(1), nullptr);
	Subsystem->EnsureSnapshot(11);
	TestEqual(TEXT("a registration rebuilds"), Subsystem->GetSnapshotBuildCount(), 3);
	TestEqual(TEXT("with the newcomer sorted in"), Subsystem->GetSnapshot().Entries.Num(), 4);
	Subsystem->UnregisterEntity(7);
	Subsystem->EnsureSnapshot(11);
	TestEqual(TEXT("a removal rebuilds"), Subsystem->GetSnapshotBuildCount(), 4);
	TestEqual(TEXT("without it"), Subsystem->GetSnapshot().Entries.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitSnapshotInvalidatedByRestoreFrameTest, "GSTNCombat.NP.Hits.Snapshot.InvalidatedByRestoreFrame", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitSnapshotInvalidatedByRestoreFrameTest::RunTest(const FString& Parameters)
{
	UGSTN_CombatWorldSubsystem* Subsystem = MakeSubsystem();
	FSnapshotFakeSource Source;
	Source.Location = FVector(100.0, 0.0, 0.0);
	Subsystem->RegisterEntity(1, Source, nullptr, FGenericTeamId(1), nullptr);

	Subsystem->EnsureSnapshot(20);
	TestEqual(TEXT("built from the first state"), Subsystem->GetSnapshot().Entries[0].Transform.GetLocation().X, 100.0);

	// The simulation rolls back and the state changes; without the invalidation the same frame would be served from the old build
	Source.Location = FVector(300.0, 0.0, 0.0);
	Subsystem->EnsureSnapshot(20);
	TestEqual(TEXT("without an invalidation the frame keeps its snapshot"), Subsystem->GetSnapshot().Entries[0].Transform.GetLocation().X, 100.0);

	// The component's RestoreFrame is the invalidation
	AActor* Owner = NewObject<AActor>(GetTransientPackage());
	UGSTN_CombatSimComponent* Component = NewObject<UGSTN_CombatSimComponent>(Owner);
	Component->SetCombatWorldSubsystem(Subsystem);
	FGSTN_CombatSyncState Restored;
	Component->RestoreFrame(&Restored, nullptr);
	Subsystem->EnsureSnapshot(20);
	TestEqual(TEXT("after RestoreFrame the same frame is built again"), Subsystem->GetSnapshotBuildCount(), 2);
	TestEqual(TEXT("from the restored state"), Subsystem->GetSnapshot().Entries[0].Transform.GetLocation().X, 300.0);

	// And the re-simulation goes back in frames: an older frame after the invalidation is the normal case
	Component->RestoreFrame(&Restored, nullptr);
	Source.Location = FVector(50.0, 0.0, 0.0);
	Subsystem->EnsureSnapshot(18);
	TestEqual(TEXT("an earlier frame after a restore is built"), Subsystem->GetSnapshot().Frame, 18);
	TestEqual(TEXT("from the state it has"), Subsystem->GetSnapshot().Entries[0].Transform.GetLocation().X, 50.0);

	// A restore with no state (the instance has none yet) invalidates too
	Component->RestoreFrame(nullptr, nullptr);
	const int32 Builds = Subsystem->GetSnapshotBuildCount();
	Subsystem->EnsureSnapshot(18);
	TestEqual(TEXT("a restore without a state still invalidates"), Subsystem->GetSnapshotBuildCount(), Builds + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitSnapshotEndOfPrevFrameTest, "GSTNCombat.NP.Hits.Snapshot.EntriesAreEndOfPrevFrame", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitSnapshotEndOfPrevFrameTest::RunTest(const FString& Parameters)
{
	UGSTN_CombatWorldSubsystem* Subsystem = MakeSubsystem();
	FSnapshotFakeSource First, Second;
	First.Location = FVector(0.0, 0.0, 0.0);
	First.Velocity = FVector(600.0, 0.0, 0.0);
	// A tilted gravity: up is not world Z, and the snapshot carries it as given
	Second.Location = FVector(500.0, 0.0, 0.0);
	Second.Up = FVector(1.0, 0.0, 0.0);
	Subsystem->RegisterEntity(1, First, nullptr, FGenericTeamId(1), nullptr);
	Subsystem->RegisterEntity(2, Second, nullptr, FGenericTeamId(2), nullptr);

	// The first combat tick of frame 30 builds: both entities are at the end of frame 29
	const FGSTN_StepSnapshot& Snapshot = Subsystem->EnsureSnapshot(30);
	TestEqual(TEXT("entity 1 at the end of the previous frame"), Snapshot.Find(1)->Transform.GetLocation().X, 0.0);

	// Entity 1 ticks and writes frame 30: its state moves on. The entity that ticks second still reads the snapshot of the first
	First.Location = FVector(10.0, 0.0, 0.0);
	First.Velocity = FVector(610.0, 0.0, 0.0);
	const FGSTN_StepSnapshot& SeenBySecond = Subsystem->EnsureSnapshot(30);
	TestEqual(TEXT("the second attacker sees entity 1 where it was"), SeenBySecond.Find(1)->Transform.GetLocation().X, 0.0);
	TestEqual(TEXT("and the velocity it had"), SeenBySecond.Find(1)->Velocity.X, 600.0);
	TestEqual(TEXT("one build"), Subsystem->GetSnapshotBuildCount(), 1);

	// Up comes from the entity's gravity frame
	TestTrue(TEXT("up is the one the source reported, not world Z"), SeenBySecond.Find(2)->Up.Equals(FVector(1.0, 0.0, 0.0)));
	TestTrue(TEXT("the other keeps its own"), SeenBySecond.Find(1)->Up.Equals(FVector::UpVector));

	// Frame 31 starts from the end of frame 30
	const FGSTN_StepSnapshot& Next = Subsystem->EnsureSnapshot(31);
	TestEqual(TEXT("the next step reads the new state"), Next.Find(1)->Transform.GetLocation().X, 10.0);

	// A component without a simulation state contributes nothing rather than a made up entry
	AActor* Owner = NewObject<AActor>(GetTransientPackage());
	UGSTN_CombatSimComponent* Component = NewObject<UGSTN_CombatSimComponent>(Owner);
	FGSTN_SnapshotEntry Entry;
	TestFalse(TEXT("no sync state, no entry"), Component->ReadSnapshotEntry(31, Entry));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitSnapshotProviderOrderTest, "GSTNCombat.NP.Hits.Snapshot.ProviderOrderDeterministic", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitSnapshotProviderOrderTest::RunTest(const FString& Parameters)
{
	auto Build = [this](bool bReversed) -> TArray<FVector2D>
	{
		UGSTN_CombatWorldSubsystem* Subsystem = MakeSubsystem();
		FSnapshotFakeSource Player;
		FSnapshotFakeProvider Minions;
		Minions.ProviderId = 20;
		Minions.EntityIds = { 7, 3 };
		FSnapshotFakeProvider Dummies;
		Dummies.ProviderId = 10;
		Dummies.EntityIds = { 5, 3 };
		Subsystem->RegisterEntity(1, Player, nullptr, FGenericTeamId(1), nullptr);
		if (bReversed)
		{
			TestTrue(TEXT("dummies register"), Subsystem->RegisterProvider(&Dummies));
			TestTrue(TEXT("minions register"), Subsystem->RegisterProvider(&Minions));
		}
		else
		{
			TestTrue(TEXT("minions register"), Subsystem->RegisterProvider(&Minions));
			TestTrue(TEXT("dummies register"), Subsystem->RegisterProvider(&Dummies));
		}
		FSnapshotFakeProvider Duplicate;
		Duplicate.ProviderId = 10;
		TestFalse(TEXT("a provider id registers once"), Subsystem->RegisterProvider(&Duplicate));

		TArray<FVector2D> Order;
		for (const FGSTN_SnapshotEntry& Entry : Subsystem->EnsureSnapshot(5).Entries)
		{
			Order.Add(FVector2D(Entry.EntityId, Entry.Velocity.X));
		}
		Subsystem->UnregisterProvider(&Dummies);
		Subsystem->UnregisterProvider(&Minions);
		return Order;
	};

	const TArray<FVector2D> Forward = Build(false);
	const TArray<FVector2D> Reversed = Build(true);
	TestEqual(TEXT("the same number of entries"), Forward.Num(), Reversed.Num());
	TestEqual(TEXT("five entries: the player, two dummies and two minions"), Forward.Num(), 5);
	TestTrue(TEXT("the order does not depend on the registration order"), Forward == Reversed);
	if (Forward.Num() == 5)
	{
		TestEqual(TEXT("1 first"), Forward[0].X, 1.0);
		TestEqual(TEXT("then 3 from the lower provider id"), Forward[1].Y, 10.0);
		TestEqual(TEXT("then 3 from the higher"), Forward[2].Y, 20.0);
		TestEqual(TEXT("then 5"), Forward[3].X, 5.0);
		TestEqual(TEXT("then 7"), Forward[4].X, 7.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_HitSnapshotBroadphaseTest, "GSTNCombat.NP.Hits.Snapshot.BroadphaseCandidates", GSTN_NP_TEST_FLAGS)
bool FGSTN_HitSnapshotBroadphaseTest::RunTest(const FString& Parameters)
{
	UGSTN_CombatWorldSubsystem* Subsystem = MakeSubsystem();
	UGSTN_HurtboxSetDefinition* Set = MakeSphereSet(100.0);
	TestEqual(TEXT("the bound radius of a 100 cm sphere"), UGSTN_CombatWorldSubsystem::ComputeBoundRadius(Set), 100.0);

	FSnapshotFakeSource A, B, C, D, E, F;
	A.Location = FVector(0.0, 0.0, 0.0);
	B.Location = FVector(150.0, 0.0, 0.0);
	C.Location = FVector(1000.0, 0.0, 0.0);
	D.Location = FVector(0.0, 0.0, 5000.0);
	E.Location = FVector(-3000.0, -3000.0, 0.0);
	F.Location = FVector(199.0, 0.0, 0.0);
	Subsystem->RegisterEntity(1, A, nullptr, FGenericTeamId(1), Set);
	Subsystem->RegisterEntity(2, B, nullptr, FGenericTeamId(1), Set);
	Subsystem->RegisterEntity(3, C, nullptr, FGenericTeamId(1), Set);
	Subsystem->RegisterEntity(4, D, nullptr, FGenericTeamId(1), Set);
	Subsystem->RegisterEntity(5, E, nullptr, FGenericTeamId(1), Set);
	Subsystem->RegisterEntity(6, F, nullptr, FGenericTeamId(1), Set);

	const FGSTN_StepSnapshot& Snapshot = Subsystem->EnsureSnapshot(1);
	TestEqual(TEXT("the cell is twice the largest extent"), Snapshot.CellSize, 200.0);

	TArray<int32> Found;
	Snapshot.QueryCandidates(FBox(FVector(-50.0), FVector(50.0)), Found);
	TestTrue(TEXT("a box at the origin touches the entity there and the ones whose sphere reaches it"), IdsOf(Snapshot, Found) == TArray<int32>({ 1, 2 }));

	Snapshot.QueryCandidates(FBox(FVector(180.0, -10.0, -10.0), FVector(190.0, 10.0, 10.0)), Found);
	TestTrue(TEXT("an entity whose sphere spans two cells is found from the first"), IdsOf(Snapshot, Found) == TArray<int32>({ 2, 6 }));

	Snapshot.QueryCandidates(FBox(FVector(210.0, -10.0, -10.0), FVector(220.0, 10.0, 10.0)), Found);
	TestTrue(TEXT("and from the second"), IdsOf(Snapshot, Found) == TArray<int32>({ 2, 6 }));

	Snapshot.QueryCandidates(FBox(FVector(900.0, -50.0, -50.0), FVector(1100.0, 50.0, 50.0)), Found);
	TestTrue(TEXT("a box at the far entity finds only it"), IdsOf(Snapshot, Found) == TArray<int32>({ 3 }));

	Snapshot.QueryCandidates(FBox(FVector(-100000.0), FVector(100000.0)), Found);
	TestTrue(TEXT("a box over everything finds everything, in id order"), IdsOf(Snapshot, Found) == TArray<int32>({ 1, 2, 3, 4, 5, 6 }));

	Snapshot.QueryCandidates(FBox(FVector(40000.0), FVector(40100.0)), Found);
	TestTrue(TEXT("a box over nothing finds nothing"), Found.IsEmpty());

	Snapshot.QueryCandidates(FBox(FVector(-3050.0, -3050.0, -50.0), FVector(-2950.0, -2950.0, 50.0)), Found);
	TestTrue(TEXT("negative coordinates floor to their own cells"), IdsOf(Snapshot, Found) == TArray<int32>({ 5 }));

	// Without any hurtbox the default cell applies
	UGSTN_CombatWorldSubsystem* Bare = MakeSubsystem();
	FSnapshotFakeSource G;
	Bare->RegisterEntity(1, G, nullptr, FGenericTeamId(1), nullptr);
	TestEqual(TEXT("the default cell size is 400 cm"), Bare->EnsureSnapshot(1).CellSize, 400.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
