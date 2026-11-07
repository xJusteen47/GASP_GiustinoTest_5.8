// Automation tests of the authority batch (M8 G3): stage order by slot, timeline frame skipping, the spawn queue budget and the
// actor pool. They drive TickBatch directly in a transient game world; definitions and timelines are built in code (no content).

#include "Data/GSTN_MinionDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GSTN_MinionTags.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Runtime/GSTN_MinionActor.h"
#include "Runtime/GSTN_MinionCombatComponent.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Spawn/GSTN_MinionSpawner.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_MINION_RUNTIME_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** A transient game world with an activated minion subsystem (a test world has no begin play) */
	struct FMinionRig
	{
		FMinionRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNMinionRuntimeTestWorld"));
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

		~FMinionRig()
		{
			if (Minions)
			{
				Minions->StageTraceForTests = nullptr;
				Minions->SetForcedLODTier(INDEX_NONE);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		FGSTN_MinionHandle Spawn(UGSTN_MinionDefinition* Definition, const FVector& Location = FVector::ZeroVector)
		{
			FGSTN_MinionSpawnParams Params;
			Params.bPlaySpawnTimeline = false;
			return Minions->EnqueueSpawn(Definition, FTransform(Location), Params);
		}

		UWorld* World = nullptr;
		UGSTN_MinionCombatSubsystem* Minions = nullptr;
	};

	/** A definition in numbers only (no hurtboxes, no moves) */
	TStrongObjectPtr<UGSTN_MinionDefinition> MakeDefinition()
	{
		TStrongObjectPtr<UGSTN_MinionDefinition> Definition(NewObject<UGSTN_MinionDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
		Definition->Stats.MaxHealth = 40.f;
		return Definition;
	}

	/** The command of the test move: the ini leaf when the plugin's tag ini is loaded, else a native tag (the batch only compares tags) */
	FGameplayTag TestCommand()
	{
		const FGameplayTag Swing = FGameplayTag::RequestGameplayTag(TEXT("Command.Minion.Attack.Swing"), false);
		return Swing.IsValid() ? Swing : FGameplayTag(GSTNMinionTags::Type);
	}

	/** Restores a console variable when the test ends */
	struct FScopedCVarInt
	{
		FScopedCVarInt(const TCHAR* Name, int32 Value)
		{
			Variable = IConsoleManager::Get().FindConsoleVariable(Name);
			if (Variable)
			{
				Previous = Variable->GetInt();
				Variable->Set(Value, ECVF_SetByCode);
			}
		}

		~FScopedCVarInt()
		{
			if (Variable)
			{
				Variable->Set(Previous, ECVF_SetByCode);
			}
		}

		IConsoleVariable* Variable = nullptr;
		int32 Previous = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionRuntimeBatchOrderTest, "GSTNMinion.Runtime.BatchOrderBySlot", GSTN_MINION_RUNTIME_TEST_FLAGS)
bool FGSTN_MinionRuntimeBatchOrderTest::RunTest(const FString& Parameters)
{
	FMinionRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	FScopedCVarInt Budget(TEXT("gstn.Minion.SpawnsPerFrame"), 4);
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeDefinition();
	Rig.Minions->SetForcedLODTier(static_cast<int32>(EGSTN_MinionLODTier::Combat));

	TArray<FGSTN_MinionHandle> Handles;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Handles.Add(Rig.Spawn(Definition.Get(), FVector(100.0 * Index, 0.0, 0.0)));
	}
	Rig.Minions->TickBatch(1);
	Rig.Minions->TickBatch(2);
	TestEqual(TEXT("six minions are active after two batches"), Rig.Minions->GetLastBatchStats().Alive, 6);

	// Hits posted out of slot order and out of attacker order: the drain must sort them by (victim slot, attacker)
	for (const int32 Victim : { 4, 1, 4 })
	{
		FGSTN_InboxEntry Entry;
		Entry.Kind = GSTNInboxKinds::Hit;
		Entry.Frame = 2;
		Entry.SenderId = Victim == 4 ? 7 : 3;
		Rig.Minions->PostToEntityInbox(GSTNEntityKinds::MakeMinionEntityId(static_cast<uint16>(Victim)), Handles[Victim].Generation, Entry);
	}

	TArray<FIntPoint> Trace;
	Rig.Minions->StageTraceForTests = &Trace;
	Rig.Minions->TickBatch(3);
	Rig.Minions->StageTraceForTests = nullptr;

	TestTrue(TEXT("the batch recorded stage writes"), Trace.Num() > 0);
	int32 PreviousStage = -1;
	int32 PreviousSlot = -1;
	for (const FIntPoint& Write : Trace)
	{
		TestTrue(TEXT("stages run in order"), Write.X >= PreviousStage);
		if (Write.X != PreviousStage)
		{
			PreviousStage = Write.X;
			PreviousSlot = -1;
		}
		// The drain visits a victim once per entry (equal slots allowed); every other stage visits a slot once
		const bool bAscending = Write.X == static_cast<int32>(UGSTN_MinionCombatSubsystem::EStage::InboxDrain) ? Write.Y >= PreviousSlot : Write.Y > PreviousSlot;
		TestTrue(*FString::Printf(TEXT("stage %d visits slot %d in ascending slot order"), Write.X, Write.Y), bAscending);
		PreviousSlot = Write.Y;
	}
	const int32 LODWrites = Trace.FilterByPredicate([](const FIntPoint& Write) { return Write.X == static_cast<int32>(UGSTN_MinionCombatSubsystem::EStage::LODUpdate); }).Num();
	const int32 DrainWrites = Trace.FilterByPredicate([](const FIntPoint& Write) { return Write.X == static_cast<int32>(UGSTN_MinionCombatSubsystem::EStage::InboxDrain); }).Num();
	TestEqual(TEXT("every T0 minion is scheduled every frame"), LODWrites, 6);
	TestEqual(TEXT("the three hits are drained"), DrainWrites, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionRuntimeTimelineSkipTest, "GSTNMinion.Runtime.TimelineSkipFiresOnce", GSTN_MINION_RUNTIME_TEST_FLAGS)
bool FGSTN_MinionRuntimeTimelineSkipTest::RunTest(const FString& Parameters)
{
	FMinionRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}

	// A 20-frame timeline with events on frames a 1-in-4 schedule steps over
	const TArray<int32> EventFrames = { 0, 1, 2, 3, 5, 6, 7, 11, 13, 19 };
	TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transient));
	Timeline->Data.TotalFrames = 20;
	Timeline->Data.FirstActionableFrame = 20;
	for (const int32 EventFrame : EventFrames)
	{
		TInstancedStruct<FGSTN_TimelineTrack>& Track = Timeline->Data.Tracks.AddDefaulted_GetRef();
		Track.InitializeAs<FGSTN_TimelineTrack_Event>();
		Track.GetMutable<FGSTN_TimelineTrack_Event>().StartFrame = EventFrame;
		Track.GetMutable<FGSTN_TimelineTrack_Event>().EndFrame = EventFrame + 1;
	}

	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeDefinition();
	FGSTN_MinionMove& Move = Definition->Moves.AddDefaulted_GetRef();
	Move.Command = TestCommand();
	Move.Timeline = Timeline.Get();
	Move.CooldownFrames = 1000;

	TMap<int32, int32> Fired;
	const FDelegateHandle Listener = Rig.Minions->OnTimelineEvent.AddLambda([&Fired](const FGSTN_MinionHandle&, const FGSTN_TimelineTrack_Event& Track, int32)
	{
		Fired.FindOrAdd(Track.StartFrame)++;
	});

	Rig.Minions->SetForcedLODTier(static_cast<int32>(EGSTN_MinionLODTier::Combat));
	const FGSTN_MinionHandle Handle = Rig.Spawn(Definition.Get());
	Rig.Minions->TickBatch(1);
	TestTrue(TEXT("the command is accepted"), Rig.Minions->RequestCommand(Handle, Move.Command));
	Rig.Minions->TickBatch(2);
	TestEqual(TEXT("the move started (MoveId 1)"), static_cast<int32>(Rig.Minions->GetStore().MoveId[Handle.Index]), 1);

	// From now on the slot is Far: interval 4, so it steps 4 frames at a time
	Rig.Minions->SetForcedLODTier(static_cast<int32>(EGSTN_MinionLODTier::Far));
	int32 Runs = 0;
	for (int32 Frame = 3; Frame <= 40; ++Frame)
	{
		Rig.Minions->TickBatch(Frame);
		Runs += Rig.Minions->GetLastBatchStats().Scheduled;
	}
	Rig.Minions->OnTimelineEvent.Remove(Listener);

	TestTrue(TEXT("the slot skipped frames (fewer runs than frames)"), Runs < 38);
	for (const int32 EventFrame : EventFrames)
	{
		TestEqual(*FString::Printf(TEXT("the event of frame %d fired once"), EventFrame), Fired.FindRef(EventFrame), 1);
	}
	TestEqual(TEXT("no other event fired"), Fired.Num(), EventFrames.Num());
	TestEqual(TEXT("the move ended"), static_cast<int32>(Rig.Minions->GetStore().MoveId[Handle.Index]), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionRuntimeSpawnBudgetTest, "GSTNMinion.Runtime.SpawnQueueBudget", GSTN_MINION_RUNTIME_TEST_FLAGS)
bool FGSTN_MinionRuntimeSpawnBudgetTest::RunTest(const FString& Parameters)
{
	FMinionRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	FScopedCVarInt Budget(TEXT("gstn.Minion.SpawnsPerFrame"), 4);
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeDefinition();

	TArray<FGSTN_MinionHandle> Handles;
	for (int32 Index = 0; Index < 10; ++Index)
	{
		Handles.Add(Rig.Spawn(Definition.Get(), FVector(100.0 * Index, 0.0, 0.0)));
		TestTrue(TEXT("a queued spawn returns a live handle at once"), Rig.Minions->IsValidHandle(Handles.Last()));
	}
	TestNull(TEXT("a queued minion has no actor yet"), Rig.Minions->GetActorOfSlot(Handles[0].Index));

	const int32 Expected[] = { 4, 4, 2 };
	for (int32 Batch = 0; Batch < 3; ++Batch)
	{
		Rig.Minions->TickBatch(Batch + 1);
		TestEqual(*FString::Printf(TEXT("batch %d activates its budget"), Batch + 1), Rig.Minions->GetLastBatchStats().SpawnsActivated, Expected[Batch]);
	}
	TestEqual(TEXT("all ten are active"), Rig.Minions->GetLastBatchStats().Alive, 10);
	// FIFO: the first request got the first activation, so slot order equals request order here
	TestNotNull(TEXT("the first request has its actor"), Rig.Minions->GetActorOfSlot(Handles[0].Index));
	TestNotNull(TEXT("the last request has its actor"), Rig.Minions->GetActorOfSlot(Handles[9].Index));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionRuntimePoolReuseTest, "GSTNMinion.Runtime.PoolReuse", GSTN_MINION_RUNTIME_TEST_FLAGS)
bool FGSTN_MinionRuntimePoolReuseTest::RunTest(const FString& Parameters)
{
	FMinionRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeDefinition();

	const FGSTN_MinionHandle First = Rig.Spawn(Definition.Get());
	Rig.Minions->TickBatch(1);
	AGSTN_MinionActor* Actor = Rig.Minions->GetActorOfSlot(First.Index);
	if (!TestNotNull(TEXT("the activated minion has an actor"), Actor))
	{
		return false;
	}
	TestTrue(TEXT("the component carries the handle"), Actor->GetMinionComponent()->GetHandle() == First);

	TestTrue(TEXT("despawn succeeds"), Rig.Minions->Despawn(First));
	TestFalse(TEXT("the old handle is invalid"), Rig.Minions->IsValidHandle(First));
	TestTrue(TEXT("the actor is back in the pool"), Actor->IsPooled() && Actor->IsHidden());
	TestEqual(TEXT("one pooled actor"), Rig.Minions->GetNumPooledActors(), 1);
	TestTrue(TEXT("the pooled component forgot the handle"), Actor->GetMinionComponent()->GetHandle().IsNull());

	const FGSTN_MinionHandle Second = Rig.Spawn(Definition.Get());
	TestEqual(TEXT("the lowest free slot is reused"), static_cast<int32>(Second.Index), static_cast<int32>(First.Index));
	TestNotEqual(TEXT("the generation was bumped"), static_cast<int32>(Second.Generation), static_cast<int32>(First.Generation));
	Rig.Minions->TickBatch(2);
	TestTrue(TEXT("the pooled actor is reused"), Rig.Minions->GetActorOfSlot(Second.Index) == Actor);
	TestEqual(TEXT("the pool is empty again"), Rig.Minions->GetNumPooledActors(), 0);
	TestTrue(TEXT("the component carries the new handle"), Actor->GetMinionComponent()->GetHandle() == Second);
	TestFalse(TEXT("the old handle still does not resolve"), Rig.Minions->IsValidHandle(First));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
