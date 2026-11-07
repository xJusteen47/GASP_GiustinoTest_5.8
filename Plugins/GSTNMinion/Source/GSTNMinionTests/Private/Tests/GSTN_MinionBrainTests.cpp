// Automation tests of the minion brain in a world (M8 G8): the per-tier budget in a real store, the native chase and swing writing intents,
// the StateTree test tree writing the same intents (only when the editor module built it), a stopped brain going stale, a wake on a hit or
// a launch, and the spike 6 timing (40 minions at T0 for 120 frames, both modes). They drive TickBatch and TickBrains directly in a
// transient game world; no content.

#include "Brain/GSTN_MinionBrainSubsystem.h"
#include "Brain/GSTN_MinionStateTree.h"
#include "Data/GSTN_MinionDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GSTNMinionCoreModule.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Simulation/GSTN_HitInbox.h"
#include "Spawn/GSTN_MinionSpawner.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_MINION_BRAIN_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** A transient game world with an activated minion subsystem, every spawn drained at once, no gravity, and the brain subsystem */
	struct FBrainRig
	{
		FBrainRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNMinionBrainTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());

			Minions = World->GetSubsystem<UGSTN_MinionCombatSubsystem>();
			Brain = World->GetSubsystem<UGSTN_MinionBrainSubsystem>();
			if (Minions)
			{
				Minions->ActivateForTests();
				Minions->SetForcedLODTier(static_cast<int32>(EGSTN_MinionLODTier::Combat));
				Minions->SetGravitySource([](const FVector&) { return FVector::ZeroVector; });
			}
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("gstn.Minion.SpawnsPerFrame")))
			{
				PreviousSpawns = Variable->GetInt();
				Variable->Set(256, ECVF_SetByCode);
			}
		}

		~FBrainRig()
		{
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("gstn.Minion.SpawnsPerFrame")))
			{
				Variable->Set(PreviousSpawns, ECVF_SetByCode);
			}
			if (Brain)
			{
				Brain->SetTargetProvider(nullptr);
			}
			if (Minions)
			{
				Minions->SetForcedLODTier(INDEX_NONE);
				Minions->SetGravitySource(nullptr);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		FGSTN_MinionHandle Spawn(UGSTN_MinionDefinition* Definition, const FVector& Location)
		{
			FGSTN_MinionSpawnParams Params;
			Params.bPlaySpawnTimeline = false;
			return Minions->EnqueueSpawn(Definition, FTransform(Location), Params);
		}

		/** One engine frame: the fixed batch of the next frame, then the brain pass */
		void Step()
		{
			++Frame;
			Minions->TickBatch(Frame);
			Brain->TickBrains(1.f / 60.f);
		}

		void Steps(int32 Count)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Step();
			}
		}

		UWorld* World = nullptr;
		UGSTN_MinionCombatSubsystem* Minions = nullptr;
		UGSTN_MinionBrainSubsystem* Brain = nullptr;
		int32 Frame = 0;
		int32 PreviousSpawns = 4;
	};

	TStrongObjectPtr<UGSTN_MinionDefinition> MakeBrainTestDefinition()
	{
		TStrongObjectPtr<UGSTN_MinionDefinition> Definition(NewObject<UGSTN_MinionDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
		Definition->Stats.MaxHealth = 40.f;
		return Definition;
	}

	/** Scoped console variable, restored on exit */
	struct FBrainScopedCVar
	{
		FBrainScopedCVar(const TCHAR* Name, int32 Value)
		{
			Variable = IConsoleManager::Get().FindConsoleVariable(Name);
			if (Variable)
			{
				Previous = Variable->GetInt();
				Variable->Set(Value, ECVF_SetByCode);
			}
		}

		~FBrainScopedCVar()
		{
			if (Variable)
			{
				Variable->Set(Previous, ECVF_SetByCode);
			}
		}

		IConsoleVariable* Variable = nullptr;
		int32 Previous = 0;
	};

	/** The test tree, or null when the editor module is not loaded (a game or commandlet context) */
	UObject* GetBrainTestTree()
	{
		return GSTNMinion::GetTestBrainTreeFactory().IsBound() ? GSTNMinion::GetTestBrainTreeFactory().Execute() : nullptr;
	}

	/** Scripted target: Offset cm along +X from the position of one minion, re-read on every pass so it follows the minion's drift */
	void SetBrainRelativeTarget(FBrainRig& Rig, const FGSTN_MinionHandle& Anchor, const double& Offset)
	{
		Rig.Brain->SetTargetProvider([&Rig, Anchor, &Offset](TArray<FGSTN_MinionBrainTarget>& Out)
		{
			const FGSTN_MinionStore& Store = Rig.Minions->GetStore();
			if (Store.IsValid(Anchor))
			{
				FGSTN_MinionBrainTarget& Target = Out.AddDefaulted_GetRef();
				Target.Id = 0x1000;
				Target.Position = FVector(Store.Position[Anchor.Index]) + FVector(Offset, 0.0, 0.0);
			}
		});
	}

	struct FBrainIntentSample
	{
		double MoveX = 0.0;
		float Speed = 0.f;
		bool bFace = false;
		FString State;
	};

	/** Runs the scripted target distances (far, close, between, far again) and samples the committed intent once settled */
	bool RunBrainScript(bool bNative, UObject* Tree, TArray<FBrainIntentSample>& OutSamples, FString& OutError)
	{
		FBrainScopedCVar Native(TEXT("gstn.Minion.Brain.Native"), bNative ? 1 : 0);
		FBrainRig Rig;
		if (!Rig.Minions || !Rig.Brain)
		{
			OutError = TEXT("no subsystems");
			return false;
		}
		TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeBrainTestDefinition();
		const FGSTN_MinionHandle Handle = Rig.Spawn(Definition.Get(), FVector::ZeroVector);
		Rig.Step();
		if (!Rig.Brain->StartBrain(Handle, bNative ? nullptr : Tree))
		{
			OutError = TEXT("StartBrain failed");
			return false;
		}
		double Offset = 1000.0;
		SetBrainRelativeTarget(Rig, Handle, Offset);
		for (const double Distance : { 1000.0, 190.0, 260.0, 600.0 })
		{
			Offset = Distance;
			// The first pass at a new distance may be the transition tick (the tree and the native brain agree after it)
			Rig.Steps(4);
			const FGSTN_MinionIntent& Intent = Rig.Minions->GetStore().Intent[Handle.Index];
			FBrainIntentSample& Sample = OutSamples.AddDefaulted_GetRef();
			Sample.MoveX = Intent.MoveDir.X;
			Sample.Speed = Intent.DesiredSpeed;
			Sample.bFace = Intent.bFaceRequested;
			Sample.State = Rig.Brain->GetSlotStateName(Handle.Index);
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBrainBudgetWorldTest, "GSTNMinion.Runtime.Brain.BudgetSpread", GSTN_MINION_BRAIN_TEST_FLAGS)
bool FGSTN_MinionBrainBudgetWorldTest::RunTest(const FString& Parameters)
{
	FBrainScopedCVar Native(TEXT("gstn.Minion.Brain.Native"), 1);
	FBrainRig Rig;
	if (!TestNotNull(TEXT("minion and brain subsystems exist"), Rig.Brain) || !TestNotNull(TEXT("minion subsystem"), Rig.Minions))
	{
		return false;
	}
	constexpr int32 NumMinions = 32;
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeBrainTestDefinition();
	TArray<FGSTN_MinionHandle> Handles;
	for (int32 Index = 0; Index < NumMinions; ++Index)
	{
		Handles.Add(Rig.Spawn(Definition.Get(), FVector(Index * 300.0, 0.0, 0.0)));
	}
	Rig.Step();
	for (const FGSTN_MinionHandle& Handle : Handles)
	{
		Rig.Brain->StartBrain(Handle, nullptr);
	}
	// A target nobody reaches in the window (the minions only need something to think about)
	Rig.Brain->SetTargetProvider([](TArray<FGSTN_MinionBrainTarget>& Out)
	{
		FGSTN_MinionBrainTarget& Target = Out.AddDefaulted_GetRef();
		Target.Id = 0x1000;
		Target.Position = FVector(1.0e6, 0.0, 0.0);
	});

	struct FPhase { EGSTN_MinionLODTier Tier; int32 ExpectedPerSlot; int32 ExpectedPerPass; const TCHAR* Name; };
	const FPhase Phases[] = {
		{ EGSTN_MinionLODTier::Combat, 32, NumMinions, TEXT("T0") },
		{ EGSTN_MinionLODTier::Near, 8, NumMinions / 4, TEXT("T1") },
		{ EGSTN_MinionLODTier::Far, 2, NumMinions / 16, TEXT("T2") },
		{ EGSTN_MinionLODTier::Dormant, 0, 0, TEXT("T3") },
	};
	for (const FPhase& Phase : Phases)
	{
		Rig.Minions->SetForcedLODTier(static_cast<int32>(Phase.Tier));
		// The LOD stage only visits a slot on its schedule (up to 16 frames apart for Dormant): settle first
		Rig.Steps(20);
		bool bAllInTier = true;
		for (const FGSTN_MinionHandle& Handle : Handles)
		{
			bAllInTier &= Rig.Minions->GetStore().LODTier[Handle.Index] == static_cast<uint8>(Phase.Tier);
		}
		TestTrue(FString::Printf(TEXT("%s: every minion reached the tier"), Phase.Name), bAllInTier);

		TArray<int32> Before;
		for (const FGSTN_MinionHandle& Handle : Handles)
		{
			Before.Add(Rig.Brain->GetSlotTickCount(Handle.Index));
		}
		int32 MinPerPass = INT32_MAX;
		int32 MaxPerPass = 0;
		for (int32 Pass = 0; Pass < 32; ++Pass)
		{
			const int64 TicksBefore = Rig.Brain->GetStats().TotalTicks;
			Rig.Step();
			const int32 PerPass = static_cast<int32>(Rig.Brain->GetStats().TotalTicks - TicksBefore);
			MinPerPass = FMath::Min(MinPerPass, PerPass);
			MaxPerPass = FMath::Max(MaxPerPass, PerPass);
		}
		bool bEverySlotExact = true;
		for (int32 Index = 0; Index < NumMinions; ++Index)
		{
			bEverySlotExact &= (Rig.Brain->GetSlotTickCount(Handles[Index].Index) - Before[Index]) == Phase.ExpectedPerSlot;
		}
		TestTrue(FString::Printf(TEXT("%s: every slot ticked %d times in 32 passes"), Phase.Name, Phase.ExpectedPerSlot), bEverySlotExact);
		TestEqual(FString::Printf(TEXT("%s: the load per pass is flat (min)"), Phase.Name), MinPerPass, Phase.ExpectedPerPass);
		TestEqual(FString::Printf(TEXT("%s: the load per pass is flat (max)"), Phase.Name), MaxPerPass, Phase.ExpectedPerPass);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBrainNativeWorldTest, "GSTNMinion.Runtime.Brain.NativeChaseSwingIntents", GSTN_MINION_BRAIN_TEST_FLAGS)
bool FGSTN_MinionBrainNativeWorldTest::RunTest(const FString& Parameters)
{
	TArray<FBrainIntentSample> Samples;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("the native script ran (%s)"), *Error), RunBrainScript(true, nullptr, Samples, Error)) || Samples.Num() != 4)
	{
		return false;
	}
	TestTrue(TEXT("far target: chasing at speed along +X"), Samples[0].Speed > 0.f && Samples[0].MoveX > 0.9 && Samples[0].State == TEXT("Chase"));
	TestTrue(TEXT("close target: swinging, facing, standing"), Samples[1].Speed == 0.f && Samples[1].bFace && Samples[1].State == TEXT("Swing"));
	TestTrue(TEXT("between the ranges it is still swinging"), Samples[2].State == TEXT("Swing"));
	TestTrue(TEXT("far again: back to chasing"), Samples[3].Speed > 0.f && Samples[3].State == TEXT("Chase"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBrainTreeMatchesNativeTest, "GSTNMinion.Runtime.Brain.StateTreeMatchesNative", GSTN_MINION_BRAIN_TEST_FLAGS)
bool FGSTN_MinionBrainTreeMatchesNativeTest::RunTest(const FString& Parameters)
{
	UObject* Tree = GetBrainTestTree();
	if (!Tree)
	{
		// A context without the editor module cannot build the tree; the native tests above still cover the behaviour
		AddInfo(TEXT("The test StateTree is not built in this context (GSTNMinionEditor not loaded); comparison skipped"));
		return true;
	}
	TArray<FBrainIntentSample> TreeSamples;
	TArray<FBrainIntentSample> NativeSamples;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("the tree script ran (%s)"), *Error), RunBrainScript(false, Tree, TreeSamples, Error)) || TreeSamples.Num() != 4)
	{
		return false;
	}
	if (!TestTrue(FString::Printf(TEXT("the native script ran (%s)"), *Error), RunBrainScript(true, nullptr, NativeSamples, Error)) || NativeSamples.Num() != 4)
	{
		return false;
	}
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const FBrainIntentSample& A = TreeSamples[Index];
		const FBrainIntentSample& B = NativeSamples[Index];
		TestTrue(FString::Printf(TEXT("step %d: same move (%.2f/%.0f vs %.2f/%.0f)"), Index, A.MoveX, A.Speed, B.MoveX, B.Speed), FMath::IsNearlyEqual(A.MoveX, B.MoveX, 0.05) && FMath::IsNearlyEqual(A.Speed, B.Speed, 1.f));
		TestTrue(FString::Printf(TEXT("step %d: same facing request"), Index), A.bFace == B.bFace);
		// The tree reports a path of state names ("Root > Chase"); the native brain the bare phase
		TestTrue(FString::Printf(TEXT("step %d: same state ('%s' vs '%s')"), Index, *A.State, *B.State), A.State.Contains(B.State));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBrainStaleWorldTest, "GSTNMinion.Runtime.Brain.IntentStaleWhenBrainStops", GSTN_MINION_BRAIN_TEST_FLAGS)
bool FGSTN_MinionBrainStaleWorldTest::RunTest(const FString& Parameters)
{
	FBrainScopedCVar Native(TEXT("gstn.Minion.Brain.Native"), 1);
	FBrainRig Rig;
	if (!TestNotNull(TEXT("subsystems exist"), Rig.Brain) || !TestNotNull(TEXT("minion subsystem"), Rig.Minions))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeBrainTestDefinition();
	const FGSTN_MinionHandle Handle = Rig.Spawn(Definition.Get(), FVector::ZeroVector);
	Rig.Step();
	double Offset = 1000.0;
	SetBrainRelativeTarget(Rig, Handle, Offset);
	Rig.Brain->StartBrain(Handle, nullptr);
	Rig.Steps(3);
	const FGSTN_MinionStore& Store = Rig.Minions->GetStore();
	TestFalse(TEXT("the intent is fresh while the brain runs"), Store.Intent[Handle.Index].IsStale(Rig.Minions->GetLastBatchFrame()));
	TestTrue(TEXT("the brain asked for a move"), Store.Intent[Handle.Index].DesiredSpeed > 0.f);

	TestTrue(TEXT("the brain stops"), Rig.Brain->StopBrain(Handle));
	TestFalse(TEXT("a stopped brain is gone"), Rig.Brain->HasBrain(Handle));
	Rig.Steps(GSTNMinion::IntentStaleFrames + 5);
	TestTrue(TEXT("with the brain stopped the intent goes stale"), Store.Intent[Handle.Index].IsStale(Rig.Minions->GetLastBatchFrame()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBrainWakeTest, "GSTNMinion.Runtime.Brain.WakeOnHitAndLaunch", GSTN_MINION_BRAIN_TEST_FLAGS)
bool FGSTN_MinionBrainWakeTest::RunTest(const FString& Parameters)
{
	FBrainScopedCVar Native(TEXT("gstn.Minion.Brain.Native"), 1);
	FBrainRig Rig;
	if (!TestNotNull(TEXT("subsystems exist"), Rig.Brain) || !TestNotNull(TEXT("minion subsystem"), Rig.Minions))
	{
		return false;
	}
	TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeBrainTestDefinition();
	const FGSTN_MinionHandle Hit = Rig.Spawn(Definition.Get(), FVector(0.0, 0.0, 0.0));
	const FGSTN_MinionHandle Launch = Rig.Spawn(Definition.Get(), FVector(500.0, 0.0, 0.0));
	Rig.Step();
	Rig.Brain->StartBrain(Hit, nullptr);
	Rig.Brain->StartBrain(Launch, nullptr);
	Rig.Brain->SetTargetProvider([](TArray<FGSTN_MinionBrainTarget>& Out)
	{
		FGSTN_MinionBrainTarget& Target = Out.AddDefaulted_GetRef();
		Target.Id = 0x1000;
		Target.Position = FVector(1.0e6, 0.0, 0.0);
	});

	// Both minions go Dormant and stay there on the distance rule (no player = infinitely far)
	Rig.Minions->SetForcedLODTier(static_cast<int32>(EGSTN_MinionLODTier::Dormant));
	Rig.Steps(20);
	Rig.Minions->SetForcedLODTier(INDEX_NONE);
	Rig.Steps(2);
	const FGSTN_MinionStore& Store = Rig.Minions->GetStore();
	TestEqual(TEXT("the hit minion is Dormant"), static_cast<int32>(Store.LODTier[Hit.Index]), static_cast<int32>(EGSTN_MinionLODTier::Dormant));
	TestEqual(TEXT("the launched minion is Dormant"), static_cast<int32>(Store.LODTier[Launch.Index]), static_cast<int32>(EGSTN_MinionLODTier::Dormant));
	const int32 HitTicks = Rig.Brain->GetSlotTickCount(Hit.Index);
	const int32 LaunchTicks = Rig.Brain->GetSlotTickCount(Launch.Index);
	Rig.Steps(20);
	TestEqual(TEXT("a dormant brain does not tick"), Rig.Brain->GetSlotTickCount(Hit.Index), HitTicks);

	// Wake 1: a hit posted to the inbox, as a fighter's hit would arrive
	FGSTN_HitInboxEntry Entry;
	Entry.Frame = Rig.Frame;
	Entry.AttackerId = 0x3FFFFFF0;
	Entry.AttackerTeam = 255;
	Entry.Relationship = 255;
	Entry.Hit.Damage = 1.f;
	Entry.HitLocation = FVector(Store.Position[Hit.Index]);
	TestTrue(TEXT("the hit is accepted"), Rig.Minions->PostToEntityInbox(GSTNEntityKinds::MakeMinionEntityId(Hit.Index), Hit.Generation, Entry.ToInboxEntry()));
	const int32 WakesBefore = Rig.Brain->GetStats().Wakes;
	Rig.Step();
	TestEqual(TEXT("the hit puts the minion in Combat at the next batch"), static_cast<int32>(Store.LODTier[Hit.Index]), static_cast<int32>(EGSTN_MinionLODTier::Combat));
	TestEqual(TEXT("its brain ticked in that same pass"), Rig.Brain->GetSlotTickCount(Hit.Index), HitTicks + 1);
	TestEqual(TEXT("one wake counted"), Rig.Brain->GetStats().Wakes, WakesBefore + 1);

	// Wake 2: a launch, which must not wait for the next scheduled LOD step of a dormant slot
	TestTrue(TEXT("the launch is accepted"), Rig.Minions->LaunchMinion(Launch, FVector(0.0, 0.0, 300.0), false));
	TestEqual(TEXT("the launch puts the minion in Combat at once"), static_cast<int32>(Store.LODTier[Launch.Index]), static_cast<int32>(EGSTN_MinionLODTier::Combat));
	Rig.Step();
	TestEqual(TEXT("its brain ticked in the next pass"), Rig.Brain->GetSlotTickCount(Launch.Index), LaunchTicks + 1);
	TestEqual(TEXT("two wakes counted"), Rig.Brain->GetStats().Wakes, WakesBefore + 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBrainSpike6Test, "GSTNMinion.Runtime.Brain.Spike6", GSTN_MINION_BRAIN_TEST_FLAGS)
bool FGSTN_MinionBrainSpike6Test::RunTest(const FString& Parameters)
{
	// 40 minions at forced T0 for 120 frames, once with the native brain and once with the test tree. The test only asserts that it ran;
	// the 0.02 ms per minion tick threshold of the plan is judged from the logged numbers
	UObject* Tree = GetBrainTestTree();
	for (const bool bNative : { true, false })
	{
		if (!bNative && !Tree)
		{
			AddInfo(TEXT("Spike6 mode=StateTree skipped: the test StateTree is not built in this context"));
			continue;
		}
		FBrainScopedCVar Native(TEXT("gstn.Minion.Brain.Native"), bNative ? 1 : 0);
		FBrainRig Rig;
		if (!TestNotNull(TEXT("subsystems exist"), Rig.Brain) || !TestNotNull(TEXT("minion subsystem"), Rig.Minions))
		{
			return false;
		}
		constexpr int32 NumMinions = 40;
		constexpr int32 NumFrames = 120;
		TStrongObjectPtr<UGSTN_MinionDefinition> Definition = MakeBrainTestDefinition();
		TArray<FGSTN_MinionHandle> Handles;
		for (int32 Index = 0; Index < NumMinions; ++Index)
		{
			Handles.Add(Rig.Spawn(Definition.Get(), FVector((Index % 8) * 120.0, (Index / 8) * 120.0, 0.0)));
		}
		Rig.Step();
		for (const FGSTN_MinionHandle& Handle : Handles)
		{
			Rig.Brain->StartBrain(Handle, bNative ? nullptr : Tree);
		}
		Rig.Brain->SetTargetProvider([](TArray<FGSTN_MinionBrainTarget>& Out)
		{
			FGSTN_MinionBrainTarget& Target = Out.AddDefaulted_GetRef();
			Target.Id = 0x1000;
			Target.Position = FVector(1500.0, 300.0, 0.0);
		});
		Rig.Brain->ResetStats();
		Rig.Steps(NumFrames);

		const FGSTN_MinionBrainStats& Stats = Rig.Brain->GetStats();
		TestEqual(FString::Printf(TEXT("%s: every minion ticked every frame"), bNative ? TEXT("Native") : TEXT("StateTree")), Stats.TotalTicks, static_cast<int64>(NumMinions) * NumFrames);
		const double UsPerMinionTick = Stats.TotalTicks > 0 ? Stats.TotalSeconds * 1.0e6 / static_cast<double>(Stats.TotalTicks) : 0.0;
		const FString Line = FString::Printf(TEXT("Spike6 mode=%s usPerMinionTick=%.3f maxUsPerTick=%.1f ticks=%lld"), bNative ? TEXT("Native") : TEXT("StateTree"), UsPerMinionTick, Stats.MaxTickSeconds * 1.0e6, Stats.TotalTicks);
		UE_LOG(LogGSTNMinion, Log, TEXT("%s"), *Line);
		AddInfo(Line);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
