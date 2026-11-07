// Automation tests of the minion Core: store, handles, entity ids, scheduler, attack tokens.

#include "GSTN_MinionTestUtils.h"
#include "HAL/IConsoleManager.h"
#include "Schedule/GSTN_MinionSchedule.h"
#include "Store/GSTN_MinionStore.h"
#include "Tokens/GSTN_MinionAttackTokens.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionStoreAllocFreeTest, "GSTNMinion.Core.Store.AllocFreeReuse", GSTNMinionTest::Flags)
bool FGSTN_MinionStoreAllocFreeTest::RunTest(const FString& Parameters)
{
	FGSTN_MinionStore Store;
	Store.Reset(4);
	TestEqual(TEXT("Capacity"), Store.Capacity(), 4);
	TestEqual(TEXT("Empty"), Store.Num(), 0);

	const FGSTN_MinionHandle A = Store.Allocate();
	const FGSTN_MinionHandle B = Store.Allocate();
	const FGSTN_MinionHandle C = Store.Allocate();
	TestEqual(TEXT("Lowest free index first (A)"), static_cast<int32>(A.Index), 0);
	TestEqual(TEXT("Lowest free index first (B)"), static_cast<int32>(B.Index), 1);
	TestEqual(TEXT("Lowest free index first (C)"), static_cast<int32>(C.Index), 2);
	TestEqual(TEXT("Three alive"), Store.Num(), 3);

	// Free the middle and the first: the next two allocations take the lowest holes, in order
	TestTrue(TEXT("Free B"), Store.Free(B));
	TestTrue(TEXT("Free A"), Store.Free(A));
	const FGSTN_MinionHandle D = Store.Allocate();
	const FGSTN_MinionHandle E = Store.Allocate();
	TestEqual(TEXT("Reuses index 0 first"), static_cast<int32>(D.Index), 0);
	TestEqual(TEXT("Then index 1"), static_cast<int32>(E.Index), 1);
	TestEqual(TEXT("Generation of a reused slot is bumped (D)"), static_cast<int32>(D.Generation), static_cast<int32>(A.Generation) + 1);
	TestEqual(TEXT("Generation of a reused slot is bumped (E)"), static_cast<int32>(E.Generation), static_cast<int32>(B.Generation) + 1);
	TestTrue(TEXT("C untouched"), Store.IsValid(C));

	// Fill, then the store refuses
	const FGSTN_MinionHandle F = Store.Allocate();
	TestEqual(TEXT("Last slot"), static_cast<int32>(F.Index), 3);
	TestTrue(TEXT("Full"), Store.IsFull());
	TestTrue(TEXT("A full store returns the null handle"), Store.Allocate().IsNull());

	// A recycled slot starts from defaults
	Store.Health[D.Index] = 12.f;
	Store.Position[D.Index] = FVector3f(1.f, 2.f, 3.f);
	TestTrue(TEXT("Free D"), Store.Free(D));
	const FGSTN_MinionHandle G = Store.Allocate();
	TestEqual(TEXT("Same slot"), static_cast<int32>(G.Index), static_cast<int32>(D.Index));
	TestEqual(TEXT("Health reset"), Store.Health[G.Index], 0.f);
	TestTrue(TEXT("Position reset"), Store.Position[G.Index].IsZero());
	TestEqual(TEXT("Iteration visits live slots in order"), Store.NextAliveSlot(0), 0);
	TestEqual(TEXT("Iteration continues"), Store.NextAliveSlot(1), 1);
	TestEqual(TEXT("Iteration ends"), Store.NextAliveSlot(4), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionStoreStaleTest, "GSTNMinion.Core.Store.StaleHandles", GSTNMinionTest::Flags)
bool FGSTN_MinionStoreStaleTest::RunTest(const FString& Parameters)
{
	FGSTN_MinionStore Store;
	Store.Reset(2);

	TestFalse(TEXT("The null handle is never valid"), Store.IsValid(FGSTN_MinionHandle()));
	TestFalse(TEXT("Freeing the null handle changes nothing"), Store.Free(FGSTN_MinionHandle()));

	const FGSTN_MinionHandle Old = Store.Allocate();
	TestTrue(TEXT("Valid while alive"), Store.IsValid(Old));
	TestTrue(TEXT("Free"), Store.Free(Old));
	TestFalse(TEXT("Stale after free"), Store.IsValid(Old));
	TestFalse(TEXT("Second free of the same handle is refused"), Store.Free(Old));

	const FGSTN_MinionHandle New = Store.Allocate();
	TestEqual(TEXT("The slot was recycled"), static_cast<int32>(New.Index), static_cast<int32>(Old.Index));
	TestFalse(TEXT("The old handle does not resolve to the new occupant"), Store.IsValid(Old));
	TestFalse(TEXT("The old handle cannot free the new occupant"), Store.Free(Old));
	TestTrue(TEXT("The new occupant is still alive"), Store.IsValid(New));

	// Out-of-range index
	TestFalse(TEXT("An out-of-range index is invalid"), Store.IsValid(FGSTN_MinionHandle(99, 1)));

	// The generation wraps from 65535 to 1, never to 0 (0 is the null generation)
	FGSTN_MinionStore Single;
	Single.Reset(1);
	bool bSawZero = false;
	FGSTN_MinionHandle Previous;
	for (int32 Round = 0; Round < 70000; ++Round)
	{
		const FGSTN_MinionHandle Handle = Single.Allocate();
		bSawZero |= Handle.Generation == 0;
		if (!Previous.IsNull() && Single.IsValid(Previous))
		{
			AddError(TEXT("A freed handle resolved again"));
			break;
		}
		Single.Free(Handle);
		Previous = Handle;
	}
	TestFalse(TEXT("A live generation is never 0 across a wrap"), bSawZero);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionStoreCapacityTest, "GSTNMinion.Core.Store.CapacityAndEntityIds", GSTNMinionTest::Flags)
bool FGSTN_MinionStoreCapacityTest::RunTest(const FString& Parameters)
{
	// gstn.Minion.Max is clamped to [1, MaxMinionIndex + 1]
	IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("gstn.Minion.Max"));
	if (TestNotNull(TEXT("gstn.Minion.Max exists"), Var))
	{
		const int32 Saved = Var->GetInt();
		Var->Set(0, ECVF_SetByCode);
		TestEqual(TEXT("Clamped up to 1"), GSTNMinion::GetConfiguredCapacity(), 1);
		Var->Set(1000000, ECVF_SetByCode);
		TestEqual(TEXT("Clamped down to the index range"), GSTNMinion::GetConfiguredCapacity(), static_cast<int32>(GSTNEntityKinds::MaxMinionIndex) + 1);
		Var->Set(40, ECVF_SetByCode);
		TestEqual(TEXT("Taken as is inside the range"), GSTNMinion::GetConfiguredCapacity(), 40);
		FGSTN_MinionStore Store;
		Store.ResetToConfiguredCapacity();
		TestEqual(TEXT("The store takes the configured capacity"), Store.Capacity(), 40);
		Var->Set(Saved, ECVF_SetByCode);
	}

	FGSTN_MinionStore Store;
	Store.Reset(8);
	Store.Allocate();
	Store.Allocate();
	FGSTN_MinionHandle Handle = Store.Allocate();
	Store.Free(Store.HandleOfSlot(0));
	Store.Allocate();
	Store.Free(Handle);
	Handle = Store.Allocate();

	const int32 EntityId = GSTNMinion::ToEntityId(Handle);
	TestEqual(TEXT("The pipeline id is the shared range id of the slot"), EntityId, GSTNEntityKinds::MakeMinionEntityId(Handle.Index));
	TestTrue(TEXT("It is a minion id"), GSTNEntityKinds::IsMinionEntityId(EntityId));
	TestEqual(TEXT("The slot index survives the round trip"), static_cast<int32>(GSTNEntityKinds::MinionIndexOf(EntityId)), static_cast<int32>(Handle.Index));
	TestTrue(TEXT("The id resolves to the live handle"), Store.HandleFromEntityId(EntityId) == Handle);
	TestTrue(TEXT("A fighter id does not resolve"), Store.HandleFromEntityId(3).IsNull());

	const FGSTN_EntityRef Ref = GSTNMinion::ToEntityRef(Handle);
	TestEqual(TEXT("Ref kind"), static_cast<int32>(Ref.Kind), static_cast<int32>(GSTNEntityKinds::Minion));
	TestEqual(TEXT("Ref id is the slot index"), static_cast<int32>(Ref.Id), static_cast<int32>(Handle.Index));
	TestEqual(TEXT("Ref serial is the low 8 bits of the generation"), static_cast<int32>(Ref.Serial), static_cast<int32>(Handle.Generation & 0xFF));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionScheduleSpreadTest, "GSTNMinion.Core.Schedule.SpreadAndExactlyOnce", GSTNMinionTest::Flags)
bool FGSTN_MinionScheduleSpreadTest::RunTest(const FString& Parameters)
{
	const int32 Intervals[] = { 1, 2, 4, 16 };
	const int32 SlotCounts[] = { 1, 5, 40, 48, 128 };
	for (const int32 Interval : Intervals)
	{
		for (const int32 SlotCount : SlotCounts)
		{
			// Each slot runs exactly once in any window of Interval consecutive frames, whatever the window starts at
			for (const int32 Start : { 0, 3, 1000 })
			{
				TArray<int32> RunsPerSlot;
				RunsPerSlot.Init(0, SlotCount);
				TArray<int32> RunsPerFrame;
				for (int32 Frame = Start; Frame < Start + Interval; ++Frame)
				{
					TArray<int32> Running;
					GSTNMinionSchedule::CollectRunning(Frame, SlotCount, Interval, Running);
					RunsPerFrame.Add(Running.Num());
					for (const int32 Slot : Running)
					{
						++RunsPerSlot[Slot];
					}
				}
				const FString Label = FString::Printf(TEXT("P=%d N=%d start=%d"), Interval, SlotCount, Start);
				for (int32 Slot = 0; Slot < SlotCount; ++Slot)
				{
					if (RunsPerSlot[Slot] != 1)
					{
						AddError(FString::Printf(TEXT("%s: slot %d ran %d times in one period"), *Label, Slot, RunsPerSlot[Slot]));
						break;
					}
				}
				const float Average = static_cast<float>(SlotCount) / static_cast<float>(Interval);
				for (const int32 Count : RunsPerFrame)
				{
					if (FMath::Abs(static_cast<float>(Count) - Average) > 1.f)
					{
						AddError(FString::Printf(TEXT("%s: %d slots in a frame, average %.2f"), *Label, Count, Average));
						break;
					}
				}
			}
		}
	}

	// NextRunFrame lands on a running frame and is the first such frame
	for (int32 Slot = 0; Slot < 20; ++Slot)
	{
		for (int32 Frame = 0; Frame < 40; ++Frame)
		{
			const int32 Next = GSTNMinionSchedule::NextRunFrame(Frame, Slot, 4);
			if (!GSTNMinionSchedule::ShouldRun(Next, Slot, 4) || Next < Frame || Next - Frame >= 4)
			{
				AddError(FString::Printf(TEXT("NextRunFrame(%d, slot %d) = %d is wrong"), Frame, Slot, Next));
				return true;
			}
			for (int32 Between = Frame; Between < Next; ++Between)
			{
				if (GSTNMinionSchedule::ShouldRun(Between, Slot, 4))
				{
					AddError(TEXT("NextRunFrame skipped a running frame"));
					return true;
				}
			}
		}
	}

	// Large frame counters do not overflow
	int32 Runs = 0;
	for (int32 Frame = MAX_int32 - 15; Frame > 0 && Frame <= MAX_int32 && Runs < 100; ++Frame)
	{
		Runs += GSTNMinionSchedule::ShouldRun(Frame, 7, 16) ? 1 : 0;
		if (Frame == MAX_int32)
		{
			break;
		}
	}
	TestEqual(TEXT("A slot runs once in the 16 frames ending at MAX_int32"), Runs, 1);
	TestEqual(TEXT("StepFrames is the interval"), GSTNMinionSchedule::StepFrames(4), 4);
	TestEqual(TEXT("StepFrames never below 1"), GSTNMinionSchedule::StepFrames(0), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionTokensCapTest, "GSTNMinion.Core.Tokens.CapAndRelease", GSTNMinionTest::Flags)
bool FGSTN_MinionTokensCapTest::RunTest(const FString& Parameters)
{
	constexpr int32 Target = 100;
	FGSTN_MinionAttackTokens Tokens;
	TestEqual(TEXT("Default cap"), Tokens.GetMaxPerTarget(), 4);

	// Six minions ask in a scrambled order; the four lowest slots win
	for (const int32 Slot : { 5, 3, 9, 1, 7, 2 })
	{
		Tokens.Request(Target, Slot);
	}
	TestFalse(TEXT("Nothing is held before Resolve"), Tokens.Holds(Target, 1));
	TestEqual(TEXT("Four granted"), Tokens.Resolve(), 4);
	TestEqual(TEXT("Four holders"), Tokens.NumHolders(Target), 4);
	TestEqual(TEXT("Two waiting"), Tokens.NumWaiting(Target), 2);
	for (const int32 Slot : { 1, 2, 3, 5 })
	{
		TestTrue(FString::Printf(TEXT("Slot %d holds"), Slot), Tokens.Holds(Target, Slot));
	}
	TestTrue(TEXT("Slot 7 waits"), Tokens.IsWaiting(Target, 7));
	TestTrue(TEXT("Slot 9 waits"), Tokens.IsWaiting(Target, 9));
	TestEqual(TEXT("A second Resolve grants nothing"), Tokens.Resolve(), 0);

	// A lower slot arriving late does not preempt a holder
	Tokens.Request(Target, 0);
	TestEqual(TEXT("Still nothing granted"), Tokens.Resolve(), 0);
	TestTrue(TEXT("Slot 0 waits"), Tokens.IsWaiting(Target, 0));

	// A release (death or hit) frees one token; the lowest waiting slot gets it
	TestTrue(TEXT("Release slot 3"), Tokens.Release(Target, 3));
	TestFalse(TEXT("Releasing twice finds nothing"), Tokens.Release(Target, 3));
	TestEqual(TEXT("One granted"), Tokens.Resolve(), 1);
	TestTrue(TEXT("Slot 0 got it"), Tokens.Holds(Target, 0));
	TestEqual(TEXT("Still capped at four"), Tokens.NumHolders(Target), 4);

	// ReleaseSlot clears a slot everywhere
	constexpr int32 OtherTarget = 200;
	Tokens.Request(OtherTarget, 1);
	Tokens.Resolve();
	TestTrue(TEXT("Slot 1 holds on both targets"), Tokens.Holds(Target, 1) && Tokens.Holds(OtherTarget, 1));
	TestEqual(TEXT("Both entries removed"), Tokens.ReleaseSlot(1), 2);
	TestFalse(TEXT("Gone from the first target"), Tokens.Holds(Target, 1));
	TestFalse(TEXT("Gone from the second target"), Tokens.Holds(OtherTarget, 1));

	// Targets are independent
	TestEqual(TEXT("The other target has no holders"), Tokens.NumHolders(OtherTarget), 0);
	Tokens.ReleaseTarget(Target);
	TestEqual(TEXT("ReleaseTarget drops the target"), Tokens.NumHolders(Target) + Tokens.NumWaiting(Target), 0);

	// The cap is configurable and at least 1
	Tokens.Reset();
	Tokens.SetMaxPerTarget(0);
	TestEqual(TEXT("Cap is at least 1"), Tokens.GetMaxPerTarget(), 1);
	Tokens.SetMaxPerTarget(5);
	for (int32 Slot = 0; Slot < 8; ++Slot)
	{
		Tokens.Request(Target, Slot);
		Tokens.Request(Target, Slot);
	}
	TestEqual(TEXT("A duplicate request adds nothing"), Tokens.NumEntries(), 8);
	TestEqual(TEXT("Cap of five"), Tokens.Resolve(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionTokensOrderTest, "GSTNMinion.Core.Tokens.GrantOrderIsDeterministic", GSTNMinionTest::Flags)
bool FGSTN_MinionTokensOrderTest::RunTest(const FString& Parameters)
{
	// The same set of requesters gives the same holders whatever order they asked in
	TArray<int32> Slots = { 14, 2, 9, 30, 4, 11, 6, 21 };
	TSet<int32> FirstHolders;
	FRandomStream Random(1234);
	for (int32 Round = 0; Round < 20; ++Round)
	{
		for (int32 Index = Slots.Num() - 1; Index > 0; --Index)
		{
			Slots.Swap(Index, Random.RandRange(0, Index));
		}
		FGSTN_MinionAttackTokens Tokens;
		for (const int32 Slot : Slots)
		{
			Tokens.Request(7, Slot);
		}
		Tokens.Resolve();

		TSet<int32> Holders;
		for (const int32 Slot : Slots)
		{
			if (Tokens.Holds(7, Slot))
			{
				Holders.Add(Slot);
			}
		}
		if (Round == 0)
		{
			FirstHolders = Holders;
		}
		TestTrue(TEXT("Holders do not depend on the request order"), Holders.Num() == 4 && Holders.Includes(FirstHolders) && FirstHolders.Includes(Holders));
	}
	TestTrue(TEXT("The four lowest slots hold"), FirstHolders.Contains(2) && FirstHolders.Contains(4) && FirstHolders.Contains(6) && FirstHolders.Contains(9));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
