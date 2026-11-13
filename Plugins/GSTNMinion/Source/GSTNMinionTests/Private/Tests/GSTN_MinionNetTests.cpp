// Automation tests of the authority replication layer (M8 G6): MutatePack's dormancy order (a spy on the subsystem), pack assignment by
// spawn group, and the registry hash check through the client registry. A transient game world, no net driver: what is tested is the
// order and the bookkeeping of the calls, not the engine's replication (the harness scenarios cover that).

#include "Data/GSTN_MinionDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Net/GSTN_MinionClientRegistry.h"
#include "Net/GSTN_MinionNetManager.h"
#include "Net/GSTN_MinionNetRelevancy.h"
#include "Net/GSTN_MinionNetRates.h"
#include "Net/GSTN_MinionPack.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_MINION_NET_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** A transient game world with an activated minion subsystem (a test world has no begin play) */
	struct FNetRig
	{
		FNetRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNMinionNetTestWorld"));
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

		~FNetRig()
		{
			if (Minions)
			{
				Minions->NetTraceForTests = nullptr;
				Minions->SetForcedLODTier(INDEX_NONE);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		FGSTN_MinionHandle Spawn(UGSTN_MinionDefinition* Definition, int32 SpawnGroup, const FVector& Location = FVector::ZeroVector)
		{
			FGSTN_MinionSpawnParams Params;
			Params.SpawnGroup = SpawnGroup;
			Params.bPlaySpawnTimeline = false;
			return Minions->EnqueueSpawn(Definition, FTransform(Location), Params);
		}

		UWorld* World = nullptr;
		UGSTN_MinionCombatSubsystem* Minions = nullptr;
	};

	TStrongObjectPtr<UGSTN_MinionDefinition> MakeNetTestDefinition()
	{
		TStrongObjectPtr<UGSTN_MinionDefinition> Definition(NewObject<UGSTN_MinionDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
		Definition->Stats.MaxHealth = 40.f;
		return Definition;
	}

	/**
	 * Replays a spy trace with the pack's dormancy: Sleep makes it dormant, Wake wakes it, Flush allows the next change while it stays
	 * dormant. A Mutate while dormant and not flushed since the last Sleep is the forbidden "mutate, then flush" order. Empty = clean
	 */
	FString FindMutateWhileDormant(const TArray<FName>& Trace)
	{
		bool bDormant = false;
		bool bFlushed = false;
		for (int32 Index = 0; Index < Trace.Num(); ++Index)
		{
			const FName Step = Trace[Index];
			if (Step == TEXT("Sleep"))
			{
				bDormant = true;
				bFlushed = false;
			}
			else if (Step == TEXT("Wake"))
			{
				bDormant = false;
			}
			else if (Step == TEXT("Flush"))
			{
				bFlushed = true;
			}
			else if (Step == TEXT("Mutate"))
			{
				if (bDormant && !bFlushed)
				{
					return FString::Printf(TEXT("Mutate at trace index %d on a dormant pack"), Index);
				}
				bFlushed = false;
			}
		}
		return FString();
	}

	FString TraceToString(const TArray<FName>& Trace)
	{
		FString Out;
		for (const FName Step : Trace)
		{
			Out += (Out.IsEmpty() ? TEXT("") : TEXT(",")) + Step.ToString();
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionNetMutateOrderTest, "GSTNMinion.Runtime.Net.MutatePackWakesBeforeMutation", GSTN_MINION_NET_TEST_FLAGS)
bool FGSTN_MinionNetMutateOrderTest::RunTest(const FString& Parameters)
{
	FNetRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeNetTestDefinition();
	TArray<FName> Trace;
	Rig.Minions->NetTraceForTests = &Trace;

	// Two minions forced to T3 at once: the pack is created, gets its two items, then goes to sleep after the marks
	const FGSTN_MinionHandle First = Rig.Spawn(Definition.Get(), 0);
	const FGSTN_MinionHandle Second = Rig.Spawn(Definition.Get(), 0, FVector(200.0, 0.0, 0.0));
	Rig.Minions->SetForcedLODTier(static_cast<int32>(EGSTN_MinionLODTier::Dormant));
	Rig.Minions->TickBatch(1);
	AGSTN_MinionPack* Pack = Rig.Minions->GetPackOfSlot(First.Index);
	if (!TestNotNull(TEXT("the first minion has a pack"), Pack))
	{
		return false;
	}
	TestTrue(TEXT("both minions share the pack of their group"), Rig.Minions->GetPackOfSlot(Second.Index) == Pack);
	TestEqual(TEXT("two items"), Pack->GetNetArray().Items.Num(), 2);
	TestEqual(TEXT("all-T3 pack has ceiling 0"), Pack->CeilingHz, 0);
	TestTrue(TEXT("all-T3 pack is dormant"), Pack->NetDormancy == DORM_DormantAll);
	TestTrue(TEXT("sleep came after the marks of the adds"), Trace.Num() > 0 && Trace.Last() == FName(TEXT("Sleep")) && Trace.Contains(FName(TEXT("MarkItem"))));

	// A member leaves while the pack sleeps: flushed BEFORE the array changes, then the marks
	const int32 FirstSleep = Trace.Num();
	TestTrue(TEXT("despawn succeeds"), Rig.Minions->Despawn(First));
	const TArray<FName> RemoveSteps(Trace.GetData() + FirstSleep, Trace.Num() - FirstSleep);
	TestEqual(TEXT("remove on a sleeping all-T3 pack: Flush, Mutate, MarkProperty"), TraceToString(RemoveSteps), FString(TEXT("Flush,Mutate,MarkProperty")));
	TestEqual(TEXT("one item left"), Pack->GetNetArray().Items.Num(), 1);

	// The survivor comes back to T0: the pack wakes before its item is written
	Rig.Minions->SetForcedLODTier(static_cast<int32>(EGSTN_MinionLODTier::Combat));
	const int32 BeforeWake = Trace.Num();
	for (int32 Frame = 2; Frame <= 40 && Pack->CeilingHz == 0; ++Frame)
	{
		Rig.Minions->TickBatch(Frame);
	}
	TestEqual(TEXT("ceiling back to the T0 rate"), Pack->CeilingHz, GSTNMinionNetRate::RateHz(EGSTN_MinionLODTier::Combat));   // the project ini sets T0 to 10 Hz
	TestTrue(TEXT("the pack is awake"), Pack->NetDormancy == DORM_Awake);
	const int32 WakeIndex = Trace.Find(FName(TEXT("Wake")));
	int32 MutateAfterWake = INDEX_NONE;
	for (int32 Index = BeforeWake; Index < Trace.Num(); ++Index)
	{
		if (Trace[Index] == FName(TEXT("Mutate")))
		{
			MutateAfterWake = Index;
			break;
		}
	}
	TestTrue(TEXT("a Wake was recorded"), WakeIndex != INDEX_NONE && WakeIndex >= BeforeWake);
	TestTrue(FString::Printf(TEXT("the item update (Teleport cleared) came after the Wake: %s"), *TraceToString(Trace)),
		MutateAfterWake != INDEX_NONE && WakeIndex < MutateAfterWake);
	const FString Violation = FindMutateWhileDormant(Trace);
	TestTrue(FString::Printf(TEXT("no mutation of a dormant, unflushed pack anywhere in the run: %s"), *Violation), Violation.IsEmpty());

	// Every mutation is followed by its marks before the next step
	for (int32 Index = 0; Index < Trace.Num(); ++Index)
	{
		if (Trace[Index] == FName(TEXT("Mutate")))
		{
			int32 Next = Index + 1;
			while (Next < Trace.Num() && Trace[Next] == FName(TEXT("MarkItem")))
			{
				++Next;
			}
			TestTrue(TEXT("MarkProperty closes each mutation"), Next < Trace.Num() && Trace[Next] == FName(TEXT("MarkProperty")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionNetPackAssignmentTest, "GSTNMinion.Runtime.Net.PackAssignmentBySpawnGroup", GSTN_MINION_NET_TEST_FLAGS)
bool FGSTN_MinionNetPackAssignmentTest::RunTest(const FString& Parameters)
{
	FNetRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeNetTestDefinition();

	TArray<FGSTN_MinionHandle> GroupA;
	for (int32 Index = 0; Index < 17; ++Index)
	{
		GroupA.Add(Rig.Spawn(Definition.Get(), 3, FVector(100.0 * Index, 0.0, 0.0)));
	}
	const FGSTN_MinionHandle Loner = Rig.Spawn(Definition.Get(), 7, FVector(0.0, 500.0, 0.0));
	for (int32 Frame = 1; Frame <= 8; ++Frame)
	{
		Rig.Minions->TickBatch(Frame);
	}

	TestEqual(TEXT("three packs: 16 + 1 of group 3, 1 of group 7"), Rig.Minions->GetNumPacks(), 3);
	AGSTN_MinionPack* Full = Rig.Minions->GetPackOfSlot(GroupA[0].Index);
	AGSTN_MinionPack* Overflow = Rig.Minions->GetPackOfSlot(GroupA[16].Index);
	AGSTN_MinionPack* Own = Rig.Minions->GetPackOfSlot(Loner.Index);
	if (!TestTrue(TEXT("every minion has a pack"), Full && Overflow && Own))
	{
		return false;
	}
	TestEqual(TEXT("the first pack is full"), Full->GetNetArray().Items.Num(), AGSTN_MinionPack::MaxItems);
	TestTrue(TEXT("the 17th of the group overflows into a second pack"), Overflow != Full && Overflow->GetNetArray().Items.Num() == 1);
	TestTrue(TEXT("another group never shares a pack"), Own != Full && Own != Overflow);
	TSet<int32> Ids;
	for (int32 Index = 0; Index < 16; ++Index)
	{
		TestTrue(TEXT("the first 16 are in the first pack"), Rig.Minions->GetPackOfSlot(GroupA[Index].Index) == Full);
		Ids.Add(Rig.Minions->GetItemIdOfSlot(GroupA[Index].Index));
	}
	TestEqual(TEXT("16 distinct item ids"), Ids.Num(), 16);
	for (const FGSTN_MinionNetItem& Item : Full->GetNetArray().Items)
	{
		TestTrue(TEXT("every item carries its type"), Item.Wire.bHasType && Item.Wire.TypeIndex == 0);
	}
	TestEqual(TEXT("18 adds"), Rig.Minions->GetNetStats().ItemAdds, static_cast<int64>(18));

	// An emptied pack is destroyed and its id is the next one reused
	const int32 OverflowId = Overflow->GetPackId();
	TestTrue(TEXT("despawn the overflow member"), Rig.Minions->Despawn(GroupA[16]));
	TestEqual(TEXT("two packs left"), Rig.Minions->GetNumPacks(), 2);
	const FGSTN_MinionHandle Late = Rig.Spawn(Definition.Get(), 9);
	Rig.Minions->TickBatch(9);
	AGSTN_MinionPack* Reused = Rig.Minions->GetPackOfSlot(Late.Index);
	TestTrue(TEXT("a new group gets the freed pack id"), Reused && Reused->GetPackId() == OverflowId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionNetRegistryCheckTest, "GSTNMinion.Runtime.Net.RegistryHashCheck", GSTN_MINION_NET_TEST_FLAGS)
bool FGSTN_MinionNetRegistryCheckTest::RunTest(const FString& Parameters)
{
	FNetRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	UGSTN_MinionClientRegistry* Client = Rig.World->GetSubsystem<UGSTN_MinionClientRegistry>();
	AGSTN_MinionNetManager* Manager = Rig.Minions->GetNetManager();
	if (!TestTrue(TEXT("the client registry and the net manager exist"), Client && Manager))
	{
		return false;
	}

	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeNetTestDefinition();
	Rig.Spawn(Definition.Get(), 0);
	Rig.Minions->TickBatch(1);
	const FGSTN_MinionNetRegistryInfo& Info = Manager->GetRegistryInfo();
	TestEqual(TEXT("one type in the table"), Info.TypePaths.Num(), 1);
	TestTrue(TEXT("the manager's hash is the hash of the table"),
		static_cast<uint32>(Info.Hash) == FGSTN_MinionRegistryHash::Compute(TArray<FString>({ Definition->GetPathName() })));

	// What a client does in OnRep: equal tables match
	Client->ApplyRegistryInfo(Info);
	TestTrue(TEXT("same table: match"), Client->HasCheckedRegistry() && Client->IsRegistryMatching());
	TestEqual(TEXT("no mismatch"), Client->GetRegistryMismatches(), 0);
	TestTrue(TEXT("the client resolved the type"), Client->GetDefinition(0) == Definition.Get());

	// A server whose table differs (another hash for the same paths): error log and counter, no kick
	FGSTN_MinionNetRegistryInfo Other = Info;
	Other.Hash = static_cast<int32>(static_cast<uint32>(Info.Hash) ^ 0x5A5A5A5Au);
	AddExpectedError(TEXT("MinionNet registry MISMATCH"), EAutomationExpectedErrorFlags::Contains, 1);
	Client->ApplyRegistryInfo(Other);
	TestFalse(TEXT("different table: mismatch"), Client->IsRegistryMatching());
	TestEqual(TEXT("one mismatch counted"), Client->GetRegistryMismatches(), 1);
	TestEqual(TEXT("two checks"), Client->GetRegistryChecks(), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
