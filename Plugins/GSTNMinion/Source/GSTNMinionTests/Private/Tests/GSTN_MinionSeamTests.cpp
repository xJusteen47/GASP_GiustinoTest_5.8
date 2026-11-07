// Automation tests of the bridge seams (M8 G10): the platform flag and the publish observer, the per-slot LOD ceiling, the pose history
// samples and the ExtByte of the net item. Headless, in a transient game world; they drive TickBatch directly. A spawn activates at most
// gstn.Minion.SpawnsPerFrame (4) per batch frame, so every test ticks a few frames before it acts on a spawned minion.

#include "Data/GSTN_MinionDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Net/GSTN_MinionPack.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Runtime/GSTN_MinionPublishObserver.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_MINION_SEAM_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** A transient game world with an activated minion subsystem (a test world has no begin play) */
	struct FSeamRig
	{
		FSeamRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNMinionSeamTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Minions = World->GetSubsystem<UGSTN_MinionCombatSubsystem>();
			if (Minions)
			{
				Minions->ActivateForTests();
			}
		}

		~FSeamRig()
		{
			if (Minions)
			{
				Minions->SetForcedLODTier(INDEX_NONE);
			}
			// The ext-type table is process-wide: leave it as a world without bridge types has it
			AGSTN_MinionPack::SetNetExtTypes(TBitArray<>());
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		FGSTN_MinionHandle Spawn(UGSTN_MinionDefinition* Definition, int32 SpawnGroup = 0, const FVector& Location = FVector::ZeroVector)
		{
			FGSTN_MinionSpawnParams Params;
			Params.SpawnGroup = SpawnGroup;
			Params.bPlaySpawnTimeline = false;
			return Minions->EnqueueSpawn(Definition, FTransform(Location), Params);
		}

		void Tick(int32 Frames)
		{
			for (int32 Index = 0; Index < Frames; ++Index)
			{
				Minions->TickBatch(++Frame);
			}
		}

		UWorld* World = nullptr;
		UGSTN_MinionCombatSubsystem* Minions = nullptr;
		int32 Frame = 0;
	};

	TStrongObjectPtr<UGSTN_MinionDefinition> MakeSeamDefinition(bool bHasNetExt = false)
	{
		TStrongObjectPtr<UGSTN_MinionDefinition> Definition(NewObject<UGSTN_MinionDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
		Definition->Stats.MaxHealth = 40.f;
		Definition->bHasNetExt = bHasNetExt;
		return Definition;
	}

	/** Records every call of the platform pose point */
	struct FRecordingObserver : public IGSTN_MinionPublishObserver
	{
		virtual void OnMinionPosesPublished(int32 Frame, TConstArrayView<FGSTN_MinionPublishedPose> InPoses) override
		{
			Frames.Add(Frame);
			Poses.Reset();
			Poses.Append(InPoses.GetData(), InPoses.Num());
		}

		TArray<int32> Frames;
		/** The poses of the latest call */
		TArray<FGSTN_MinionPublishedPose> Poses;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionSeamsPlatformFlagTest, "GSTNMinion.Runtime.Seams.PlatformFlagSuppressesPackItem", GSTN_MINION_SEAM_TEST_FLAGS)
bool FGSTN_MinionSeamsPlatformFlagTest::RunTest(const FString& Parameters)
{
	FSeamRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeSeamDefinition();

	const FGSTN_MinionHandle A = Rig.Spawn(Definition.Get(), 1, FVector(0.0, 0.0, 0.0));
	const FGSTN_MinionHandle B = Rig.Spawn(Definition.Get(), 1, FVector(100.0, 0.0, 0.0));
	const FGSTN_MinionHandle C = Rig.Spawn(Definition.Get(), 1, FVector(200.0, 0.0, 0.0));
	// Flagged before its first publish: it never gets an item
	const FGSTN_MinionHandle D = Rig.Spawn(Definition.Get(), 1, FVector(300.0, 0.0, 0.0));
	Rig.Minions->SetPlatformActor(D, true);
	Rig.Tick(4);

	const AGSTN_MinionPack* Pack = Rig.Minions->GetPackOfSlot(A.Index);
	if (!TestNotNull(TEXT("the unflagged minions are packed"), Pack))
	{
		return false;
	}
	TestEqual(TEXT("three items: the flagged spawn has none"), Pack->GetNetArray().Items.Num(), 3);
	TestNull(TEXT("the flagged-at-spawn minion has no pack"), Rig.Minions->GetPackOfSlot(D.Index));
	TestTrue(TEXT("but it is alive and simulated"), Rig.Minions->GetStore().IsValid(D));
	TestEqual(TEXT("one platform counted"), Rig.Minions->GetNumPlatformActors(), 1);

	// Flag a minion that already has an item: the item leaves the pack the way a despawn's does
	const int32 RemovesBefore = static_cast<int32>(Rig.Minions->GetNetStats().ItemRemoves);
	Rig.Minions->SetPlatformActor(B, true);
	TestTrue(TEXT("IsPlatformActor"), Rig.Minions->IsPlatformActor(B));
	Rig.Tick(2);
	TestNull(TEXT("the flagged minion left its pack"), Rig.Minions->GetPackOfSlot(B.Index));
	TestEqual(TEXT("the pack is down to two items"), Pack->GetNetArray().Items.Num(), 2);
	TestEqual(TEXT("the removal is counted like a despawn's"), static_cast<int32>(Rig.Minions->GetNetStats().ItemRemoves), RemovesBefore + 1);
	TestTrue(TEXT("the store keeps the minion"), Rig.Minions->GetStore().IsValid(B));
	TestNotNull(TEXT("the others keep their items"), Rig.Minions->GetPackOfSlot(C.Index));

	// Clearing the flag adds the item back
	Rig.Minions->SetPlatformActor(B, false);
	Rig.Minions->SetPlatformActor(D, false);
	Rig.Tick(2);
	TestNotNull(TEXT("B has a pack again"), Rig.Minions->GetPackOfSlot(B.Index));
	TestNotNull(TEXT("D has a pack at last"), Rig.Minions->GetPackOfSlot(D.Index));
	TestEqual(TEXT("four items"), Rig.Minions->GetPackOfSlot(A.Index)->GetNetArray().Items.Num(), 4);
	TestEqual(TEXT("no platform left"), Rig.Minions->GetNumPlatformActors(), 0);

	// A freed flagged slot stops counting, and a stale handle is ignored
	Rig.Minions->SetPlatformActor(C, true);
	TestEqual(TEXT("counted"), Rig.Minions->GetNumPlatformActors(), 1);
	TestTrue(TEXT("despawn"), Rig.Minions->Despawn(C));
	TestEqual(TEXT("freed with the slot"), Rig.Minions->GetNumPlatformActors(), 0);
	Rig.Minions->SetPlatformActor(C, true);
	TestEqual(TEXT("a stale handle is ignored"), Rig.Minions->GetNumPlatformActors(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionSeamsObserverTest, "GSTNMinion.Runtime.Seams.PublishObserverOncePerStep", GSTN_MINION_SEAM_TEST_FLAGS)
bool FGSTN_MinionSeamsObserverTest::RunTest(const FString& Parameters)
{
	FSeamRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeSeamDefinition();
	const FGSTN_MinionHandle A = Rig.Spawn(Definition.Get(), 0, FVector(0.0, 0.0, 0.0));
	const FGSTN_MinionHandle B = Rig.Spawn(Definition.Get(), 0, FVector(100.0, 0.0, 0.0));
	const FGSTN_MinionHandle C = Rig.Spawn(Definition.Get(), 0, FVector(200.0, 0.0, 0.0));
	Rig.Tick(2);

	FRecordingObserver Observer;
	Rig.Minions->AddPublishObserver(&Observer);
	Rig.Minions->AddPublishObserver(&Observer);
	Rig.Tick(3);
	TestEqual(TEXT("no flagged slot: never called"), Observer.Frames.Num(), 0);

	// Flag C then A: the poses come in slot order, not in flag order
	Rig.Minions->SetPlatformActor(C, true);
	Rig.Minions->SetPlatformActor(A, true);
	for (int32 Step = 0; Step < 5; ++Step)
	{
		Rig.Tick(1);
		TestEqual(TEXT("one call per step"), Observer.Frames.Num(), Step + 1);
		TestEqual(TEXT("with the frame of the step"), Observer.Frames.Last(), Rig.Frame);
		if (!TestEqual(TEXT("only the flagged slots"), Observer.Poses.Num(), 2))
		{
			return false;
		}
		TestTrue(TEXT("slot order"), Observer.Poses[0].Handle == A && Observer.Poses[1].Handle == C);
		// The end-of-step pose: the store as the step left it
		for (const FGSTN_MinionPublishedPose& Pose : Observer.Poses)
		{
			const int32 Slot = Pose.Handle.Index;
			TestTrue(TEXT("position is the store's feet"), Pose.Position.Equals(Rig.Minions->GetStore().Position[Slot], UE_KINDA_SMALL_NUMBER));
			TestEqual(TEXT("yaw is the store's"), Pose.YawDegrees, Rig.Minions->GetStore().YawDegrees[Slot]);
			TestTrue(TEXT("velocity is the movement's"), Pose.Velocity.Equals(Rig.Minions->GetMovementColumns().Velocity[Slot], UE_KINDA_SMALL_NUMBER));
		}
	}

	// Cleared flags: silent again
	Rig.Minions->SetPlatformActor(A, false);
	Rig.Minions->SetPlatformActor(C, false);
	const int32 Calls = Observer.Frames.Num();
	Rig.Tick(3);
	TestEqual(TEXT("no flagged slot: not called"), Observer.Frames.Num(), Calls);

	// Unregistered: not called either
	Rig.Minions->SetPlatformActor(B, true);
	Rig.Tick(1);
	TestEqual(TEXT("flagged again: called"), Observer.Frames.Num(), Calls + 1);
	TestEqual(TEXT("with one pose"), Observer.Poses.Num(), 1);
	Rig.Minions->RemovePublishObserver(&Observer);
	Rig.Tick(2);
	TestEqual(TEXT("removed observer is not called"), Observer.Frames.Num(), Calls + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionSeamsLODCeilingTest, "GSTNMinion.Runtime.Seams.LODCeilingHoldsAndReleases", GSTN_MINION_SEAM_TEST_FLAGS)
bool FGSTN_MinionSeamsLODCeilingTest::RunTest(const FString& Parameters)
{
	FSeamRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeSeamDefinition();
	const FGSTN_MinionHandle Free = Rig.Spawn(Definition.Get(), 0, FVector(0.0, 0.0, 0.0));
	const FGSTN_MinionHandle Held = Rig.Spawn(Definition.Get(), 0, FVector(100.0, 0.0, 0.0));
	Rig.Tick(2);
	Rig.Minions->SetMaxLODTier(Held, EGSTN_MinionLODTier::Near);
	TestEqual(TEXT("the ceiling reads back"), static_cast<int32>(Rig.Minions->GetMaxLODTier(Held)), static_cast<int32>(EGSTN_MinionLODTier::Near));

	// No player pawn at all: the distance is infinite, so a free minion goes Dormant after the dwell (30 frames) and its wake interval
	Rig.Tick(220);
	const FGSTN_MinionStore& Store = Rig.Minions->GetStore();
	TestEqual(TEXT("the unconstrained minion went Dormant"), static_cast<int32>(Store.LODTier[Free.Index]), static_cast<int32>(EGSTN_MinionLODTier::Dormant));
	TestEqual(TEXT("the constrained one is held at its ceiling"), static_cast<int32>(Store.LODTier[Held.Index]), static_cast<int32>(EGSTN_MinionLODTier::Near));

	// Releasing restores the normal stepping
	Rig.Minions->SetMaxLODTier(Held, EGSTN_MinionLODTier::Dormant);
	Rig.Tick(220);
	TestEqual(TEXT("released: it steps to Dormant"), static_cast<int32>(Store.LODTier[Held.Index]), static_cast<int32>(EGSTN_MinionLODTier::Dormant));

	// A ceiling below the current tier pulls the minion back at once, without waiting for its next scheduled step
	Rig.Minions->SetMaxLODTier(Free, EGSTN_MinionLODTier::Combat);
	TestEqual(TEXT("pulled back at once"), static_cast<int32>(Store.LODTier[Free.Index]), static_cast<int32>(EGSTN_MinionLODTier::Combat));
	Rig.Tick(220);
	TestEqual(TEXT("and held there"), static_cast<int32>(Store.LODTier[Free.Index]), static_cast<int32>(EGSTN_MinionLODTier::Combat));

	// The test override of the tier respects the ceiling too
	Rig.Minions->SetForcedLODTier(static_cast<int32>(EGSTN_MinionLODTier::Far));
	// A Dormant minion is scheduled every 16th frame
	Rig.Tick(20);
	TestEqual(TEXT("a forced tier is capped by the ceiling"), static_cast<int32>(Store.LODTier[Free.Index]), static_cast<int32>(EGSTN_MinionLODTier::Combat));
	TestEqual(TEXT("and applies to a free minion"), static_cast<int32>(Store.LODTier[Held.Index]), static_cast<int32>(EGSTN_MinionLODTier::Far));

	// A stale handle changes nothing and reads as no ceiling
	const FGSTN_MinionHandle Gone = Held;
	TestTrue(TEXT("despawn"), Rig.Minions->Despawn(Held));
	Rig.Minions->SetMaxLODTier(Gone, EGSTN_MinionLODTier::Combat);
	TestEqual(TEXT("stale handle reads Dormant"), static_cast<int32>(Rig.Minions->GetMaxLODTier(Gone)), static_cast<int32>(EGSTN_MinionLODTier::Dormant));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionSeamsSamplePoseTest, "GSTNMinion.Runtime.Seams.SamplePoseFromHistory", GSTN_MINION_SEAM_TEST_FLAGS)
bool FGSTN_MinionSeamsSamplePoseTest::RunTest(const FString& Parameters)
{
	FSeamRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	// Combat every frame: the history gets a record each step
	Rig.Minions->SetForcedLODTier(static_cast<int32>(EGSTN_MinionLODTier::Combat));
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeSeamDefinition();
	const FGSTN_MinionHandle Handle = Rig.Spawn(Definition.Get(), 0, FVector(500.0, 250.0, 0.0));
	Rig.Tick(2);
	// Gives the pose a history that is not constant (a launch moves it); the assertions below compare with the history itself, so they
	// hold whether or not the movement backend moves it
	Rig.Minions->LaunchMinion(Handle, FVector(600.0, 0.0, 300.0), false);
	Rig.Tick(60);

	const FGSTN_MinionHistory& History = Rig.Minions->GetHistory();
	const int32 Depth = History.GetDepth();
	const int32 Last = Rig.Frame;

	FVector3f Position = FVector3f::ZeroVector;
	float Yaw = 0.f;
	bool bClamped = true;
	const int32 Exact = Last - 5;
	if (!TestTrue(TEXT("a frame inside the ring samples"), Rig.Minions->SamplePose(Handle, Exact, Position, Yaw, bClamped)))
	{
		return false;
	}
	const FGSTN_MinionHistoryRecord* Record = History.Find(Handle.Index, Exact);
	if (!TestNotNull(TEXT("the history has that frame"), Record))
	{
		return false;
	}
	TestEqual(TEXT("it is that frame's record"), Record->Frame, Exact);
	TestTrue(TEXT("the pose is the written one"), Position.Equals(Record->Position, UE_KINDA_SMALL_NUMBER));
	TestEqual(TEXT("the yaw is the written one"), Yaw, Record->YawDegrees);
	TestFalse(TEXT("an exact frame is not clamped"), bClamped);

	// The latest frame is exact, a future one clamps to it
	const FGSTN_MinionHistoryRecord* Latest = History.Find(Handle.Index, Last);
	if (!TestNotNull(TEXT("the latest record exists"), Latest))
	{
		return false;
	}
	TestTrue(TEXT("the latest frame samples"), Rig.Minions->SamplePose(Handle, Latest->Frame, Position, Yaw, bClamped));
	TestFalse(TEXT("exact"), bClamped);
	TestTrue(TEXT("a future frame samples"), Rig.Minions->SamplePose(Handle, Latest->Frame + 1000, Position, Yaw, bClamped));
	TestTrue(TEXT("and clamps"), bClamped);
	TestTrue(TEXT("to the latest pose"), Position.Equals(Latest->Position, UE_KINDA_SMALL_NUMBER));

	// A frame older than the ring clamps to the oldest record it still holds
	const FGSTN_MinionHistoryRecord* Oldest = History.FindOldest(Handle.Index);
	if (!TestNotNull(TEXT("the ring has an oldest record"), Oldest))
	{
		return false;
	}
	TestTrue(TEXT("the ring has wrapped, so an old frame is gone"), Last > Depth + 2 && Oldest->Frame > 1);
	TestTrue(TEXT("a frame before the ring samples"), Rig.Minions->SamplePose(Handle, 1, Position, Yaw, bClamped));
	TestTrue(TEXT("and clamps"), bClamped);
	TestTrue(TEXT("to the oldest pose"), Position.Equals(Oldest->Position, UE_KINDA_SMALL_NUMBER));

	// Generation check
	const FGSTN_MinionHandle Stale = Handle;
	TestTrue(TEXT("despawn"), Rig.Minions->Despawn(Handle));
	TestFalse(TEXT("a stale handle is refused"), Rig.Minions->SamplePose(Stale, Last, Position, Yaw, bClamped));
	TestFalse(TEXT("the null handle is refused"), Rig.Minions->SamplePose(FGSTN_MinionHandle(), Last, Position, Yaw, bClamped));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionSeamsExtByteWireTest, "GSTNMinion.Runtime.Seams.ExtByteInPackItem", GSTN_MINION_SEAM_TEST_FLAGS)
bool FGSTN_MinionSeamsExtByteWireTest::RunTest(const FString& Parameters)
{
	FSeamRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	// Combat tier items are sent at their own rate; a few frames give them a send
	Rig.Minions->SetForcedLODTier(static_cast<int32>(EGSTN_MinionLODTier::Combat));
	TStrongObjectPtr<UGSTN_MinionDefinition> Plain = MakeSeamDefinition(false);
	TStrongObjectPtr<UGSTN_MinionDefinition> WithExt = MakeSeamDefinition(true);
	const FGSTN_MinionHandle OfPlain = Rig.Spawn(Plain.Get(), 2, FVector(0.0, 0.0, 0.0));
	const FGSTN_MinionHandle OfExt = Rig.Spawn(WithExt.Get(), 2, FVector(100.0, 0.0, 0.0));
	Rig.Tick(3);

	Rig.Minions->SetExtByte(OfPlain, 0x5A);
	Rig.Minions->SetExtByte(OfExt, 0xC6);
	TestEqual(TEXT("the store holds the byte"), static_cast<int32>(Rig.Minions->GetExtByte(OfExt)), 0xC6);
	Rig.Tick(16);

	const AGSTN_MinionPack* Pack = Rig.Minions->GetPackOfSlot(OfExt.Index);
	if (!TestNotNull(TEXT("both are in a pack"), Pack) || !TestTrue(TEXT("the same pack"), Pack == Rig.Minions->GetPackOfSlot(OfPlain.Index)))
	{
		return false;
	}
	const int32 ExtIndex = Pack->FindItemIndex(static_cast<uint8>(Rig.Minions->GetItemIdOfSlot(OfExt.Index)));
	const int32 PlainIndex = Pack->FindItemIndex(static_cast<uint8>(Rig.Minions->GetItemIdOfSlot(OfPlain.Index)));
	if (!TestTrue(TEXT("both have an item"), ExtIndex != INDEX_NONE && PlainIndex != INDEX_NONE))
	{
		return false;
	}
	TestEqual(TEXT("the type with bHasNetExt sends the byte"), static_cast<int32>(Pack->GetNetArray().Items[ExtIndex].Wire.ExtByte), 0xC6);
	TestEqual(TEXT("the type without it sends 0"), static_cast<int32>(Pack->GetNetArray().Items[PlainIndex].Wire.ExtByte), 0);

	// A change of the byte alone makes the item dirty and goes out
	const int32 UpdatesBefore = static_cast<int32>(Rig.Minions->GetNetStats().ItemUpdates);
	Rig.Minions->SetExtByte(OfExt, 0x11);
	Rig.Tick(16);
	TestEqual(TEXT("the new byte is published"), static_cast<int32>(Pack->GetNetArray().Items[ExtIndex].Wire.ExtByte), 0x11);
	TestTrue(TEXT("by at least one item update"), static_cast<int32>(Rig.Minions->GetNetStats().ItemUpdates) > UpdatesBefore);

	// A stale handle is ignored, and the byte resets with the slot
	const FGSTN_MinionHandle Gone = OfExt;
	TestTrue(TEXT("despawn"), Rig.Minions->Despawn(OfExt));
	Rig.Minions->SetExtByte(Gone, 0x77);
	TestEqual(TEXT("a stale handle reads 0"), static_cast<int32>(Rig.Minions->GetExtByte(Gone)), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
