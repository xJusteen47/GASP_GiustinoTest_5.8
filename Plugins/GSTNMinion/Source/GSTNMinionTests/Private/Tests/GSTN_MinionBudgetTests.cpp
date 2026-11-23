// Automation tests of the host budgets (M8 G11): the MaxActiveT0 demotion of the LOD step. Headless, in a transient game world; they drive
// TickBatch directly and place the "players" with SetTestPlayerPositions (a headless world has no pawns). A spawn activates at most
// gstn.Minion.SpawnsPerFrame (4) per batch frame, so every test ticks a few frames before it reads a tier.

#include "Data/GSTN_MinionDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_MINION_BUDGET_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** Sets a cvar for the test and puts the previous value back */
	struct FCVarOverride
	{
		FCVarOverride(const TCHAR* Name, int32 Value)
			: Variable(IConsoleManager::Get().FindConsoleVariable(Name))
		{
			if (Variable)
			{
				Previous = Variable->GetInt();
				Variable->Set(Value, ECVF_SetByCode);
			}
		}
		~FCVarOverride()
		{
			if (Variable)
			{
				Variable->Set(Previous, ECVF_SetByCode);
			}
		}
		IConsoleVariable* Variable = nullptr;
		int32 Previous = 0;
	};

	struct FBudgetRig
	{
		FBudgetRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNMinionBudgetTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Minions = World->GetSubsystem<UGSTN_MinionCombatSubsystem>();
			if (Minions)
			{
				Minions->ActivateForTests();
				// One "player" at the origin: minions at X = 100, 200, ... are at those ground distances, all inside the Combat border
				Minions->SetTestPlayerPositions(TArray<FVector2D>{ FVector2D::ZeroVector });
			}
			Definition.Reset(NewObject<UGSTN_MinionDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
			Definition->Stats.MaxHealth = 40.f;
		}

		~FBudgetRig()
		{
			if (Minions)
			{
				Minions->ClearTestPlayerPositions();
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		FGSTN_MinionHandle Spawn(double X)
		{
			FGSTN_MinionSpawnParams Params;
			Params.bPlaySpawnTimeline = false;
			return Minions->EnqueueSpawn(Definition.Get(), FTransform(FVector(X, 0.0, 0.0)), Params);
		}

		void Tick(int32 Frames)
		{
			for (int32 Index = 0; Index < Frames; ++Index)
			{
				Minions->TickBatch(++Frame);
			}
		}

		int32 Tier(const FGSTN_MinionHandle& Handle) const { return Minions->GetStore().LODTier[Handle.Index]; }

		UWorld* World = nullptr;
		UGSTN_MinionCombatSubsystem* Minions = nullptr;
		TStrongObjectPtr<UGSTN_MinionDefinition> Definition;
		int32 Frame = 0;
	};

	constexpr int32 Combat = static_cast<int32>(EGSTN_MinionLODTier::Combat);
	constexpr int32 Near = static_cast<int32>(EGSTN_MinionLODTier::Near);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBudgetCapDemotesFarthestTest, "GSTNMinion.Runtime.Budget.CapDemotesTheFarthest", GSTN_MINION_BUDGET_TEST_FLAGS)
bool FGSTN_MinionBudgetCapDemotesFarthestTest::RunTest(const FString& Parameters)
{
	FCVarOverride Cap(TEXT("gstn.Minion.MaxActiveT0"), 2);
	FBudgetRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	const FGSTN_MinionHandle A = Rig.Spawn(100.0);
	const FGSTN_MinionHandle B = Rig.Spawn(200.0);
	const FGSTN_MinionHandle C = Rig.Spawn(300.0);
	const FGSTN_MinionHandle D = Rig.Spawn(400.0);
	Rig.Tick(6);

	TestEqual(TEXT("nearest stays Combat"), Rig.Tier(A), Combat);
	TestEqual(TEXT("second nearest stays Combat"), Rig.Tier(B), Combat);
	TestEqual(TEXT("third is demoted to Near"), Rig.Tier(C), Near);
	TestEqual(TEXT("farthest is demoted to Near"), Rig.Tier(D), Near);
	TestEqual(TEXT("active T0 reads the cap"), Rig.Minions->GetActiveT0Count(), 2);
	// Cumulative: at least the two; a silent re-demotion after the Near dwell counts again
	TestTrue(TEXT("two demoted"), Rig.Minions->GetT0DemotedCount() >= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBudgetCapSparesAttackersAndExemptTest, "GSTNMinion.Runtime.Budget.AttackersAndExemptNeverDemoted", GSTN_MINION_BUDGET_TEST_FLAGS)
bool FGSTN_MinionBudgetCapSparesAttackersAndExemptTest::RunTest(const FString& Parameters)
{
	FCVarOverride Cap(TEXT("gstn.Minion.MaxActiveT0"), 2);
	FBudgetRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	// The farthest is exempt (a platform a player stands on) from its first step. Token holders are spared by the same pass
	// (HoldsAny), but the batch's token stage re-resolves every frame, so a token set from outside does not survive a tick: the
	// attacker case is covered by the horde scenario, not here
	const FGSTN_MinionHandle A = Rig.Spawn(100.0);
	const FGSTN_MinionHandle B = Rig.Spawn(200.0);
	const FGSTN_MinionHandle C = Rig.Spawn(300.0);
	const FGSTN_MinionHandle Exempt = Rig.Spawn(400.0);
	// Set after the spawn step (the handle is valid from then); a first demotion may already have sent it to Near, so tick well past the
	// dwell: the stepper pulls it back to Combat and the exemption keeps it there, while the cap settles on the others
	Rig.Tick(1);
	Rig.Minions->SetExemptFromCombatCap(Exempt, true);
	Rig.Tick(120);

	TestEqual(TEXT("the exempt one stays Combat although it is the farthest"), Rig.Tier(Exempt), Combat);
	TestEqual(TEXT("the nearest stays Combat"), Rig.Tier(A), Combat);
	TestEqual(TEXT("the unprotected farthest pay for the overshoot"), Rig.Tier(C), Near);
	TestEqual(TEXT("and the next one"), Rig.Tier(B), Near);
	TestEqual(TEXT("active T0 is the cap"), Rig.Minions->GetActiveT0Count(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBudgetCapOffTest, "GSTNMinion.Runtime.Budget.CapZeroMeansNoCap", GSTN_MINION_BUDGET_TEST_FLAGS)
bool FGSTN_MinionBudgetCapOffTest::RunTest(const FString& Parameters)
{
	FCVarOverride Cap(TEXT("gstn.Minion.MaxActiveT0"), 0);
	FBudgetRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	TArray<FGSTN_MinionHandle> Handles;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Handles.Add(Rig.Spawn(100.0 * (Index + 1)));
	}
	Rig.Tick(8);
	for (const FGSTN_MinionHandle& Handle : Handles)
	{
		TestEqual(TEXT("every minion stays Combat"), Rig.Tier(Handle), Combat);
	}
	TestEqual(TEXT("active T0 counts them all"), Rig.Minions->GetActiveT0Count(), 6);
	TestEqual(TEXT("nothing demoted"), Rig.Minions->GetT0DemotedCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBudgetCapNoChurnTest, "GSTNMinion.Runtime.Budget.DemotedSlotDoesNotChurn", GSTN_MINION_BUDGET_TEST_FLAGS)
bool FGSTN_MinionBudgetCapNoChurnTest::RunTest(const FString& Parameters)
{
	FCVarOverride Cap(TEXT("gstn.Minion.MaxActiveT0"), 1);
	FBudgetRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	const FGSTN_MinionHandle A = Rig.Spawn(100.0);
	const FGSTN_MinionHandle B = Rig.Spawn(500.0);
	Rig.Tick(6);
	TestEqual(TEXT("the far one was demoted"), Rig.Tier(B), Near);

	// Still over the cap for a long stretch (well past the 30 frame dwell, so the stepper pulls B back to Combat more than once):
	// B stays Near and no tier change is queued at all
	const int64 ChangesBefore = Rig.Minions->GetLODChangeCount();
	for (int32 Index = 0; Index < 120; ++Index)
	{
		Rig.Tick(1);
		TestEqual(TEXT("demoted slot reads Near after every frame"), Rig.Tier(B), Near);
	}
	TestEqual(TEXT("no second LOD change while it stays over the cap"), Rig.Minions->GetLODChangeCount(), ChangesBefore);
	TestEqual(TEXT("the near one stays Combat"), Rig.Tier(A), Combat);

	// Lifting the cap lets it back to Combat after its dwell: one change, then quiet
	FCVarOverride Lifted(TEXT("gstn.Minion.MaxActiveT0"), 0);
	Rig.Tick(40);
	TestEqual(TEXT("back to Combat once the cap is gone"), Rig.Tier(B), Combat);
	TestEqual(TEXT("with exactly one change"), Rig.Minions->GetLODChangeCount(), ChangesBefore + 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBudgetCapTieBreakTest, "GSTNMinion.Runtime.Budget.EqualDistanceBreaksBySlot", GSTN_MINION_BUDGET_TEST_FLAGS)
bool FGSTN_MinionBudgetCapTieBreakTest::RunTest(const FString& Parameters)
{
	FCVarOverride Cap(TEXT("gstn.Minion.MaxActiveT0"), 1);
	FBudgetRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	// Same ground distance: only the slot index tells them apart, and the higher slot is the one demoted
	const FGSTN_MinionHandle Low = Rig.Spawn(300.0);
	const FGSTN_MinionHandle High = Rig.Spawn(-300.0);
	Rig.Tick(6);
	TestTrue(TEXT("the lower slot index is the lower slot"), Low.Index < High.Index);
	TestEqual(TEXT("the lower slot keeps Combat"), Rig.Tier(Low), Combat);
	TestEqual(TEXT("the higher slot is demoted"), Rig.Tier(High), Near);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
