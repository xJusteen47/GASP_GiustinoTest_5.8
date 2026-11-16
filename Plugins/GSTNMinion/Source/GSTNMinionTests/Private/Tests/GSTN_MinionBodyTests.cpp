// Automation tests of the minion body (pushbox P5): the pushbox and rigid core a minion carries in the step snapshot (what a fighter's push
// and core read), and the lightweight movement meeting fighters and other minions with them. Transient game world, boxes spawned in code,
// TickBatch driven directly (as GSTN_MinionMovementTests.cpp). A world test has no NP fighter: fighters are fixed bodies given through
// UGSTN_MinionCombatSubsystem::TestFighterBodies, the fighter's own side is FGSTN_Pushbox (Core.Pushbox.* tests) fed with the minion's entry.

#include "Components/BoxComponent.h"
#include "Data/GSTN_MinionDefinition.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Rules/GSTN_CombatRules.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "Sync/GSTN_SyncFragment.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "World/GSTN_StepSnapshot.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_MINION_BODY_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	struct FBodyRig
	{
		FBodyRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNMinionBodyTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Minions = World->GetSubsystem<UGSTN_MinionCombatSubsystem>();
			if (Minions)
			{
				Minions->ActivateForTests();
				Minions->SetForcedLODTier(static_cast<int32>(EGSTN_MinionLODTier::Combat));
				Minions->SetFixedStepSeconds(1.f / 62.5f);
			}
			Definition.Reset(NewObject<UGSTN_MinionDefinition>(GetTransientPackage(), NAME_None, RF_Transient));
			Definition->Stats.MaxHealth = 40.f;
		}

		~FBodyRig()
		{
			if (Minions)
			{
				Minions->SetForcedLODTier(INDEX_NONE);
				Minions->TestFighterBodies.Reset();
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		void AddBox(const FVector& Centre, const FVector& Extent)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Centre), Params);
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Box->SetBoxExtent(Extent);
			Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Actor->SetRootComponent(Box);
			Box->SetWorldTransform(FTransform(Centre));
			Box->RegisterComponent();
		}

		void AddFloor() { AddBox(FVector(0.0, 0.0, -50.0), FVector(3000.0, 3000.0, 50.0)); }

		FGSTN_MinionHandle Spawn(const FVector& Feet)
		{
			FGSTN_MinionSpawnParams Params;
			Params.bPlaySpawnTimeline = false;
			const FGSTN_MinionHandle Handle = Minions->EnqueueSpawn(Definition.Get(), FTransform(Feet), Params);
			Step(1);
			return Handle;
		}

		/** A fixed fighter body (the P4 fighter default: capsule 30 cm x 0.8 pushbox, core 15 cm) standing with its feet at Feet */
		void AddFighter(int32 EntityId, const FVector& Feet)
		{
			FGSTN_PushboxBody& Body = Minions->TestFighterBodies.AddDefaulted_GetRef();
			Body.EntityId = EntityId;
			Body.Cylinder.Center = Feet + FVector(0.0, 0.0, 90.0);
			Body.Cylinder.Radius = 24.f;
			Body.Cylinder.HalfHeight = 90.f;
			Body.Cylinder.CoreRadius = 15.f;
		}

		/** One batch; every handle of Walkers gets the same intent first */
		void Step(int32 Frames, TConstArrayView<FGSTN_MinionHandle> Walkers = {}, const FVector2D& Direction = FVector2D::ZeroVector, float Speed = 0.f,
			TFunctionRef<void()> AfterEach = [] {})
		{
			for (int32 Index = 0; Index < Frames; ++Index)
			{
				for (const FGSTN_MinionHandle& Handle : Walkers)
				{
					FGSTN_MinionIntent Intent;
					Intent.MoveDir = Direction.GetSafeNormal();
					Intent.DesiredSpeed = Speed;
					Minions->SetIntent(Handle, Intent);
				}
				Minions->TickBatch(++Frame);
				AfterEach();
			}
		}

		FVector Feet(const FGSTN_MinionHandle& Handle) const { return FVector(Minions->GetStore().Position[Handle.Index]); }

		static double Horizontal(const FVector& A, const FVector& B) { return FVector2D(A.X - B.X, A.Y - B.Y).Size(); }

		UWorld* World = nullptr;
		UGSTN_MinionCombatSubsystem* Minions = nullptr;
		TStrongObjectPtr<UGSTN_MinionDefinition> Definition;
		int32 Frame = 0;
	};

	const FGSTN_SnapshotEntry* FindEntry(const TArray<FGSTN_SnapshotEntry>& Entries, const FGSTN_MinionHandle& Handle)
	{
		const int32 EntityId = GSTNEntityKinds::MakeMinionEntityId(static_cast<uint16>(Handle.Index));
		return Entries.FindByPredicate([EntityId](const FGSTN_SnapshotEntry& Entry) { return Entry.EntityId == EntityId; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBodySnapshotTest, "GSTNMinion.Runtime.Body.SnapshotEntry", GSTN_MINION_BODY_TEST_FLAGS)
bool FGSTN_MinionBodySnapshotTest::RunTest(const FString& Parameters)
{
	FBodyRig Rig;
	if (!TestNotNull(TEXT("minion subsystem"), Rig.Minions))
	{
		return false;
	}
	Rig.AddFloor();
	const FGSTN_MinionHandle Minion = Rig.Spawn(FVector(0.0, 0.0, 2.0));
	Rig.Step(10);
	const FGSTN_PushboxSettings& Settings = GetDefault<UGSTN_CombatRules>()->Pushbox;
	const FGSTN_MinionStats& Stats = Rig.Definition->Stats;

	TArray<FGSTN_SnapshotEntry> Entries;
	Rig.Minions->AppendSnapshotEntities(Rig.Frame + 1, Entries);
	const FGSTN_SnapshotEntry* Entry = FindEntry(Entries, Minion);
	if (!TestNotNull(TEXT("the minion is in the snapshot"), Entry))
	{
		return false;
	}
	TestTrue(TEXT("default type: core + pushbox"), Rig.Definition->BodyCollision.Profile.Mode == EGSTN_BodyCollisionMode::PushboxAndCore);
	TestEqual(TEXT("pushbox radius = capsule x CapsuleRadiusScale"), Entry->Pushbox.Radius, Stats.CapsuleRadius * Settings.CapsuleRadiusScale, 0.01f);
	TestEqual(TEXT("core radius = capsule x CoreRadiusScale"), Entry->Pushbox.CoreRadius, Stats.CapsuleRadius * Settings.CoreRadiusScale, 0.01f);
	TestEqual(TEXT("cylinder centred on the capsule"), Entry->Pushbox.Center.Z, Rig.Feet(Minion).Z + Stats.CapsuleHalfHeight, 0.01);

	// Data per type: soft only, then none; a state listed without a body has none (the store state comes from the batch, so check the pure resolve)
	Rig.Definition->BodyCollision.Profile.Mode = EGSTN_BodyCollisionMode::PushboxOnly;
	Entries.Reset();
	Rig.Minions->AppendSnapshotEntities(Rig.Frame + 1, Entries);
	Entry = FindEntry(Entries, Minion);
	TestTrue(TEXT("PushboxOnly: pushbox, no core"), Entry && Entry->Pushbox.IsActive() && Entry->Pushbox.CoreRadius == 0.f);
	Rig.Definition->BodyCollision.Profile.Mode = EGSTN_BodyCollisionMode::None;
	Entries.Reset();
	Rig.Minions->AppendSnapshotEntities(Rig.Frame + 1, Entries);
	Entry = FindEntry(Entries, Minion);
	TestTrue(TEXT("None: no pushbox"), Entry && !Entry->Pushbox.IsActive());
	Rig.Definition->BodyCollision.Profile.Mode = EGSTN_BodyCollisionMode::PushboxAndCore;
	FGSTN_PushboxCylinder Cylinder;
	TestFalse(TEXT("Launched: no body"), Rig.Definition->ResolvePushbox(static_cast<uint8>(EGSTN_MinionActionState::Launched), FVector::ZeroVector, FVector::UpVector, Settings, Cylinder));
	TestFalse(TEXT("Knockdown: no body"), Rig.Definition->ResolvePushbox(static_cast<uint8>(EGSTN_MinionActionState::Knockdown), FVector::ZeroVector, FVector::UpVector, Settings, Cylinder));
	TestTrue(TEXT("Hitstun: body"), Rig.Definition->ResolvePushbox(static_cast<uint8>(EGSTN_MinionActionState::Hitstun), FVector::ZeroVector, FVector::UpVector, Settings, Cylinder));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBodyFighterWalksInTest, "GSTNMinion.Runtime.Body.FighterWalksIntoMinion", GSTN_MINION_BODY_TEST_FLAGS)
bool FGSTN_MinionBodyFighterWalksInTest::RunTest(const FString& Parameters)
{
	// The fighter's side, with the minion's real snapshot entry: a fighter running at 600 cm/s into a standing minion, through the same
	// pure ConstrainMove its Mover modifier calls each frame. The minion's own side (it is pushed, it never walks into the fighter) is the next test
	FBodyRig Rig;
	if (!TestNotNull(TEXT("minion subsystem"), Rig.Minions))
	{
		return false;
	}
	Rig.AddFloor();
	const FGSTN_MinionHandle Minion = Rig.Spawn(FVector(0.0, 0.0, 2.0));
	Rig.Step(10);
	TArray<FGSTN_SnapshotEntry> Entries;
	Rig.Minions->AppendSnapshotEntities(Rig.Frame + 1, Entries);
	const FGSTN_SnapshotEntry* Entry = FindEntry(Entries, Minion);
	if (!TestTrue(TEXT("the minion has a body"), Entry && Entry->Pushbox.IsActive()))
	{
		return false;
	}
	const FGSTN_PushboxSettings& Settings = GetDefault<UGSTN_CombatRules>()->Pushbox;
	FGSTN_PushboxBody MinionBody;
	MinionBody.EntityId = Entry->EntityId;
	MinionBody.Cylinder = Entry->Pushbox;
	FGSTN_PushboxBody Fighter;
	Fighter.EntityId = 1;
	Fighter.Cylinder.Center = MinionBody.Cylinder.Center - FVector(200.0, 0.0, 0.0);
	Fighter.Cylinder.Radius = 24.f;
	Fighter.Cylinder.HalfHeight = 90.f;
	Fighter.Cylinder.CoreRadius = 15.f;
	const double CoreSum = Fighter.Cylinder.CoreRadius + MinionBody.Cylinder.CoreRadius;
	double Smallest = TNumericLimits<double>::Max();
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		FVector Velocity(600.0, 0.0, 0.0);
		const FVector Move = FGSTN_Pushbox::ConstrainMove(Fighter, Velocity / 62.5, MakeArrayView(&MinionBody, 1), Settings, FVector::UpVector, &Velocity);
		Fighter.Cylinder.Center += Move;
		Smallest = FMath::Min(Smallest, FBodyRig::Horizontal(Fighter.Cylinder.Center, MinionBody.Cylinder.Center));
	}
	AddInfo(FString::Printf(TEXT("fighter into minion: smallest centre gap %.2f cm (core sum %.1f)"), Smallest, CoreSum));
	TestTrue(*FString::Printf(TEXT("the fighter stops at the minion's core (smallest %.2f)"), Smallest), Smallest >= CoreSum - 0.01);
	TestTrue(TEXT("never crosses it"), Fighter.Cylinder.Center.X < MinionBody.Cylinder.Center.X);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBodyIntoFighterTest, "GSTNMinion.Runtime.Body.MinionWalksIntoFighter", GSTN_MINION_BODY_TEST_FLAGS)
bool FGSTN_MinionBodyIntoFighterTest::RunTest(const FString& Parameters)
{
	FBodyRig Rig;
	if (!TestNotNull(TEXT("minion subsystem"), Rig.Minions))
	{
		return false;
	}
	Rig.AddFloor();
	const FVector FighterFeet(300.0, 0.0, 0.0);
	Rig.AddFighter(1, FighterFeet);
	const FGSTN_MinionHandle Minion = Rig.Spawn(FVector(0.0, 0.0, 2.0));
	Rig.Step(10);
	const double CoreSum = 15.0 + Rig.Definition->Stats.CapsuleRadius * GetDefault<UGSTN_CombatRules>()->Pushbox.CoreRadiusScale;
	double Smallest = TNumericLimits<double>::Max();
	const FGSTN_MinionHandle Walkers[] = { Minion };
	Rig.Step(150, Walkers, FVector2D(1.0, 0.0), 450.f, [&]
	{
		Smallest = FMath::Min(Smallest, FBodyRig::Horizontal(Rig.Feet(Minion), FighterFeet));
	});
	AddInfo(FString::Printf(TEXT("minion into fighter at 450 cm/s: smallest centre gap %.2f cm (core sum %.1f), end X %.1f"), Smallest, CoreSum, Rig.Feet(Minion).X));
	TestTrue(*FString::Printf(TEXT("stops at the fighter's core (smallest %.2f)"), Smallest), Smallest >= CoreSum - 0.05);
	TestTrue(TEXT("stays in front"), Rig.Feet(Minion).X < FighterFeet.X);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBodyHeadOnTest, "GSTNMinion.Runtime.Body.MinionsHeadOn", GSTN_MINION_BODY_TEST_FLAGS)
bool FGSTN_MinionBodyHeadOnTest::RunTest(const FString& Parameters)
{
	// Separation steering off: only the cores keep two minions running at each other apart
	FBodyRig Rig;
	if (!TestNotNull(TEXT("minion subsystem"), Rig.Minions))
	{
		return false;
	}
	Rig.Minions->GetMovementSettings().bSeparation = false;
	Rig.AddFloor();
	const FGSTN_MinionHandle A = Rig.Spawn(FVector(-200.0, 0.0, 2.0));
	const FGSTN_MinionHandle B = Rig.Spawn(FVector(200.0, 0.0, 2.0));
	Rig.Step(10);
	const double CoreSum = 2.0 * Rig.Definition->Stats.CapsuleRadius * GetDefault<UGSTN_CombatRules>()->Pushbox.CoreRadiusScale;
	double Smallest = TNumericLimits<double>::Max();
	bool bCrossed = false;
	for (int32 Frame = 0; Frame < 150; ++Frame)
	{
		const FGSTN_MinionHandle OnlyA[] = { A };
		FGSTN_MinionIntent Intent;
		Intent.MoveDir = FVector2D(-1.0, 0.0);
		Intent.DesiredSpeed = 450.f;
		Rig.Minions->SetIntent(B, Intent);
		Rig.Step(1, OnlyA, FVector2D(1.0, 0.0), 450.f);
		Smallest = FMath::Min(Smallest, FBodyRig::Horizontal(Rig.Feet(A), Rig.Feet(B)));
		bCrossed |= Rig.Feet(A).X > Rig.Feet(B).X;
	}
	Rig.Minions->GetMovementSettings().bSeparation = true;
	AddInfo(FString::Printf(TEXT("minions head-on at 450 cm/s, no separation: smallest gap %.2f cm (core sum %.1f)"), Smallest, CoreSum));
	TestTrue(*FString::Printf(TEXT("cores hold (smallest %.2f)"), Smallest), Smallest >= CoreSum - 0.05);
	TestFalse(TEXT("never cross"), bCrossed);
	return true;
}

namespace
{
	/**
	 * The corridor crowd: 8 minions run along +X in a 120 cm corridor at a fighter standing in its middle (narrow enough that a 34 cm minion
	 * cannot slip past a core beside the fighter: it would need 32 cm off the axis, the walls leave 26). Returns the end positions
	 */
	TArray<FVector> RunCorridor(FAutomationTestBase& Test, double& OutSmallestPair, double& OutSmallestToFighter, double& OutWorstWall, double& OutLastStep)
	{
		FBodyRig Rig;
		TArray<FVector> End;
		if (!Rig.Minions)
		{
			return End;
		}
		Rig.AddFloor();
		// Walls whose faces are Y = +-60
		Rig.AddBox(FVector(0.0, 110.0, 150.0), FVector(2000.0, 50.0, 150.0));
		Rig.AddBox(FVector(0.0, -110.0, 150.0), FVector(2000.0, 50.0, 150.0));
		const FVector FighterFeet(400.0, 0.0, 0.0);
		Rig.AddFighter(1, FighterFeet);
		TArray<FGSTN_MinionHandle> Crowd;
		for (int32 Index = 0; Index < 8; ++Index)
		{
			Crowd.Add(Rig.Spawn(FVector(-100.0 - 80.0 * (Index / 2), Index % 2 == 0 ? -20.0 : 20.0, 2.0)));
		}
		Rig.Step(10);
		OutSmallestPair = TNumericLimits<double>::Max();
		OutSmallestToFighter = TNumericLimits<double>::Max();
		OutWorstWall = 0.0;
		OutLastStep = 0.0;
		TArray<FVector> Previous;
		int32 Frame = 0;
		Rig.Step(250, Crowd, FVector2D(1.0, 0.0), 450.f, [&]
		{
			++Frame;
			for (int32 I = 0; I < Crowd.Num(); ++I)
			{
				const FVector P = Rig.Feet(Crowd[I]);
				OutSmallestToFighter = FMath::Min(OutSmallestToFighter, FBodyRig::Horizontal(P, FighterFeet));
				OutWorstWall = FMath::Max(OutWorstWall, FMath::Abs(P.Y));
				for (int32 J = I + 1; J < Crowd.Num(); ++J)
				{
					OutSmallestPair = FMath::Min(OutSmallestPair, FBodyRig::Horizontal(P, Rig.Feet(Crowd[J])));
				}
				if (Frame > 190 && Previous.IsValidIndex(I))
				{
					OutLastStep = FMath::Max(OutLastStep, FBodyRig::Horizontal(P, Previous[I]));
				}
			}
			Previous.Reset();
			for (const FGSTN_MinionHandle& Handle : Crowd)
			{
				Previous.Add(Rig.Feet(Handle));
			}
		});
		for (const FGSTN_MinionHandle& Handle : Crowd)
		{
			End.Add(Rig.Feet(Handle));
		}
		return End;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBodyCorridorTest, "GSTNMinion.Runtime.Body.CorridorCrowd", GSTN_MINION_BODY_TEST_FLAGS)
bool FGSTN_MinionBodyCorridorTest::RunTest(const FString& Parameters)
{
	double SmallestPair = 0.0, SmallestToFighter = 0.0, WorstWall = 0.0, LastStep = 0.0;
	const TArray<FVector> First = RunCorridor(*this, SmallestPair, SmallestToFighter, WorstWall, LastStep);
	if (!TestEqual(TEXT("8 minions ran"), First.Num(), 8))
	{
		return false;
	}
	const FGSTN_PushboxSettings& Settings = GetDefault<UGSTN_CombatRules>()->Pushbox;
	const double MinionCore = 34.0 * Settings.CoreRadiusScale;
	AddInfo(FString::Printf(TEXT("corridor: smallest minion pair %.2f (core sum %.1f), smallest to fighter %.2f (core sum %.1f), widest |Y| %.2f, last 60 frames largest step %.3f"),
		SmallestPair, 2.0 * MinionCore, SmallestToFighter, MinionCore + 15.0, WorstWall, LastStep));
	TestTrue(TEXT("nobody passes the fighter"), First.ContainsByPredicate([](const FVector& P) { return P.X > 400.0; }) == false);
	TestTrue(*FString::Printf(TEXT("the fighter's core holds (%.2f)"), SmallestToFighter), SmallestToFighter >= MinionCore + 15.0 - 1.0);
	TestTrue(*FString::Printf(TEXT("minion cores hold (%.2f)"), SmallestPair), SmallestPair >= 2.0 * MinionCore - 1.0);
	TestTrue(*FString::Printf(TEXT("walls win (|Y| %.2f)"), WorstWall), WorstWall <= 60.0 - 34.0 + 0.5);
	TestTrue(*FString::Printf(TEXT("the crowd settles (largest step %.3f)"), LastStep), LastStep < 1.0);

	// Determinism: the same run again gives the same positions to the bit
	const TArray<FVector> Second = RunCorridor(*this, SmallestPair, SmallestToFighter, WorstWall, LastStep);
	bool bSame = Second.Num() == First.Num();
	for (int32 Index = 0; bSame && Index < First.Num(); ++Index)
	{
		bSame = First[Index] == Second[Index];
	}
	TestTrue(TEXT("a second run ends bit-identical"), bSame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionBodyCrowdCostTest, "GSTNMinion.Runtime.Body.CrowdCost", GSTN_MINION_BODY_TEST_FLAGS)
bool FGSTN_MinionBodyCrowdCostTest::RunTest(const FString& Parameters)
{
	// 160 minions (the store default) packed 20 x 8 and all running at 4 fighters: the movement batch with and without bodies, as an Info
	// line (the toaster budget question), and a loose bound so a pair loop over the whole crowd would show
	auto Run = [this](bool bBodies, double& OutMs) -> bool
	{
		FBodyRig Rig;
		if (!Rig.Minions)
		{
			return false;
		}
		Rig.Minions->GetMovementSettings().bBodyCollision = bBodies;
		Rig.AddFloor();
		for (int32 Fighter = 0; Fighter < 4; ++Fighter)
		{
			Rig.AddFighter(1 + Fighter, FVector(900.0, -300.0 + 200.0 * Fighter, 0.0));
		}
		TArray<FGSTN_MinionHandle> Crowd;
		for (int32 Index = 0; Index < 160; ++Index)
		{
			FGSTN_MinionSpawnParams Params;
			Params.bPlaySpawnTimeline = false;
			Crowd.Add(Rig.Minions->EnqueueSpawn(Rig.Definition.Get(), FTransform(FVector(-800.0 + 75.0 * (Index % 20), -280.0 + 75.0 * (Index / 20), 2.0)), Params));
		}
		Rig.Step(10);
		Rig.Step(30, Crowd, FVector2D(1.0, 0.0), 450.f);
		const double Start = FPlatformTime::Seconds();
		Rig.Step(120, Crowd, FVector2D(1.0, 0.0), 450.f);
		OutMs = (FPlatformTime::Seconds() - Start) * 1000.0 / 120.0;
		Rig.Minions->GetMovementSettings().bBodyCollision = true;
		return true;
	};
	double WithBodies = 0.0, Without = 0.0;
	if (!TestTrue(TEXT("rigs ran"), Run(false, Without) && Run(true, WithBodies)))
	{
		return false;
	}
	AddInfo(FString::Printf(TEXT("160 minions + 4 fighters, batch per frame: %.3f ms with bodies, %.3f ms without (+%.3f ms)"), WithBodies, Without, WithBodies - Without));
	TestTrue(*FString::Printf(TEXT("bodies cost less than the sweeps themselves again (%.3f vs %.3f ms)"), WithBodies, Without), WithBodies < 2.0 * Without + 0.5);
	return true;
}

#endif
