// Automation tests of waves, corpses and promotion (M8 G9a): the body that stays CorpseSeconds, a spawner playing a two entry wave, and the
// server core of the promotion with its cap. They drive TickBatch (and the spawner's UpdateWave) directly in a transient game world;
// rules, hurtboxes, timelines and the fighter class are built in code (no content).

#include "Data/GSTN_MinionDefinition.h"
#include "Data/GSTN_MinionWaveDefinition.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GenericTeamAgentInterface.h"
#include "GSTN_MinionTestActors.h"
#include "GSTN_MinionTags.h"
#include "HAL/IConsoleManager.h"
#include "Hits/GSTN_HurtboxTypes.h"
#include "Hits/GSTN_KnockbackModel.h"
#include "Misc/AutomationTest.h"
#include "Net/GSTN_MinionNetCodec.h"
#include "Rules/GSTN_CombatRules.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Runtime/GSTN_MinionPromotedComponent.h"
#include "Simulation/GSTN_HitInbox.h"
#include "Spawn/GSTN_MinionSpawner.h"
#include "Spawn/GSTN_MinionSpawnerActor.h"
#include "Timeline/GSTN_CombatTimeline.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_MINION_WAVE_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace GSTNMinionWaveTestHelpers
{
	/** Keeps a console variable at a value while a test runs */
	struct FScopedIntCVar
	{
		FScopedIntCVar(const TCHAR* Name, int32 Value)
		{
			Variable = IConsoleManager::Get().FindConsoleVariable(Name);
			if (Variable)
			{
				Previous = Variable->GetInt();
				Variable->Set(Value, ECVF_SetByCode);
			}
		}

		~FScopedIntCVar()
		{
			if (Variable)
			{
				Variable->Set(Previous, ECVF_SetByCode);
			}
		}

		IConsoleVariable* Variable = nullptr;
		int32 Previous = 0;
	};

	/** A transient game world with an activated minion subsystem, every slot in Combat and no gravity (minions stay where they spawn) */
	struct FWaveRig
	{
		FWaveRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNMinionWaveTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());

			Minions = World->GetSubsystem<UGSTN_MinionCombatSubsystem>();
			if (Minions)
			{
				Minions->ActivateForTests();
				Minions->SetForcedLODTier(static_cast<int32>(EGSTN_MinionLODTier::Combat));
				Minions->SetGravitySource([](const FVector&) { return FVector::ZeroVector; });
			}
		}

		~FWaveRig()
		{
			if (Minions)
			{
				Minions->SetForcedLODTier(INDEX_NONE);
				Minions->SetGravitySource(nullptr);
				Minions->OnMinionPromoted.Clear();
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

		EGSTN_MinionActionState StateOf(const FGSTN_MinionHandle& Handle) const
		{
			return static_cast<EGSTN_MinionActionState>(Minions->GetStore().ActionState[Handle.Index]);
		}

		/** A hit far above any health from a neutral attacker (no fighter id, no team): the normal damage path, ends in the death timeline */
		bool PostLethalHit(const FGSTN_MinionHandle& Handle, int32 Frame)
		{
			FGSTN_HitInboxEntry Hit;
			Hit.Frame = Frame;
			Hit.AttackerId = 7;
			Hit.AttackerTeam = 255;
			Hit.Relationship = 255;
			Hit.Hit.Damage = 100000.f;
			Hit.Hit.FixedKnockback = 100.f;
			Hit.Hit.HitstopMode = EGSTN_HitstopMode::Override;
			Hit.Hit.HitstopFramesOverride = 0;
			Hit.HitLocation = FVector(Minions->GetStore().Position[Handle.Index]) + FVector(-60.0, 0.0, 90.0);
			Hit.HurtboxIndex = 0;
			return Minions->PostToEntityInbox(GSTNEntityKinds::MakeMinionEntityId(Handle.Index), Handle.Generation, Hit.ToInboxEntry());
		}

		UWorld* World = nullptr;
		UGSTN_MinionCombatSubsystem* Minions = nullptr;
	};

	/** Rules that let a neutral lethal hit through; kept alive by the caller for as long as the rig lives */
	TStrongObjectPtr<UGSTN_CombatRules> MakeRules()
	{
		TStrongObjectPtr<UGSTN_CombatRules> Rules(UGSTN_CombatRules::CreateStandard(GetTransientPackage()));
		Rules->DamageMode = EGSTN_DamageMode::Percent;
		Rules->MaxHealth = 100.f;
		Rules->StaggerThreshold = 0.f;
		Rules->MinStaggerLockFrames = 10;
		Rules->DefaultWeightCurve.Samples.Reset();
		Rules->MeterGainPerDamage = 1.f;
		if (UGSTN_KnockbackModel* Model = Rules->KnockbackModel.Get())
		{
			// Thresholds above the fixed knockback of the test hit: the body is staggered in place, never pushed or launched
			Model->Tuning.PushBackThreshold = 300.f;
			Model->Tuning.LaunchThreshold = 1000.f;
			Model->Tuning.TumbleThreshold = 100000.f;
			Model->Tuning.MaxHitstop = 20;
			Model->Tuning.WeightReference = 100.f;
		}
		return Rules;
	}

	TStrongObjectPtr<UGSTN_HurtboxSetDefinition> MakeHurtboxes()
	{
		TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Set(NewObject<UGSTN_HurtboxSetDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
		FGSTN_Hurtbox& Body = Set->Hurtboxes.AddDefaulted_GetRef();
		Body.Name = TEXT("Body");
		Body.Shape.Type = EGSTN_ShapeType::Capsule;
		Body.Shape.Extent = FVector(40.0, 40.0, 90.0);
		Body.Shape.Local = FTransform(FVector(0.0, 0.0, 90.0));
		return Set;
	}

	TStrongObjectPtr<UGSTN_CombatTimeline> MakeTimeline(int32 TotalFrames)
	{
		TStrongObjectPtr<UGSTN_CombatTimeline> Timeline(NewObject<UGSTN_CombatTimeline>(GetTransientPackage(), NAME_None, RF_Transient));
		Timeline->Data.TotalFrames = TotalFrames;
		Timeline->Data.FirstActionableFrame = TotalFrames;
		return Timeline;
	}

	/** Everything a rig needs to kill a minion and watch the body: rules, hurtboxes, a Death timeline, the definition. Declared before the rig */
	struct FDeathKit
	{
		explicit FDeathKit(float CorpseSeconds, int32 DeathFrames = 5)
			: Rules(MakeRules())
			, Hurtboxes(MakeHurtboxes())
			, Death(MakeTimeline(DeathFrames))
			, Definition(NewObject<UGSTN_MinionDefinition>(GetTransientPackage(), NAME_None, RF_Transient))
		{
			Definition->Stats.MaxHealth = 40.f;
			Definition->TeamId = 1;
			Definition->HurtboxSet = Hurtboxes.Get();
			Definition->Reactions.Death = Death.Get();
			Definition->CorpseSeconds = CorpseSeconds;
		}

		TStrongObjectPtr<UGSTN_CombatRules> Rules;
		TStrongObjectPtr<UGSTN_HurtboxSetDefinition> Hurtboxes;
		TStrongObjectPtr<UGSTN_CombatTimeline> Death;
		TStrongObjectPtr<UGSTN_MinionDefinition> Definition;
	};

	/** What the corpse test needs from one death: the frame the body went Dead (none when it was freed on that same frame) and the frame it was freed */
	struct FDeathRun
	{
		int32 DeadFrame = INDEX_NONE;
		int32 FreedFrame = INDEX_NONE;
		int32 CorpseEndFrame = INDEX_NONE;
		FGSTN_MinionHandle Handle;
		uint16 SlotGenerationAfter = 0;
	};
}
// Named, not anonymous: unity builds merge this file with the hit tests, which have helpers of the same names
using namespace GSTNMinionWaveTestHelpers;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionCorpseTest, "GSTNMinion.Runtime.Corpse.StaysThenFrees", GSTN_MINION_WAVE_TEST_FLAGS)
bool FGSTN_MinionCorpseTest::RunTest(const FString& Parameters)
{
	// 0.16 s is 10 fixed frames at 62.5 Hz
	constexpr int32 ExpectedCorpseFrames = 10;

	const auto RunDeath = [this](float CorpseSeconds, bool bProbeTheBody) -> FDeathRun
	{
		FDeathKit Kit(CorpseSeconds);
		FWaveRig Rig;
		FDeathRun Run;
		if (!TestNotNull(TEXT("the minion subsystem exists"), Rig.Minions))
		{
			return Run;
		}
		Rig.Minions->SetCombatRules(Kit.Rules.Get());

		Run.Handle = Rig.Spawn(Kit.Definition.Get(), FVector(100.0, 0.0, 0.0));
		Rig.Minions->TickBatch(1);
		TestTrue(TEXT("the lethal hit is accepted"), Rig.PostLethalHit(Run.Handle, 1));

		FVector3f DeadPosition = FVector3f::ZeroVector;
		uint8 HitCounterAtDeath = 0;
		for (int32 Frame = 2; Frame < 300; ++Frame)
		{
			Rig.Minions->TickBatch(Frame);
			if (!Rig.Minions->IsValidHandle(Run.Handle))
			{
				Run.FreedFrame = Frame;
				Run.SlotGenerationAfter = Rig.Minions->GetStore().Generation[Run.Handle.Index];
				break;
			}
			if (Rig.StateOf(Run.Handle) != EGSTN_MinionActionState::Dead)
			{
				continue;
			}
			const FGSTN_MinionStore& Store = Rig.Minions->GetStore();
			if (Run.DeadFrame == INDEX_NONE)
			{
				Run.DeadFrame = Frame;
				Run.CorpseEndFrame = Rig.Minions->GetCorpseEndFrame(Run.Handle.Index);
				DeadPosition = Store.Position[Run.Handle.Index];
				HitCounterAtDeath = Store.HitCounter[Run.Handle.Index];
			}
			else if (bProbeTheBody)
			{
				TestTrue(TEXT("a corpse is not moved"), Store.Position[Run.Handle.Index].Equals(DeadPosition, 0.01f));
				TestTrue(TEXT("a corpse holds no token"), Rig.Minions->GetAttackTokens().NumHolders(1) == 0);
				if (Frame == Run.DeadFrame + 3)
				{
					// A hit on a corpse is dropped at the drain; the body stays as it is
					Rig.PostLethalHit(Run.Handle, Frame);
				}
				if (Frame == Run.DeadFrame + 5)
				{
					TestEqual(TEXT("a corpse takes no hit"), static_cast<int32>(Store.HitCounter[Run.Handle.Index]), static_cast<int32>(HitCounterAtDeath));
					TestTrue(TEXT("and stays dead"), Rig.StateOf(Run.Handle) == EGSTN_MinionActionState::Dead);
				}
			}
		}
		return Run;
	};

	const FDeathRun Body = RunDeath(ExpectedCorpseFrames / 62.5f, true);
	if (!TestTrue(TEXT("the body went Dead"), Body.DeadFrame != INDEX_NONE))
	{
		return false;
	}
	TestEqual(TEXT("the corpse end frame is stamped when the death ends"), Body.CorpseEndFrame, Body.DeadFrame + ExpectedCorpseFrames);
	TestEqual(TEXT("the slot is freed once the corpse time is up"), Body.FreedFrame, Body.DeadFrame + ExpectedCorpseFrames);
	TestTrue(TEXT("the handle is stale after"), Body.Handle.Generation != Body.SlotGenerationAfter);

	// 0 s: freed on the frame the death ends, as before G9a (the body is never observed Dead)
	const FDeathRun Instant = RunDeath(0.f, false);
	TestEqual(TEXT("with 0 s the body is never seen Dead"), Instant.DeadFrame, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("and the slot is freed on the frame the death ends"), Instant.FreedFrame, Body.DeadFrame);
	TestTrue(TEXT("generation bumped"), Instant.Handle.Generation != Instant.SlotGenerationAfter);

	// The slot is reusable: the next spawn takes it with a new generation
	{
		FDeathKit Kit(0.f);
		FWaveRig Rig;
		if (!TestNotNull(TEXT("the minion subsystem exists"), Rig.Minions))
		{
			return false;
		}
		const FGSTN_MinionHandle First = Rig.Spawn(Kit.Definition.Get());
		Rig.Minions->TickBatch(1);
		Rig.Minions->Despawn(First);
		const FGSTN_MinionHandle Second = Rig.Spawn(Kit.Definition.Get());
		TestEqual(TEXT("a despawn frees at once and the slot is reused"), static_cast<int32>(Second.Index), static_cast<int32>(First.Index));
		TestNotEqual(TEXT("with a new generation"), static_cast<int32>(Second.Generation), static_cast<int32>(First.Generation));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionWaveSpawnerTest, "GSTNMinion.Runtime.Wave.TwoEntriesAndCleared", GSTN_MINION_WAVE_TEST_FLAGS)
bool FGSTN_MinionWaveSpawnerTest::RunTest(const FString& Parameters)
{
	FScopedIntCVar Budget(TEXT("gstn.Minion.SpawnsPerFrame"), 4);
	FDeathKit Kit(0.04f, 3);
	TStrongObjectPtr<UGSTN_MinionWaveDefinition> Wave(NewObject<UGSTN_MinionWaveDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
	{
		FGSTN_MinionWaveEntry& First = Wave->Entries.AddDefaulted_GetRef();
		First.Definition = Kit.Definition.Get();
		First.Count = 3;
		First.DelayFrames = 0;
		FGSTN_MinionWaveEntry& Second = Wave->Entries.AddDefaulted_GetRef();
		Second.Definition = Kit.Definition.Get();
		Second.Count = 2;
		Second.DelayFrames = 10;
	}

	FWaveRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists"), Rig.Minions))
	{
		return false;
	}
	Rig.Minions->SetCombatRules(Kit.Rules.Get());

	AGSTN_MinionSpawnerActor* Spawner = Rig.World->SpawnActor<AGSTN_MinionSpawnerActor>();
	if (!TestNotNull(TEXT("the spawner exists"), Spawner))
	{
		return false;
	}
	Spawner->Wave = Wave.Get();
	Spawner->bUseTrigger = false;

	int32 ClearedNative = 0;
	int32 ClearedFrame = INDEX_NONE;
	Spawner->OnWaveClearedNative.AddLambda([&ClearedNative, &ClearedFrame](int32 Frame)
	{
		++ClearedNative;
		ClearedFrame = Frame;
	});

	int32 Frame = 1;
	Rig.Minions->TickBatch(Frame);
	TestTrue(TEXT("the wave starts"), Spawner->StartWave());
	TestFalse(TEXT("and does not start twice"), Spawner->StartWave());
	TestTrue(TEXT("a wave with nothing to load is preloaded at once"), Spawner->IsPreloaded());
	Spawner->UpdateWave();
	TestEqual(TEXT("entry 1 spawns its 3 minions at the start frame"), Rig.Minions->GetStore().Num(), 3);
	TestEqual(TEXT("all alive"), Spawner->GetAliveCount(), 3);

	// Entry 2 is due at start frame (1) + 10 = 11: not at 10, yes at 11
	for (Frame = 2; Frame <= 10; ++Frame)
	{
		Rig.Minions->TickBatch(Frame);
		Spawner->UpdateWave();
	}
	TestEqual(TEXT("entry 2 has not spawned at frame 10"), Rig.Minions->GetStore().Num(), 3);
	Rig.Minions->TickBatch(Frame);
	Spawner->UpdateWave();
	TestEqual(TEXT("entry 2 spawns its 2 minions at frame 11"), Rig.Minions->GetStore().Num(), 5);
	TestEqual(TEXT("5 alive"), Spawner->GetAliveCount(), 5);
	TestTrue(TEXT("the wave still runs"), Spawner->IsWaveRunning());

	// Let the queue activate them all, then kill everything through the damage path
	for (++Frame; Frame <= 14; ++Frame)
	{
		Rig.Minions->TickBatch(Frame);
		Spawner->UpdateWave();
	}
	TestEqual(TEXT("nothing cleared yet"), ClearedNative, 0);
	TArray<FGSTN_MinionHandle> Alive;
	Spawner->GetAliveHandles(Alive);
	TestEqual(TEXT("five handles to kill"), Alive.Num(), 5);
	for (const FGSTN_MinionHandle& Handle : Alive)
	{
		Rig.PostLethalHit(Handle, 14);
	}
	Rig.Minions->TickBatch(15);
	TestTrue(TEXT("dying counts as not alive"), UGSTN_MinionSpawner::IsMinionAlive(Rig.World, Alive[0]) == false);
	TestTrue(TEXT("but the minion is still a valid handle (a body)"), UGSTN_MinionSpawner::IsMinionValid(Rig.World, Alive[0]));
	Spawner->UpdateWave();
	TestEqual(TEXT("no minion alive"), Spawner->GetAliveCount(), 0);
	TestEqual(TEXT("OnWaveCleared fired once, as soon as the last one is dying"), ClearedNative, 1);
	TestEqual(TEXT("on the batch frame of the check"), ClearedFrame, 15);
	TestFalse(TEXT("the wave stopped"), Spawner->IsWaveRunning());

	// Bodies wait out their corpse time and are freed; the spawner stays silent
	for (Frame = 16; Frame < 60; ++Frame)
	{
		Rig.Minions->TickBatch(Frame);
		Spawner->UpdateWave();
	}
	TestEqual(TEXT("every body is freed after its corpse time"), Rig.Minions->GetStore().Num(), 0);
	TestEqual(TEXT("and the clear never fired again"), ClearedNative, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionPromoteTest, "GSTNMinion.Runtime.Promote.SpawnsFighterAndCaps", GSTN_MINION_WAVE_TEST_FLAGS)
bool FGSTN_MinionPromoteTest::RunTest(const FString& Parameters)
{
	FScopedIntCVar Cap(TEXT("gstn.Minion.MaxFightersFromMinions"), 4);
	FScopedIntCVar Hold(TEXT("gstn.Minion.PromoteHoldFrames"), 2);
	FDeathKit Kit(2.f, 40);
	Kit.Definition->PromotionFighterClass = TSoftClassPtr<AActor>(AGSTN_MinionTestFighter::StaticClass());

	FWaveRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists"), Rig.Minions))
	{
		return false;
	}
	Rig.Minions->SetCombatRules(Kit.Rules.Get());

	TArray<FGSTN_MinionHandle> Minions;
	for (int32 Index = 0; Index < 7; ++Index)
	{
		Minions.Add(Rig.Spawn(Kit.Definition.Get(), FVector(200.0 * Index, 0.0, 0.0)));
	}
	// Seven spawns take two frames of the queue (gstn.Minion.SpawnsPerFrame 4): every minion is active before anything is hit or promoted
	Rig.Minions->TickBatch(1);
	Rig.Minions->TickBatch(2);

	int32 PromotedEvents = 0;
	AActor* LastEventFighter = nullptr;
	Rig.Minions->OnMinionPromoted.AddLambda([&PromotedEvents, &LastEventFighter](const FGSTN_MinionHandle&, AActor* Fighter)
	{
		++PromotedEvents;
		LastEventFighter = Fighter;
	});
	const FGSTN_MinionPromotionParams Keep = FGSTN_MinionPromotionParams();
	FGSTN_MinionPromotionParams Reset;
	Reset.bKeepHealthFraction = false;

	// A dying minion cannot be promoted, a stale or null handle neither
	Rig.PostLethalHit(Minions[6], 3);
	Rig.Minions->TickBatch(3);
	Rig.Minions->TickBatch(4);
	TestTrue(TEXT("the victim is dying"), Rig.StateOf(Minions[6]) == EGSTN_MinionActionState::Dying);
	TestNull(TEXT("a dying minion is refused"), UGSTN_MinionSpawner::Promote(Rig.World, Minions[6], Keep));
	TestTrue(TEXT("and stays where it is"), Rig.Minions->IsValidHandle(Minions[6]));
	TestNull(TEXT("the null handle is refused"), UGSTN_MinionSpawner::Promote(Rig.World, FGSTN_MinionHandle(), Keep));
	TestEqual(TEXT("no event for a refusal"), PromotedEvents, 0);

	// The first promotion: class, handle, component, team, health fraction
	const_cast<FGSTN_MinionStore&>(Rig.Minions->GetStore()).Health[Minions[0].Index] = 10.f;
	const FVector Feet(Rig.Minions->GetStore().Position[Minions[0].Index]);
	AActor* First = UGSTN_MinionSpawner::Promote(Rig.World, Minions[0], Keep);
	if (!TestNotNull(TEXT("the first promotion spawns the fighter"), First))
	{
		return false;
	}
	TestTrue(TEXT("of the definition's class"), First->GetClass() == AGSTN_MinionTestFighter::StaticClass());
	// G9b: the minion holds as a Promoting body (not alive, flag on the wire) for gstn.Minion.PromoteHoldFrames, then it is freed
	TestFalse(TEXT("the promoted minion is no longer alive"), Rig.Minions->IsAlive(Minions[0]));
	TestTrue(TEXT("it holds as a body during the hold"), Rig.Minions->IsValidHandle(Minions[0]));
	TestTrue(TEXT("with the Promoting flag"), (Rig.Minions->GetStore().Flags[Minions[0].Index] & GSTNMinion::NetFlags::Promoting) != 0);
	TestTrue(TEXT("the fighter stands at the minion (lifted by the capsule half height)"), FMath::IsNearlyEqual(First->GetActorLocation().X, Feet.X, 1.0));
	const UGSTN_MinionPromotedComponent* Marker = First->FindComponentByClass<UGSTN_MinionPromotedComponent>();
	if (TestNotNull(TEXT("the fighter carries the promoted component"), Marker))
	{
		TestEqual(TEXT("with the health fraction of the minion"), Marker->HealthFraction, 0.25f);
		TestEqual(TEXT("and the team of the type"), static_cast<int32>(Marker->Team), 1);
		TestTrue(TEXT("replicated"), Marker->GetIsReplicated());
	}
	if (const IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(First))
	{
		TestEqual(TEXT("the fighter was given the team"), static_cast<int32>(TeamAgent->GetGenericTeamId().GetId()), 1);
	}
	TestEqual(TEXT("the promoted event fired once"), PromotedEvents, 1);
	TestTrue(TEXT("with the fighter"), LastEventFighter == First);
	TestNull(TEXT("a held body is refused"), UGSTN_MinionSpawner::Promote(Rig.World, Minions[0], Keep));
	Rig.Minions->TickBatch(5);
	TestTrue(TEXT("still held one frame before the hold ends"), Rig.Minions->IsValidHandle(Minions[0]));
	Rig.Minions->TickBatch(6);
	TestFalse(TEXT("the minion handle is invalid after the hold"), Rig.Minions->IsValidHandle(Minions[0]));
	TestNull(TEXT("a stale handle is refused"), UGSTN_MinionSpawner::Promote(Rig.World, Minions[0], Keep));

	// bKeepHealthFraction false: the fraction is 1
	AActor* Second = UGSTN_MinionSpawner::Promote(Rig.World, Minions[1], Reset);
	if (TestNotNull(TEXT("the second promotion works"), Second))
	{
		const UGSTN_MinionPromotedComponent* Other = Second->FindComponentByClass<UGSTN_MinionPromotedComponent>();
		TestEqual(TEXT("without keeping the health the fraction is 1"), Other ? Other->HealthFraction : -1.f, 1.f);
	}
	TestNotNull(TEXT("the third"), UGSTN_MinionSpawner::Promote(Rig.World, Minions[2], Keep));
	TestNotNull(TEXT("the fourth"), UGSTN_MinionSpawner::Promote(Rig.World, Minions[3], Keep));
	TestEqual(TEXT("four fighters from minions live"), Rig.Minions->GetNumPromotedFighters(), 4);

	// The cap: the fifth is refused and nothing changes
	TestNull(TEXT("the fifth is refused while four fighters live"), UGSTN_MinionSpawner::Promote(Rig.World, Minions[4], Keep));
	TestTrue(TEXT("its minion is untouched"), Rig.Minions->IsAlive(Minions[4]));
	TestEqual(TEXT("and no extra event fired"), PromotedEvents, 4);

	// One fighter goes: the fifth is allowed
	First->Destroy();
	TestEqual(TEXT("three fighters live after a destroy"), Rig.Minions->GetNumPromotedFighters(), 3);
	AActor* Fifth = UGSTN_MinionSpawner::Promote(Rig.World, Minions[4], Keep);
	TestNotNull(TEXT("the fifth is allowed after one fighter is destroyed"), Fifth);
	TestFalse(TEXT("and its minion is no longer alive"), Rig.Minions->IsAlive(Minions[4]));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
