// Automation tests of the minion presentation (M8 G7) in a transient game world with no net driver: the listen-host path (puppets read
// the authority store, no registry), the puppet pool, the MaxVisible cut and the flash lifecycle on a puppet. The client path's pieces
// (interpolation, blends, the HitCounter confirm) are pure and tested in GSTNMinion.Core.Present.*; the harness covers replication.

#include "Data/GSTN_MinionDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Net/GSTN_MinionClientRegistry.h"
#include "Present/GSTN_MinionPresentationSubsystem.h"
#include "Present/GSTN_MinionPuppet.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_MINION_PRESENT_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** A transient game world with an active minion batch and an active presentation (a test world has no begin play) */
	struct FPresentRig
	{
		FPresentRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNMinionPresentTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Minions = World->GetSubsystem<UGSTN_MinionCombatSubsystem>();
			if (Minions)
			{
				Minions->ActivateForTests();
			}
			Present = World->GetSubsystem<UGSTN_MinionPresentationSubsystem>();
			if (Present)
			{
				Present->ActivateForTests(false);
				Present->ViewerOverrideForTests = FVector::ZeroVector;
			}
			Definition.Reset(NewObject<UGSTN_MinionDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
			Definition->Stats.MaxHealth = 40.f;
		}

		~FPresentRig()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		bool IsValid() const { return Minions && Present; }

		FGSTN_MinionHandle Spawn(const FVector& Location)
		{
			FGSTN_MinionSpawnParams Params;
			Params.bPlaySpawnTimeline = false;
			return Minions->EnqueueSpawn(Definition.Get(), FTransform(Location), Params);
		}

		UWorld* World = nullptr;
		UGSTN_MinionCombatSubsystem* Minions = nullptr;
		UGSTN_MinionPresentationSubsystem* Present = nullptr;
		TStrongObjectPtr<UGSTN_MinionDefinition> Definition;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionPresentListenHostTest, "GSTNMinion.Runtime.Present.ListenHostReadsStore", GSTN_MINION_PRESENT_TEST_FLAGS)
bool FGSTN_MinionPresentListenHostTest::RunTest(const FString& Parameters)
{
	FPresentRig Rig;
	if (!TestTrue(TEXT("minion batch and presentation exist in a game world"), Rig.IsValid()))
	{
		return false;
	}
	const FGSTN_MinionHandle Handles[] = { Rig.Spawn(FVector(100.0, 0.0, 0.0)), Rig.Spawn(FVector(0.0, 300.0, 0.0)), Rig.Spawn(FVector(-200.0, -50.0, 0.0)) };
	Rig.Minions->TickBatch(1);
	Rig.Present->TickPresentation(0.0);

	const UGSTN_MinionClientRegistry* Registry = Rig.World->GetSubsystem<UGSTN_MinionClientRegistry>();
	TestTrue(TEXT("no network copy on the host: the client registry is empty"), !Registry || Registry->GetItems().Num() == 0);
	TestTrue(TEXT("source is the store"), Rig.Present->GetStats().Source == EGSTN_MinionPresentSource::Store);
	TestEqual(TEXT("one presented minion per live slot"), Rig.Present->GetRecords().Num(), 3);

	const FGSTN_MinionStore& Store = Rig.Minions->GetStore();
	for (const FGSTN_MinionHandle& Handle : Handles)
	{
		const FGSTN_MinionPresentRecord* Record = Rig.Present->FindRecord(Handle.Index);
		if (!TestNotNull(TEXT("a record keyed by the slot"), Record))
		{
			continue;
		}
		TestTrue(TEXT("written straight from the store"), Record->Pose.Mode == EGSTN_MinionDisplayMode::Direct);
		TestTrue(TEXT("store position, no render delay"), Record->Pose.Position.Equals(Store.Position[Handle.Index], 0.001f));
		TestEqual(TEXT("displayed frame is the last batch"), Record->Pose.Frame, static_cast<double>(Rig.Minions->GetLastBatchFrame()));
		const AGSTN_MinionPuppet* Puppet = Record->Puppet.Get();
		if (TestNotNull(TEXT("a puppet (null mesh is fine headless)"), Puppet))
		{
			TestTrue(TEXT("puppet at the store position"), Puppet->GetActorLocation().Equals(FVector(Store.Position[Handle.Index]), 0.01));
			TestFalse(TEXT("puppet not replicated"), Puppet->GetIsReplicated());
		}
	}

	// The store moves (a despawn): the host's puppet follows in the next presentation frame, no network in between
	TestTrue(TEXT("despawn"), Rig.Minions->Despawn(Handles[1]));
	Rig.Present->TickPresentation(0.016);
	TestEqual(TEXT("its record is gone"), Rig.Present->GetRecords().Num(), 2);
	TestEqual(TEXT("its puppet went back to the pool"), Rig.Present->GetNumPooledPuppets(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionPresentPoolTest, "GSTNMinion.Runtime.Present.PoolReusesPuppets", GSTN_MINION_PRESENT_TEST_FLAGS)
bool FGSTN_MinionPresentPoolTest::RunTest(const FString& Parameters)
{
	FPresentRig Rig;
	if (!TestTrue(TEXT("rig"), Rig.IsValid()))
	{
		return false;
	}
	// Direct: acquire, release, acquire gives the same actor back
	AGSTN_MinionPuppet* First = Rig.Present->AcquirePuppet(Rig.Definition.Get(), 1000, FTransform::Identity);
	if (!TestNotNull(TEXT("acquired"), First))
	{
		return false;
	}
	const int32 SpawnedAfterFirst = Rig.Present->GetStats().PuppetsSpawned;
	Rig.Present->ReleasePuppet(First);
	TestFalse(TEXT("released puppet is hidden"), First->IsInUse() || !First->IsHidden());
	AGSTN_MinionPuppet* Second = Rig.Present->AcquirePuppet(Rig.Definition.Get(), 1001, FTransform(FVector(50.0, 0.0, 0.0)));
	TestTrue(TEXT("the same actor comes back"), Second == First);
	TestEqual(TEXT("no new actor after a release"), Rig.Present->GetStats().PuppetsSpawned, SpawnedAfterFirst);
	TestTrue(TEXT("reused puppet is shown at its new place"), Second && !Second->IsHidden() && Second->GetActorLocation().Equals(FVector(50.0, 0.0, 0.0), 0.01));
	Rig.Present->ReleasePuppet(Second);

	// Through minions: two come, one dies away, another comes: still the actors spawned for the first two
	const FGSTN_MinionHandle A = Rig.Spawn(FVector(100.0, 0.0, 0.0));
	Rig.Spawn(FVector(200.0, 0.0, 0.0));
	Rig.Minions->TickBatch(1);
	Rig.Present->TickPresentation(0.0);
	const int32 Spawned = Rig.Present->GetStats().PuppetsSpawned;
	TestEqual(TEXT("two puppets in use"), Rig.Present->GetStats().Visible, 2);
	Rig.Minions->Despawn(A);
	Rig.Present->TickPresentation(0.016);
	Rig.Spawn(FVector(300.0, 0.0, 0.0));
	Rig.Minions->TickBatch(2);
	Rig.Present->TickPresentation(0.032);
	TestEqual(TEXT("two minions shown"), Rig.Present->GetStats().Visible, 2);
	TestEqual(TEXT("no new actor for the new minion"), Rig.Present->GetStats().PuppetsSpawned, Spawned);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionPresentMaxVisibleTest, "GSTNMinion.Runtime.Present.MaxVisibleNearestFirst", GSTN_MINION_PRESENT_TEST_FLAGS)
bool FGSTN_MinionPresentMaxVisibleTest::RunTest(const FString& Parameters)
{
	FPresentRig Rig;
	if (!TestTrue(TEXT("rig"), Rig.IsValid()))
	{
		return false;
	}
	TArray<FGSTN_MinionHandle> Handles;
	for (int32 Index = 1; Index <= 5; ++Index)
	{
		Handles.Add(Rig.Spawn(FVector(Index * 1000.0, 0.0, 0.0)));
	}
	Rig.Minions->TickBatch(1);
	Rig.Present->MaxVisibleOverrideForTests = 2;
	Rig.Present->ViewerOverrideForTests = FVector::ZeroVector;
	Rig.Present->TickPresentation(0.0);

	TestEqual(TEXT("two shown"), Rig.Present->GetStats().Visible, 2);
	TestEqual(TEXT("three culled"), Rig.Present->GetStats().Culled, 3);
	TestEqual(TEXT("still five presented (culled ones stay hittable)"), Rig.Present->GetRecords().Num(), 5);
	auto IsShown = [&Rig](const FGSTN_MinionHandle& Handle)
	{
		const FGSTN_MinionPresentRecord* Record = Rig.Present->FindRecord(Handle.Index);
		return Record && Record->bVisible && Record->Puppet.IsValid();
	};
	TestTrue(TEXT("the two nearest are shown"), IsShown(Handles[0]) && IsShown(Handles[1]));
	TestFalse(TEXT("the farther ones are not"), IsShown(Handles[2]) || IsShown(Handles[3]) || IsShown(Handles[4]));

	// The viewer walks to the far end: the cut follows, puppets move between minions through the pool
	const int32 Spawned = Rig.Present->GetStats().PuppetsSpawned;
	Rig.Present->ViewerOverrideForTests = FVector(6000.0, 0.0, 0.0);
	Rig.Present->TickPresentation(0.016);
	TestTrue(TEXT("now the two nearest the new view"), IsShown(Handles[4]) && IsShown(Handles[3]));
	TestFalse(TEXT("the old ones lost their puppets"), IsShown(Handles[0]) || IsShown(Handles[1]));
	TestEqual(TEXT("the cut reuses the pool"), Rig.Present->GetStats().PuppetsSpawned, Spawned);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionPresentFlashPuppetTest, "GSTNMinion.Runtime.Present.PredictedFlashOnPuppet", GSTN_MINION_PRESENT_TEST_FLAGS)
bool FGSTN_MinionPresentFlashPuppetTest::RunTest(const FString& Parameters)
{
	FPresentRig Rig;
	if (!TestTrue(TEXT("rig"), Rig.IsValid()))
	{
		return false;
	}
	const FGSTN_MinionHandle Handle = Rig.Spawn(FVector(100.0, 0.0, 0.0));
	Rig.Minions->TickBatch(1);
	Rig.Present->TickPresentation(0.0);
	const int32 EntityId = GSTNEntityKinds::MakeMinionEntityId(Handle.Index);

	// A predicted hit on the minion's pipeline id: flash and pause at once on its puppet (custom primitive data, anim rate 0)
	Rig.Present->NotePredictedHitForTests(EntityId, 1.0);
	Rig.Present->TickPresentation(1.0);
	const FGSTN_MinionPresentRecord* Record = Rig.Present->FindRecord(Handle.Index);
	const AGSTN_MinionPuppet* Puppet = Record ? Record->Puppet.Get() : nullptr;
	if (!TestNotNull(TEXT("puppet"), Puppet))
	{
		return false;
	}
	TestEqual(TEXT("flash on"), Puppet->GetFlash(), 1.f);
	TestTrue(TEXT("mesh paused"), Puppet->IsPaused());
	TestTrue(TEXT("predicted"), Record->Flash.GetState() == EGSTN_MinionFlashState::Predicted);

	// No HitCounter change for 500 ms: the effect ends
	Rig.Present->TickPresentation(1.6);
	TestTrue(TEXT("expired without a confirm"), Record->Flash.GetState() == EGSTN_MinionFlashState::Expired);
	TestEqual(TEXT("flash off"), Puppet->GetFlash(), 0.f);
	TestFalse(TEXT("mesh resumed"), Puppet->IsPaused());

	// Predicted then cancelled (the attacker's entry vanished)
	Rig.Present->NotePredictedHitForTests(EntityId, 2.0);
	Rig.Present->NotePredictionCancelledForTests(EntityId, 2.02);
	Rig.Present->TickPresentation(2.02);
	TestTrue(TEXT("cancelled"), Record->Flash.GetState() == EGSTN_MinionFlashState::Cancelled);
	TestEqual(TEXT("flash off after a cancel"), Puppet->GetFlash(), 0.f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
