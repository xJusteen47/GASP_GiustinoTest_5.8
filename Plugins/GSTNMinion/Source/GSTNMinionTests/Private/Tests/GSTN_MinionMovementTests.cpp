// Automation tests of the lightweight movement backend (M8 G4): flat ground, steps, slopes, walls, tunnelling, ground snap, gravity
// direction and push back. Each test builds a transient game world with collision boxes spawned in code (no content) and drives
// TickBatch directly, like the G3 runtime tests.

#include "Components/BoxComponent.h"
#include "Data/GSTN_MinionDefinition.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Runtime/GSTN_MinionCombatSubsystem.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_MINION_MOVEMENT_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	/** A transient game world with an activated minion subsystem and helpers for boxes, minions and frames */
	struct FMovementRig
	{
		FMovementRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNMinionMovementTestWorld"));
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

		~FMovementRig()
		{
			if (Minions)
			{
				Minions->SetForcedLODTier(INDEX_NONE);
				Minions->SetGravitySource(nullptr);
			}
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		/** A blocking box (WorldStatic, BlockAll) of half size Extent */
		void AddBox(const FVector& Centre, const FVector& Extent, const FRotator& Rotation = FRotator::ZeroRotator)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Rotation, Centre), Params);
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
			Box->SetBoxExtent(Extent);
			Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			Actor->SetRootComponent(Box);
			Box->SetWorldTransform(FTransform(Rotation, Centre));
			Box->RegisterComponent();
		}

		/** A 40 m floor whose top is at Z = 0 */
		void AddFloor()
		{
			AddBox(FVector(0.0, 0.0, -50.0), FVector(2000.0, 2000.0, 50.0));
		}

		/**
		 * A ramp rising along +X from (StartX, 0, 0) at Degrees, Length cm along its surface. A thick box pitched up, its top face
		 * through the start line
		 */
		void AddRamp(double StartX, double Degrees, double Length)
		{
			const double Slope = FMath::DegreesToRadians(Degrees);
			const FVector Normal(-FMath::Sin(Slope), 0.0, FMath::Cos(Slope));
			const FVector TopCentre(StartX + 0.5 * Length * FMath::Cos(Slope), 0.0, 0.5 * Length * FMath::Sin(Slope));
			const double HalfThickness = 100.0;
			AddBox(TopCentre - Normal * HalfThickness, FVector(0.5 * Length, 500.0, HalfThickness), FRotator(Degrees, 0.0, 0.0));
		}

		/** Spawns and activates a minion with its feet at Feet */
		FGSTN_MinionHandle Spawn(const FVector& Feet)
		{
			FGSTN_MinionSpawnParams Params;
			Params.bPlaySpawnTimeline = false;
			const FGSTN_MinionHandle Handle = Minions->EnqueueSpawn(Definition.Get(), FTransform(Feet), Params);
			Step(1);
			return Handle;
		}

		/** Runs Frames batches; when Direction is set, writes the intent before each (it would go stale otherwise) */
		void Step(int32 Frames, const FGSTN_MinionHandle& Handle = FGSTN_MinionHandle(), const FVector2D& Direction = FVector2D::ZeroVector, float Speed = 0.f)
		{
			for (int32 Index = 0; Index < Frames; ++Index)
			{
				if (!Handle.IsNull())
				{
					FGSTN_MinionIntent Intent;
					Intent.MoveDir = Direction.GetSafeNormal();
					Intent.DesiredSpeed = Speed;
					Minions->SetIntent(Handle, Intent);
				}
				Minions->TickBatch(++Frame);
			}
		}

		FVector Feet(const FGSTN_MinionHandle& Handle) const
		{
			return FVector(Minions->GetStore().Position[Handle.Index]);
		}

		bool IsGrounded(const FGSTN_MinionHandle& Handle) const
		{
			return Minions->GetMovementColumns().bGrounded[Handle.Index] != 0;
		}

		UWorld* World = nullptr;
		UGSTN_MinionCombatSubsystem* Minions = nullptr;
		TStrongObjectPtr<UGSTN_MinionDefinition> Definition;
		int32 Frame = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionMovementFlatTest, "GSTNMinion.Runtime.Movement.Flat", GSTN_MINION_MOVEMENT_TEST_FLAGS)
bool FGSTN_MinionMovementFlatTest::RunTest(const FString& Parameters)
{
	FMovementRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	Rig.AddFloor();
	const FGSTN_MinionHandle Minion = Rig.Spawn(FVector(0.0, 0.0, 2.0));
	Rig.Step(10);
	TestTrue(TEXT("the minion lands on the floor"), Rig.IsGrounded(Minion));

	// Two seconds at 200 cm/s (1500 cm/s^2 to get there): about 390 cm
	Rig.Step(125, Minion, FVector2D(1.0, 0.0), 200.f);
	const FVector Feet = Rig.Feet(Minion);
	TestTrue(*FString::Printf(TEXT("walks about 390 cm along X (%.1f)"), Feet.X), Feet.X > 340.0 && Feet.X < 410.0);
	TestTrue(*FString::Printf(TEXT("stays on the floor (Z %.2f)"), Feet.Z), FMath::Abs(Feet.Z) < 3.0);
	TestTrue(TEXT("still grounded"), Rig.IsGrounded(Minion));
	TestEqual(TEXT("walking state"), Rig.Minions->GetStore().ActionState[Minion.Index], static_cast<uint8>(EGSTN_MinionActionState::Walk));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionMovementStepUpTest, "GSTNMinion.Runtime.Movement.StepUp", GSTN_MINION_MOVEMENT_TEST_FLAGS)
bool FGSTN_MinionMovementStepUpTest::RunTest(const FString& Parameters)
{
	FMovementRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	Rig.AddFloor();
	// A 30 cm step from X = 100 on
	Rig.AddBox(FVector(600.0, 0.0, 15.0), FVector(500.0, 500.0, 15.0));
	const FGSTN_MinionHandle Minion = Rig.Spawn(FVector(0.0, 0.0, 2.0));
	Rig.Step(10);
	Rig.Step(125, Minion, FVector2D(1.0, 0.0), 200.f);
	const FVector Feet = Rig.Feet(Minion);
	TestTrue(*FString::Printf(TEXT("got past the step edge (X %.1f)"), Feet.X), Feet.X > 200.0);
	TestTrue(*FString::Printf(TEXT("stands on the step (Z %.2f)"), Feet.Z), FMath::Abs(Feet.Z - 30.0) < 3.0);
	TestTrue(TEXT("grounded on the step"), Rig.IsGrounded(Minion));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionMovementSlope40Test, "GSTNMinion.Runtime.Movement.Slope40Walks", GSTN_MINION_MOVEMENT_TEST_FLAGS)
bool FGSTN_MinionMovementSlope40Test::RunTest(const FString& Parameters)
{
	FMovementRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	Rig.AddFloor();
	Rig.AddRamp(100.0, 40.0, 1200.0);
	const FGSTN_MinionHandle Minion = Rig.Spawn(FVector(0.0, 0.0, 2.0));
	Rig.Step(10);
	Rig.Step(125, Minion, FVector2D(1.0, 0.0), 200.f);
	const FVector Feet = Rig.Feet(Minion);
	TestTrue(*FString::Printf(TEXT("climbs the 40 degree ramp (Z %.1f)"), Feet.Z), Feet.Z > 60.0);
	TestTrue(TEXT("grounded on the ramp"), Rig.IsGrounded(Minion));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionMovementSlope60Test, "GSTNMinion.Runtime.Movement.Slope60Blocks", GSTN_MINION_MOVEMENT_TEST_FLAGS)
bool FGSTN_MinionMovementSlope60Test::RunTest(const FString& Parameters)
{
	FMovementRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	Rig.AddFloor();
	Rig.AddRamp(100.0, 60.0, 1200.0);
	const FGSTN_MinionHandle Minion = Rig.Spawn(FVector(0.0, 0.0, 2.0));
	Rig.Step(10);
	Rig.Step(125, Minion, FVector2D(1.0, 0.0), 200.f);
	const FVector Feet = Rig.Feet(Minion);
	TestTrue(*FString::Printf(TEXT("does not climb the 60 degree ramp (Z %.1f)"), Feet.Z), Feet.Z < 25.0);
	TestTrue(*FString::Printf(TEXT("stops at its foot (X %.1f)"), Feet.X), Feet.X < 130.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionMovementWallSlideTest, "GSTNMinion.Runtime.Movement.WallSlide", GSTN_MINION_MOVEMENT_TEST_FLAGS)
bool FGSTN_MinionMovementWallSlideTest::RunTest(const FString& Parameters)
{
	FMovementRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	Rig.AddFloor();
	// A tall wall whose face is the plane X = 200
	Rig.AddBox(FVector(250.0, 0.0, 200.0), FVector(50.0, 1500.0, 200.0));
	const FGSTN_MinionHandle Minion = Rig.Spawn(FVector(0.0, 0.0, 2.0));
	Rig.Step(10);
	Rig.Step(125, Minion, FVector2D(1.0, 1.0), 200.f);
	const FVector Feet = Rig.Feet(Minion);
	const double Radius = Rig.Definition->Stats.CapsuleRadius;
	TestTrue(*FString::Printf(TEXT("stays in front of the wall (X %.1f)"), Feet.X), Feet.X <= 200.0 - Radius + 1.0);
	TestTrue(*FString::Printf(TEXT("slides along it (Y %.1f)"), Feet.Y), Feet.Y > 200.0);
	TestTrue(TEXT("grounded"), Rig.IsGrounded(Minion));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionMovementFallingAgainstWallTest, "GSTNMinion.Runtime.Movement.FallingAgainstWallLands", GSTN_MINION_MOVEMENT_TEST_FLAGS)
bool FGSTN_MinionMovementFallingAgainstWallTest::RunTest(const FString& Parameters)
{
	FMovementRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	Rig.AddFloor();
	// A tall wall whose face is the plane X = 200; the minion starts 50 cm up, touching it, and keeps walking into it
	Rig.AddBox(FVector(250.0, 0.0, 200.0), FVector(50.0, 1500.0, 200.0));
	const double Radius = Rig.Definition->Stats.CapsuleRadius;
	const FGSTN_MinionHandle Minion = Rig.Spawn(FVector(200.0 - Radius - 0.05, 0.0, 50.0));
	Rig.Step(60, Minion, FVector2D(1.0, 0.0), 200.f);
	FVector Feet = Rig.Feet(Minion);
	TestTrue(TEXT("lands while pressed against the wall"), Rig.IsGrounded(Minion));
	TestTrue(*FString::Printf(TEXT("on the floor (Z %.2f)"), Feet.Z), FMath::Abs(Feet.Z) < 3.0);

	// Pressing on for two seconds must neither climb the wall nor leave the ground
	Rig.Step(125, Minion, FVector2D(1.0, 0.0), 200.f);
	Feet = Rig.Feet(Minion);
	TestTrue(*FString::Printf(TEXT("does not climb the wall (Z %.2f)"), Feet.Z), FMath::Abs(Feet.Z) < 3.0);
	TestTrue(TEXT("still grounded"), Rig.IsGrounded(Minion));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionMovementNoTunnelTest, "GSTNMinion.Runtime.Movement.NoTunnelAt3000", GSTN_MINION_MOVEMENT_TEST_FLAGS)
bool FGSTN_MinionMovementNoTunnelTest::RunTest(const FString& Parameters)
{
	FMovementRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	Rig.AddFloor();
	// A 10 cm wall at X 300..310, much thinner than one frame of travel at 3000 cm/s (48 cm)
	Rig.AddBox(FVector(305.0, 0.0, 200.0), FVector(5.0, 500.0, 200.0));
	const FGSTN_MinionHandle Minion = Rig.Spawn(FVector(0.0, 0.0, 2.0));
	Rig.Step(10);

	TestTrue(TEXT("the launch is accepted"), Rig.Minions->LaunchMinion(Minion, FVector(3000.0, 0.0, 200.0), false));
	Rig.Step(60);
	FVector Feet = Rig.Feet(Minion);
	TestTrue(*FString::Printf(TEXT("a launch does not cross the thin wall (X %.1f)"), Feet.X), Feet.X < 300.0);

	// The same with a slot stepped every 4 frames (dt x 4, 192 cm per step)
	Rig.Minions->SetForcedLODTier(static_cast<int32>(EGSTN_MinionLODTier::Far));
	Rig.Minions->LaunchMinion(Minion, FVector(3000.0, 0.0, 0.0), false);
	Rig.Step(60);
	Feet = Rig.Feet(Minion);
	TestTrue(*FString::Printf(TEXT("nor with LOD steps (X %.1f)"), Feet.X), Feet.X < 300.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionMovementGroundSnapTest, "GSTNMinion.Runtime.Movement.GroundSnap", GSTN_MINION_MOVEMENT_TEST_FLAGS)
bool FGSTN_MinionMovementGroundSnapTest::RunTest(const FString& Parameters)
{
	FMovementRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	Rig.AddFloor();
	// A 30 cm platform ending at X = 100: walking off it is a step down, not a fall
	Rig.AddBox(FVector(-400.0, 0.0, 15.0), FVector(500.0, 500.0, 15.0));
	const FGSTN_MinionHandle Minion = Rig.Spawn(FVector(0.0, 0.0, 32.0));
	Rig.Step(10);
	TestTrue(TEXT("on the platform"), Rig.IsGrounded(Minion) && FMath::Abs(Rig.Feet(Minion).Z - 30.0) < 3.0);

	bool bAlwaysGrounded = true;
	for (int32 Frame = 0; Frame < 100; ++Frame)
	{
		Rig.Step(1, Minion, FVector2D(1.0, 0.0), 200.f);
		bAlwaysGrounded &= Rig.IsGrounded(Minion);
	}
	const FVector Feet = Rig.Feet(Minion);
	TestTrue(TEXT("the step down keeps the minion grounded every frame"), bAlwaysGrounded);
	TestTrue(*FString::Printf(TEXT("ends on the floor below (X %.1f, Z %.2f)"), Feet.X, Feet.Z), Feet.X > 150.0 && FMath::Abs(Feet.Z) < 3.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionMovementGravityTest, "GSTNMinion.Runtime.Movement.GravityDirectionChange", GSTN_MINION_MOVEMENT_TEST_FLAGS)
bool FGSTN_MinionMovementGravityTest::RunTest(const FString& Parameters)
{
	FMovementRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	// Gravity along -X: the wall whose face is the plane X = -500 is the floor
	Rig.Minions->SetGravitySource([](const FVector&) { return FVector(-980.0, 0.0, 0.0); });
	Rig.AddBox(FVector(-550.0, 0.0, 0.0), FVector(50.0, 1000.0, 1000.0));
	const FGSTN_MinionHandle Minion = Rig.Spawn(FVector(0.0, 0.0, 0.0));

	// About a second to fall 500 cm at 980 cm/s^2; two to settle
	Rig.Step(125);
	const FVector Feet = Rig.Feet(Minion);
	TestTrue(*FString::Printf(TEXT("fell along -X onto the wall (X %.1f)"), Feet.X), FMath::Abs(Feet.X + 500.0) < 3.0);
	TestTrue(*FString::Printf(TEXT("did not move along Z (Z %.2f)"), Feet.Z), FMath::Abs(Feet.Z) < 1.0);
	TestTrue(TEXT("grounded on the wall"), Rig.IsGrounded(Minion));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_MinionMovementPushBackTest, "GSTNMinion.Runtime.Movement.PushBackStaysGrounded", GSTN_MINION_MOVEMENT_TEST_FLAGS)
bool FGSTN_MinionMovementPushBackTest::RunTest(const FString& Parameters)
{
	FMovementRig Rig;
	if (!TestNotNull(TEXT("the minion subsystem exists in a game world"), Rig.Minions))
	{
		return false;
	}
	Rig.AddFloor();
	// A 100 cm platform ending at X = 300: a push back toward the edge must stop there, not fly off
	Rig.AddBox(FVector(-200.0, 0.0, 50.0), FVector(500.0, 500.0, 50.0));
	const FGSTN_MinionHandle Minion = Rig.Spawn(FVector(0.0, 0.0, 102.0));
	Rig.Step(10);
	TestTrue(TEXT("on the platform"), Rig.IsGrounded(Minion));

	// An upward part in the push is dropped: a push back never leaves the ground
	TestTrue(TEXT("the push back is accepted"), Rig.Minions->LaunchMinion(Minion, FVector(1500.0, 0.0, 800.0), true));
	TestEqual(TEXT("pushed back state"), Rig.Minions->GetStore().ActionState[Minion.Index], static_cast<uint8>(EGSTN_MinionActionState::PushedBack));
	bool bAlwaysGrounded = true;
	double MaxZ = 0.0;
	for (int32 Frame = 0; Frame < 90; ++Frame)
	{
		Rig.Step(1);
		bAlwaysGrounded &= Rig.IsGrounded(Minion);
		MaxZ = FMath::Max(MaxZ, Rig.Feet(Minion).Z);
	}
	const FVector Feet = Rig.Feet(Minion);
	const double Radius = Rig.Definition->Stats.CapsuleRadius;
	TestTrue(TEXT("grounded every frame of the push back"), bAlwaysGrounded);
	TestTrue(*FString::Printf(TEXT("never rose (max Z %.2f)"), MaxZ), MaxZ < 103.0);
	// At the edge the capsule may rest on the corner and sink a few cm (perched on half its radius at most)
	TestTrue(*FString::Printf(TEXT("stopped at the ledge, still on the platform (X %.1f, Z %.2f)"), Feet.X, Feet.Z),
		Feet.X <= 300.0 + Radius && Feet.Z > 92.0 && Feet.Z < 103.0);
	TestTrue(TEXT("the push back ended"), Rig.Minions->GetStore().ActionState[Minion.Index] != static_cast<uint8>(EGSTN_MinionActionState::PushedBack));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
