// Tests of the corners, bumps and ledges of the wall slide v2 (wall-slide-v2.md W3 and W4): GSTN.Wall.V2.*
//
// Same headless approach as GSTN_WallV2Tests.cpp: TraverseStep (the world part of the tick) in a throwaway game world, driven by a
// working state, with box geometry. Everything is built in a canonical frame (wall plane x = 0 facing +X, gravity-up +Z, the capsule
// sliding along Y) and carried through a rigid rotation, so the same scene runs under several gravity directions.
// Not covered here (PIE): the full simulation tick, the Mover queries, base riding, the sync state.

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "MoveLibrary/MovementRecord.h"
#include "MoveLibrary/MovementUtilsTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "WallMovement/GSTN_WallMovementDebug.h"
#include "WallMovement/Modes/GSTN_WallSlideV2MovementMode.h"
#include "WallMovement/MoveLibrary/GSTN_WallControlUtils.h"
#include "WallMovement/MoveLibrary/GSTN_WallEdgeUtils.h"
#include "WallMovement/MoveLibrary/GSTN_WallMovementUtils.h"
#include "MoverDataModelTypes.h"
#include "GSTN_WallV2TestAccess.h"

namespace GSTNWallEdgeWorld
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float EdgeDt = 1.f / 60.f;
	constexpr float CapRadius = 34.f;
	constexpr float CapHalfHeight = 88.f;
	/** Depth and width of the second face of a corner wedge (cm) */
	constexpr float WedgeDepth = 60.f;
	constexpr float WedgeWidth = 200.f;

	/** The gravity directions the scenes are run under: the canonical frame, upside down and two skewed ones */
	static TArray<FQuat> Frames()
	{
		return {
			FQuat::Identity,
			FQuat(FVector::XAxisVector, PI),
			FQuat::FindBetweenNormals(FVector::UpVector, FVector(1, 1, 1).GetSafeNormal()),
			FQuat::FindBetweenNormals(FVector::UpVector, FVector(-2, 1, 3).GetSafeNormal()),
		};
	}

	/** Counts the wall queries of a scope through gstn.Wall.CountQueries and restores the cvar */
	struct FQueryCounting
	{
#if GSTN_WALL_DEBUG
		IConsoleVariable* CVar = nullptr;
		int32 Previous = 0;

		FQueryCounting()
		{
			CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("gstn.Wall.CountQueries"));
			if (CVar)
			{
				Previous = CVar->GetInt();
				CVar->Set(1, ECVF_SetByCode);
			}
			GSTNWallDebug::ResetQueryStats();
		}

		~FQueryCounting()
		{
			if (CVar)
			{
				CVar->Set(Previous, ECVF_SetByCode);
			}
			GSTNWallDebug::ResetQueryStats();
		}
#endif
	};

	/**
	 * A throwaway game world holding box geometry, one upright capsule and a working state. All coordinates passed in and read out are
	 * canonical (wall x = 0 facing +X, up +Z); Frame turns them into the world the physics runs in
	 */
	struct FEdgeScene
	{
		UWorld* World = nullptr;
		FQuat Frame = FQuat::Identity;
		UCapsuleComponent* Capsule = nullptr;
		TStrongObjectPtr<UGSTN_WallSlideV2MovementMode> Mode;
		FGSTN_WallTickState State;
		FMovementRecord MoveRecord;

		explicit FEdgeScene(const FQuat& InFrame = FQuat::Identity)
			: Frame(InFrame)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNWallEdgeTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Mode.Reset(NewObject<UGSTN_WallSlideV2MovementMode>(GetTransientPackage()));
			MoveRecord.SetDeltaSeconds(EdgeDt);
		}

		~FEdgeScene()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		FVector ToWorld(const FVector& V) const { return Frame.RotateVector(V); }
		FVector ToCanon(const FVector& V) const { return Frame.UnrotateVector(V); }

		void AddBoxCentered(const FVector& Center, const FVector& HalfExtent, float YawDeg)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("Geometry"));
			Box->SetBoxExtent(HalfExtent);
			Box->SetMobility(EComponentMobility::Static);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Actor->SetRootComponent(Box);
			Box->SetWorldLocationAndRotation(ToWorld(Center), Frame * FQuat(FVector::UpVector, FMath::DegreesToRadians(YawDeg)));
			Box->RegisterComponent();
		}

		/** Axis-aligned box from two canonical corners */
		void AddBox(const FVector& Min, const FVector& Max)
		{
			AddBoxCentered((Min + Max) * 0.5, (Max - Min) * 0.5, 0.f);
		}

		/** The wall: solid behind x = 0, along Y from Y0 to Y1 */
		void AddWall(float Y0 = -2000.f, float Y1 = 2000.f)
		{
			AddBox(FVector(-50, Y0, -4000), FVector(0, Y1, 4000));
		}

		/** An outside edge at y = Edge: the wall ends there and its surface turns away by AngleDeg (90 = the wall stops, a pillar) */
		void AddConvexCorner(float Edge, float AngleDeg)
		{
			AddWall(-2000.f, Edge);
			const float A = FMath::DegreesToRadians(AngleDeg);
			const FVector N2(FMath::Cos(A), FMath::Sin(A), 0), T2(-FMath::Sin(A), FMath::Cos(A), 0);
			AddBoxCentered(FVector(0, Edge, 0) + T2 * WedgeWidth - N2 * WedgeDepth, FVector(WedgeDepth, WedgeWidth, 4000), AngleDeg);
		}

		/** An inside edge at y = Edge: a second surface rises toward the character, turned by AngleDeg from the wall (90 = a perpendicular wall) */
		void AddConcaveCorner(float Edge, float AngleDeg)
		{
			AddWall();
			const float A = FMath::DegreesToRadians(AngleDeg);
			const FVector N2(FMath::Cos(A), -FMath::Sin(A), 0), T2(FMath::Sin(A), FMath::Cos(A), 0);
			AddBoxCentered(FVector(0, Edge, 0) + T2 * WedgeWidth - N2 * WedgeDepth, FVector(WedgeDepth, WedgeWidth, 4000), -AngleDeg);
		}

		/** The capsule, upright and facing the wall, seated Gap (default: the mode's WallGap) from the plane at canonical (Y, Z) */
		void PlaceCapsule(float Y, float Z, float Gap = -1.f)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			Capsule = NewObject<UCapsuleComponent>(Actor, TEXT("Capsule"));
			Capsule->InitCapsuleSize(CapRadius, CapHalfHeight);
			Capsule->SetMobility(EComponentMobility::Movable);
			Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Capsule->SetCollisionObjectType(ECC_Pawn);
			Capsule->SetCollisionResponseToAllChannels(ECR_Block);
			Actor->SetRootComponent(Capsule);

			const float UseGap = Gap >= 0.f ? Gap : Mode->WallGap;
			const FVector Up = ToWorld(FVector::UpVector);
			const FVector Normal = ToWorld(FVector::ForwardVector);
			Capsule->SetWorldLocationAndRotation(ToWorld(FVector(CapRadius + UseGap, Y, Z)), UGSTN_WallTraverseModeBase::ComputeFacingQuat(FQuat::Identity, Normal, Up));
			Capsule->RegisterComponent();

			State = FGSTN_WallTickState();
			State.WallNormal = Normal;
			State.WallContactPoint = ToWorld(FVector(0, Y, Z));
			State.GravityUp = Up;
			State.Orientation = Capsule->GetComponentQuat();
			State.bHasWall = true;
		}

		FVector Pos() const { return ToCanon(Capsule->GetComponentLocation()); }
		FVector Velocity() const { return ToCanon(State.Velocity); }
		FVector PlaneNormal() const { return ToCanon(State.WallNormal); }
		float PlaneTurnDeg() const { return UGSTN_WallEdgeUtils::AngleBetweenNormalsDeg(PlaneNormal().GetSafeNormal(), FVector::ForwardVector); }

		void Tick(const FVector& CanonVelocity)
		{
			State.Velocity = ToWorld(CanonVelocity);
			FGSTN_WallTraverseTestAccess::Step(*Mode, FMovingComponentSet(Capsule), EdgeDt, !CanonVelocity.IsNearlyZero(), State, MoveRecord);
		}

		FGSTN_WallAheadResult Probe(const FVector& CanonVelocity)
		{
			State.Velocity = ToWorld(CanonVelocity);
			return FGSTN_WallTraverseTestAccess::ProbeAhead(*Mode, FMovingComponentSet(Capsule), State);
		}
	};

	/** Feeds the speed the last tick ended with back as the next proposed velocity, regaining Gain cm/s^2 up to Target (the control model's pull) */
	static FVector NextProposed(const FEdgeScene& Scene, const FVector& Direction, float Target, float Gain)
	{
		const float Speed = FMath::Min(Scene.State.Velocity.Size() + Gain * EdgeDt, Target);
		return Direction * Speed;
	}

	static FMoverTickStartData MakeStartStateWithVelocity(const FVector& Velocity)
	{
		FMoverTickStartData StartState;
		FMoverDefaultSyncState& Sync = StartState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
		Sync.SetTransforms_WorldSpace(FVector::ZeroVector, FRotator::ZeroRotator, Velocity, FVector::ZeroVector, nullptr);
		return StartState;
	}

	static float AllowedAt(const FGSTN_WallEdgeSettings& Edges, EGSTN_WallEdgeKind Kind, float Angle, float Reference, float Distance)
	{
		return UGSTN_WallEdgeUtils::AllowedSpeedAtDistance(Edges, UGSTN_WallEdgeUtils::EdgeSpeedCap(Edges, Kind, Angle, Reference), Distance);
	}
}

using namespace GSTNWallEdgeWorld;

// -------------------------------------------------------------------------------------------------------------------------------------
// W3: corners
// -------------------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2ProbeKindsTest, "GSTN.Wall.V2.ProbeAheadClassifiesWhatIsAhead", GSTNWallEdgeWorld::TestFlags)
bool FGSTN_WallV2ProbeKindsTest::RunTest(const FString& Parameters)
{
	for (const FQuat& Frame : Frames())
	{
		const FVector Move(0, 600, 0);
		{
			FEdgeScene Scene(Frame);
			Scene.AddWall();
			Scene.PlaceCapsule(0, 300);
			const FGSTN_WallAheadResult Ahead = Scene.Probe(Move);
			TestEqual(TEXT("Flat wall: nothing ahead"), Ahead.Kind, EGSTN_WallEdgeKind::None);
			Scene.State.Velocity = FVector::ZeroVector;
			TestEqual(TEXT("At rest: nothing looked at"), FGSTN_WallTraverseTestAccess::ProbeAhead(*Scene.Mode, FMovingComponentSet(Scene.Capsule), Scene.State).Kind, EGSTN_WallEdgeKind::None);
		}
		{
			FEdgeScene Scene(Frame);
			Scene.AddConvexCorner(200, 90);
			Scene.PlaceCapsule(126, 300);
			const FGSTN_WallAheadResult Ahead = Scene.Probe(Move);
			TestEqual(TEXT("Pillar edge: convex"), Ahead.Kind, EGSTN_WallEdgeKind::Convex);
			TestTrue(*FString::Printf(TEXT("Far face is at 90 deg (was %.1f)"), Ahead.Angle), FMath::Abs(Ahead.Angle - 90.f) < 1.f);
			TestTrue(*FString::Printf(TEXT("Edge is 40 cm from the leading edge (was %.1f)"), Ahead.Distance), FMath::Abs(Ahead.Distance - 40.f) < 3.f);
		}
		{
			FEdgeScene Scene(Frame);
			Scene.AddConcaveCorner(200, 90);
			Scene.PlaceCapsule(126, 300);
			const FGSTN_WallAheadResult Ahead = Scene.Probe(Move);
			TestEqual(TEXT("Perpendicular wall: concave"), Ahead.Kind, EGSTN_WallEdgeKind::Concave);
			TestTrue(*FString::Printf(TEXT("Corner face at 90 deg (was %.1f)"), Ahead.Angle), FMath::Abs(Ahead.Angle - 90.f) < 1.f);
			TestTrue(*FString::Printf(TEXT("Corner is 40 cm from the leading edge (was %.1f)"), Ahead.Distance), FMath::Abs(Ahead.Distance - 40.f) < 3.f);
		}
		{
			FEdgeScene Scene(Frame);
			Scene.AddWall();
			Scene.AddBox(FVector(0, 215, 250), FVector(5, 255, 350));
			Scene.PlaceCapsule(126, 300);
			TestEqual(TEXT("A 5 cm rib in the look: bump"), Scene.Probe(Move).Kind, EGSTN_WallEdgeKind::Bump);
		}
		{
			FEdgeScene Scene(Frame);
			Scene.AddConvexCorner(200, 10);
			Scene.PlaceCapsule(126, 300);
			const EGSTN_WallEdgeKind Kind = Scene.Probe(Move).Kind;
			TestTrue(TEXT("A 10 deg facet is not a corner"), Kind == EGSTN_WallEdgeKind::Slope || Kind == EGSTN_WallEdgeKind::Bump || Kind == EGSTN_WallEdgeKind::None);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2ConvexCapTest, "GSTN.Wall.V2.ConvexCornerCapsSpeed", GSTNWallEdgeWorld::TestFlags)
bool FGSTN_WallV2ConvexCapTest::RunTest(const FString& Parameters)
{
	constexpr float Edge = 200.f;
	constexpr float Speed = 600.f;
	for (const FQuat& Frame : Frames())
	{
		for (float Angle : { 90.f, 60.f })
		{
			const FGSTN_WallEdgeSettings Edges;
			const float Terminal = FGSTN_WallSlideControlSettings().TerminalDownSpeed;

			{
				// Far from the edge (beyond the look): nothing slows
				FEdgeScene Scene(Frame);
				Scene.AddConvexCorner(Edge, Angle);
				Scene.PlaceCapsule(-300, 300);
				Scene.Tick(FVector(0, Speed, 0));
				TestTrue(*FString::Printf(TEXT("%.0f deg, far: speed kept (was %.1f)"), Angle, Scene.State.Velocity.Size()), FMath::Abs(Scene.State.Velocity.Size() - Speed) < 0.5f);
			}
			{
				// 40 cm before the edge: capped to the approach curve of the corner, not below the corner speed
				FEdgeScene Scene(Frame);
				Scene.AddConvexCorner(Edge, Angle);
				Scene.PlaceCapsule(Edge - CapRadius - 40.f, 300);
				const float Expected = AllowedAt(Edges, EGSTN_WallEdgeKind::Convex, Angle, Terminal, 40.f);
				Scene.Tick(FVector(0, Speed, 0));
				const float After = Scene.State.Velocity.Size();
				TestTrue(*FString::Printf(TEXT("%.0f deg, 40 cm: capped to about %.0f (was %.1f)"), Angle, Expected, After), FMath::Abs(After - Expected) < 25.f);
				TestTrue(TEXT("Below the proposed speed"), After < Speed - 20.f);
				TestTrue(TEXT("Transport kept the velocity on the plane"), FMath::Abs(FVector::DotProduct(Scene.State.Velocity, Scene.State.WallNormal)) < 1.f);
			}
			{
				// At the edge: about the corner speed
				FEdgeScene Scene(Frame);
				Scene.AddConvexCorner(Edge, Angle);
				Scene.PlaceCapsule(Edge - CapRadius, 300);
				const float Corner = UGSTN_WallEdgeUtils::EdgeSpeedCap(Edges, EGSTN_WallEdgeKind::Convex, Angle, Terminal);
				Scene.Tick(FVector(0, Speed, 0));
				const float After = Scene.State.Velocity.Size();
				TestTrue(*FString::Printf(TEXT("%.0f deg, at the edge: about %.0f (was %.1f)"), Angle, Corner, After), After >= Corner - 2.f && After <= Corner + 45.f);
			}
		}
	}

	// A run at the edge with the speed regained every tick (the control model pulling): it brakes on approach along the curve, never under the
	// corner speed, and the whole thing costs a few queries per tick
	{
		FQueryCounting Counting;
		FEdgeScene Scene;
		Scene.AddConvexCorner(Edge, 90.f);
		Scene.PlaceCapsule(-100, 300);
		const FGSTN_WallEdgeSettings Edges;
		const float Corner = UGSTN_WallEdgeUtils::EdgeSpeedCap(Edges, EGSTN_WallEdgeKind::Convex, 90.f, 600.f);
		const FName ModeName(TEXT("V2ConvexRun"));
		float Previous = Speed;
		float Slowest = Speed;
		FVector Proposed(0, Speed, 0);
		for (int32 i = 0; i < 40 && Scene.Pos().Y + CapRadius < Edge; ++i)
		{
			Scene.Tick(Proposed);
			const float Now = Scene.State.Velocity.Size();
			const float DistanceToEdge = Edge - (Scene.Pos().Y + CapRadius);
			TestTrue(*FString::Printf(TEXT("Never faster than the curve at %.0f cm (was %.0f)"), DistanceToEdge, Now),
				Now <= AllowedAt(Edges, EGSTN_WallEdgeKind::Convex, 90.f, 600.f, DistanceToEdge + 12.f) + 2.f);
			Previous = Now;
			Slowest = FMath::Min(Slowest, Now);
			Proposed = NextProposed(Scene, FVector(0, 1, 0), Speed, 980.f);
#if GSTN_WALL_DEBUG
			GSTNWallDebug::FinishTick(ModeName, i * EdgeDt, Now, 0.f);
#endif
		}
		TestTrue(*FString::Printf(TEXT("It slowed down (slowest %.0f, corner speed %.0f)"), Slowest, Corner), Slowest < Speed - 100.f && Slowest >= Corner - 2.f);
		TestTrue(TEXT("It kept the wall all along"), Scene.State.bHasWall && !Scene.State.IsLeaving());
		TestTrue(*FString::Printf(TEXT("The plane did not turn before the edge (%.1f deg)"), Scene.PlaneTurnDeg()), Scene.PlaneTurnDeg() < 1.f);
#if GSTN_WALL_DEBUG
		int32 Ticks;
		double Average;
		int32 Max;
		GSTNWallDebug::GetQueryStats(ModeName, Ticks, Average, Max);
		TestTrue(*FString::Printf(TEXT("Approaching a corner stays within the budget (avg %.2f, max %d)"), Average, Max), Max <= 9);
#endif
		(void)Previous;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2ConcaveCapTest, "GSTN.Wall.V2.ConcaveCornerCapsSpeed", GSTNWallEdgeWorld::TestFlags)
bool FGSTN_WallV2ConcaveCapTest::RunTest(const FString& Parameters)
{
	constexpr float Edge = 200.f;
	constexpr float Speed = 600.f;
	for (const FQuat& Frame : Frames())
	{
		for (float Angle : { 90.f, 60.f })
		{
			const FGSTN_WallEdgeSettings Edges;
			const float Terminal = FGSTN_WallSlideControlSettings().TerminalDownSpeed;

			{
				FEdgeScene Scene(Frame);
				Scene.AddConcaveCorner(Edge, Angle);
				Scene.PlaceCapsule(-300, 300);
				Scene.Tick(FVector(0, Speed, 0));
				TestTrue(*FString::Printf(TEXT("%.0f deg, far: speed kept (was %.1f)"), Angle, Scene.State.Velocity.Size()), FMath::Abs(Scene.State.Velocity.Size() - Speed) < 0.5f);
			}
			{
				// Close to the corner: never faster than the approach curve of the corner angle (its distance is the one the look reports)
				FEdgeScene Scene(Frame);
				Scene.AddConcaveCorner(Edge, Angle);
				Scene.PlaceCapsule(Edge - CapRadius - 40.f, 300);
				const FGSTN_WallAheadResult Ahead = Scene.Probe(FVector(0, Speed, 0));
				TestEqual(TEXT("Seen as an inside corner"), Ahead.Kind, EGSTN_WallEdgeKind::Concave);
				const float Cap = AllowedAt(Edges, EGSTN_WallEdgeKind::Concave, Ahead.Angle, Terminal, Ahead.Distance);
				TestTrue(*FString::Printf(TEXT("The hook returns the curve (%.1f vs %.1f)"), FGSTN_WallTraverseTestAccess::ComputeEdgeSpeedCap(*Scene.Mode, Ahead), Cap),
					FMath::Abs(FGSTN_WallTraverseTestAccess::ComputeEdgeSpeedCap(*Scene.Mode, Ahead) - Cap) < 0.5f);
				Scene.Tick(FVector(0, Speed, 0));
				const float After = Scene.State.Velocity.Size();
				TestTrue(*FString::Printf(TEXT("%.0f deg, 40 cm: never above the cap %.0f (was %.1f)"), Angle, Cap, After), After <= FMath::Min(Cap, Speed) + 0.5f);
				if (Angle >= 85.f)
				{
					TestTrue(*FString::Printf(TEXT("90 deg, 40 cm: really capped (was %.1f)"), After), After < Speed - 20.f);
				}
			}
			{
				// At the corner: the corner speed, plus what the last centimetres allow
				FEdgeScene Scene(Frame);
				Scene.AddConcaveCorner(Edge, Angle);
				Scene.PlaceCapsule(Edge - CapRadius - 4.f, 300);
				const float Corner = UGSTN_WallEdgeUtils::EdgeSpeedCap(Edges, EGSTN_WallEdgeKind::Concave, Angle, Terminal);
				Scene.Tick(FVector(0, Speed, 0));
				const float After = Scene.State.Velocity.Size();
				TestTrue(*FString::Printf(TEXT("%.0f deg, at the corner: about %.0f (was %.1f)"), Angle, Corner, After), After <= Corner + 60.f);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2SmallSlopeTest, "GSTN.Wall.V2.SmallSlopeNoCap", GSTNWallEdgeWorld::TestFlags)
bool FGSTN_WallV2SmallSlopeTest::RunTest(const FString& Parameters)
{
	constexpr float Speed = 600.f;
	for (const FQuat& Frame : Frames())
	{
		for (bool bConvex : { true, false })
		{
			for (float Angle : { 5.f, 10.f })
			{
				FEdgeScene Scene(Frame);
				if (bConvex)
				{
					Scene.AddConvexCorner(200, Angle);
				}
				else
				{
					Scene.AddConcaveCorner(200, Angle);
				}
				Scene.PlaceCapsule(0, 300);

				float Slowest = Speed;
				bool bCapped = false;
				for (int32 i = 0; i < 40; ++i)
				{
					const FGSTN_WallAheadResult Ahead = Scene.Probe(FVector(0, Speed, 0));
					bCapped |= Ahead.Kind == EGSTN_WallEdgeKind::Concave || Ahead.Kind == EGSTN_WallEdgeKind::Convex || Ahead.Kind == EGSTN_WallEdgeKind::Blocked;
					Scene.Tick(FVector(0, Speed, 0));
					Slowest = FMath::Min(Slowest, Scene.State.Velocity.Size());
				}
				const FString What = FString::Printf(TEXT("%s %.0f deg"), bConvex ? TEXT("convex") : TEXT("concave"), Angle);
				TestFalse(*(What + TEXT(": never read as a corner")), bCapped);
				TestTrue(*FString::Printf(TEXT("%s: no slowdown (slowest %.1f)"), *What, Slowest), Slowest > Speed - 1.f);
				TestTrue(*(What + TEXT(": still on the wall")), Scene.State.bHasWall && !Scene.State.IsLeaving());
				TestTrue(*FString::Printf(TEXT("%s: went past the facet (y %.0f)"), *What, Scene.Pos().Y), Scene.Pos().Y > 200.f);
			}
		}
	}
	return true;
}

// -------------------------------------------------------------------------------------------------------------------------------------
// W4: bumps, ledges, recesses
// -------------------------------------------------------------------------------------------------------------------------------------

namespace GSTNWallEdgeWorld
{
	/** Slides sideways at Speed across a 40 cm wide, 100 cm tall bump of the given depth; reports the progress of every tick and the worst plane turn */
	struct FBumpRun
	{
		float MinProgress = TNumericLimits<float>::Max();
		float WorstTurnDeg = 0.f;
		float FinalY = 0.f;
		float FinalGap = 0.f;
		bool bLeft = false;
		bool bLostWall = false;
	};

	static FBumpRun RunSidewaysOverBump(const FQuat& Frame, float Depth, float Speed)
	{
		FBumpRun Run;
		FEdgeScene Scene(Frame);
		Scene.AddWall();
		Scene.AddBox(FVector(0, 300, 250), FVector(Depth, 340, 350));
		Scene.PlaceCapsule(0, 300);

		for (int32 i = 0; i < 80; ++i)
		{
			const float Before = Scene.Pos().Y;
			Scene.Tick(FVector(0, Speed, 0));
			if (Before < 380.f)
			{
				Run.MinProgress = FMath::Min(Run.MinProgress, Scene.Pos().Y - Before);
			}
			Run.WorstTurnDeg = FMath::Max(Run.WorstTurnDeg, Scene.PlaneTurnDeg());
			Run.bLeft |= Scene.State.IsLeaving();
			Run.bLostWall |= !Scene.State.bHasWall;
		}
		Run.FinalY = Scene.Pos().Y;
		Run.FinalGap = Scene.Pos().X - CapRadius;
		return Run;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2BumpStepTest, "GSTN.Wall.V2.BumpSteppedOverWithoutStop", GSTNWallEdgeWorld::TestFlags)
bool FGSTN_WallV2BumpStepTest::RunTest(const FString& Parameters)
{
	constexpr float Speed = 400.f;
	for (const FQuat& Frame : Frames())
	{
		for (float Depth : { 3.f, 5.f, 9.f })
		{
			const FBumpRun Run = RunSidewaysOverBump(Frame, Depth, Speed);
			const FString What = FString::Printf(TEXT("%.0f cm bump"), Depth);
			TestTrue(*FString::Printf(TEXT("%s: crossed (y %.0f)"), *What, Run.FinalY), Run.FinalY > 400.f);
			TestTrue(*FString::Printf(TEXT("%s: no tick lost more than 10%% of its travel (worst %.2f of %.2f)"), *What, Run.MinProgress, Speed * EdgeDt), Run.MinProgress >= 0.9f * Speed * EdgeDt);
			TestFalse(*(What + TEXT(": mode not left")), Run.bLeft);
			TestFalse(*(What + TEXT(": wall never lost")), Run.bLostWall);
			TestTrue(*FString::Printf(TEXT("%s: seated back on the wall afterwards (gap %.2f)"), *What, Run.FinalGap), FMath::Abs(Run.FinalGap - 2.f) < 0.3f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2BumpPlaneTest, "GSTN.Wall.V2.BumpDoesNotTurnPlane", GSTNWallEdgeWorld::TestFlags)
bool FGSTN_WallV2BumpPlaneTest::RunTest(const FString& Parameters)
{
	for (const FQuat& Frame : Frames())
	{
		for (float Depth : { 3.f, 5.f, 9.f })
		{
			const FBumpRun Run = RunSidewaysOverBump(Frame, Depth, 400.f);
			TestTrue(*FString::Printf(TEXT("%.0f cm bump: the proxy plane never turned (worst %.2f deg)"), Depth, Run.WorstTurnDeg), Run.WorstTurnDeg < 1.f);
		}
	}

	// The same bump met from the edge of the capsule path (its side only grazes it): still no turn
	FEdgeScene Scene;
	Scene.AddWall();
	Scene.AddBox(FVector(0, 300, 250), FVector(8, 340, 350));
	Scene.PlaceCapsule(200, 300);
	float Worst = 0.f;
	for (int32 i = 0; i < 60; ++i)
	{
		Scene.Tick(FVector(0, 300, -100));
		Worst = FMath::Max(Worst, Scene.PlaneTurnDeg());
	}
	TestTrue(*FString::Printf(TEXT("Sliding down and sideways across it: the plane stays (worst %.2f deg)"), Worst), Worst < 1.f);
	return true;
}

namespace GSTNWallEdgeWorld
{
	struct FLedgeRun
	{
		bool bLeft = false;
		FName NextMode;
		float EndZ = 0.f;
		bool bHasWall = true;
		float WorstTurnDeg = 0.f;
	};

	/** Slides straight down at Speed onto a ledge of the given depth (wide, 30 cm tall) */
	static FLedgeRun RunDownOntoLedge(const FQuat& Frame, float Depth, float Speed)
	{
		FLedgeRun Run;
		FEdgeScene Scene(Frame);
		Scene.AddWall();
		Scene.AddBox(FVector(0, -150, 0), FVector(Depth, 150, 30));
		Scene.PlaceCapsule(0, 250);
		for (int32 i = 0; i < 90 && !Scene.State.IsLeaving(); ++i)
		{
			Scene.Tick(FVector(0, 0, -Speed));
			Run.WorstTurnDeg = FMath::Max(Run.WorstTurnDeg, Scene.PlaneTurnDeg());
			Run.bHasWall &= Scene.State.bHasWall;
		}
		Run.bLeft = Scene.State.IsLeaving();
		Run.NextMode = Scene.State.NextModeName;
		Run.EndZ = Scene.Pos().Z;
		return Run;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2LedgeFloorTest, "GSTN.Wall.V2.LedgeBelowLandsOnRealFloor", GSTNWallEdgeWorld::TestFlags)
bool FGSTN_WallV2LedgeFloorTest::RunTest(const FString& Parameters)
{
	for (const FQuat& Frame : Frames())
	{
		for (float Depth : { 15.f, 25.f })
		{
			const FLedgeRun Run = RunDownOntoLedge(Frame, Depth, 500.f);
			const FString What = FString::Printf(TEXT("%.0f cm ledge"), Depth);
			TestTrue(*(What + TEXT(": it is a floor, the mode is left")), Run.bLeft);
			TestEqual(*(What + TEXT(": to the floor mode")), Run.NextMode, FName(DefaultModeNames::Walking));
			// The capsule's round bottom rests on the ledge's outer edge, not flat on it: its centre sits that much lower than a flat-bottomed
			// capsule would (a 15 cm ledge lets the hemisphere dip about 7 cm)
			const float EdgeOffset = CapRadius + GetDefault<UGSTN_WallSlideV2MovementMode>()->WallGap - Depth;
			const float RestZ = 30.f + (CapHalfHeight - CapRadius) + FMath::Sqrt(FMath::Max(CapRadius * CapRadius - EdgeOffset * EdgeOffset, 0.f));
			TestTrue(*FString::Printf(TEXT("%s: stopped on top of it (z %.1f, rest %.1f)"), *What, Run.EndZ, RestZ), Run.EndZ > RestZ - 2.f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2SmallLedgeTest, "GSTN.Wall.V2.SmallLedgeKeepsSliding", GSTNWallEdgeWorld::TestFlags)
bool FGSTN_WallV2SmallLedgeTest::RunTest(const FString& Parameters)
{
	for (const FQuat& Frame : Frames())
	{
		for (float Depth : { 3.f, 5.f, 9.f })
		{
			const FLedgeRun Run = RunDownOntoLedge(Frame, Depth, 500.f);
			const FString What = FString::Printf(TEXT("%.0f cm ledge"), Depth);
			TestFalse(*(What + TEXT(": the slide goes on, the mode is not left")), Run.bLeft);
			TestTrue(*FString::Printf(TEXT("%s: slid past it (z %.0f)"), *What, Run.EndZ), Run.EndZ < -100.f);
			TestTrue(*(What + TEXT(": the wall was kept")), Run.bHasWall);
			TestTrue(*FString::Printf(TEXT("%s: the plane did not turn (%.2f deg)"), *What, Run.WorstTurnDeg), Run.WorstTurnDeg < 1.f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2RecessTest, "GSTN.Wall.V2.RecessNoPop", GSTNWallEdgeWorld::TestFlags)
bool FGSTN_WallV2RecessTest::RunTest(const FString& Parameters)
{
	for (const FQuat& Frame : Frames())
	{
		for (float RecessDepth : { 5.f, 15.f })
		{
			FEdgeScene Scene(Frame);
			Scene.AddBox(FVector(-50, -2000, -4000), FVector(0, 300, 4000));
			Scene.AddBox(FVector(-50, 300, -4000), FVector(-RecessDepth, 400, 4000));
			Scene.AddBox(FVector(-50, 400, -4000), FVector(0, 2000, 4000));
			Scene.PlaceCapsule(100, 300);

			const float MaxPull = Scene.Mode->MaxAdhesionSpeed * EdgeDt;
			float WorstStep = 0.f;
			float Deepest = 0.f;
			bool bLost = false;
			for (int32 i = 0; i < 160 && !Scene.State.IsLeaving(); ++i)
			{
				const float Before = Scene.Pos().X;
				Scene.Tick(FVector(0, 300, 0));
				// Moving off the wall (a step over a bump on the far side of the recess) is a lift, instant by design; the pop to avoid is the dive
				WorstStep = FMath::Max(WorstStep, Before - Scene.Pos().X);
				Deepest = FMath::Max(Deepest, CapRadius + Scene.Mode->WallGap - Scene.Pos().X);
				bLost |= !Scene.State.bHasWall;
			}
			const FString What = FString::Printf(TEXT("%.0f cm recess"), RecessDepth);
			TestFalse(*(What + TEXT(": mode not left")), Scene.State.IsLeaving());
			TestFalse(*(What + TEXT(": wall never lost")), bLost);
			TestTrue(*FString::Printf(TEXT("%s: it dives no faster than the pull-in rate (worst %.2f of %.2f)"), *What, WorstStep, MaxPull + 0.5f), WorstStep <= MaxPull + 0.5f);
			TestTrue(*FString::Printf(TEXT("%s: never deeper than the recess (%.2f)"), *What, Deepest), Deepest <= RecessDepth + 0.5f);
			if (RecessDepth <= 5.f)
			{
				TestTrue(*FString::Printf(TEXT("%s: crossed (y %.0f)"), *What, Scene.Pos().Y), Scene.Pos().Y > 450.f);
				TestTrue(*FString::Printf(TEXT("%s: the plane did not turn (%.2f deg)"), *What, Scene.PlaneTurnDeg()), Scene.PlaneTurnDeg() < 1.f);
			}
		}
	}
	return true;
}

// -------------------------------------------------------------------------------------------------------------------------------------
// Control: the stick into the wall
// -------------------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2PushIntoWallTest, "GSTN.Wall.V2.PushIntoWallDoesNotChangeSlide", GSTNWallEdgeWorld::TestFlags)
bool FGSTN_WallV2PushIntoWallTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_WallSlideV2MovementMode> Mode(NewObject<UGSTN_WallSlideV2MovementMode>(GetTransientPackage()));
	for (const FQuat& Frame : Frames())
	{
		const FVector Up = Frame.RotateVector(FVector::UpVector);
		const FVector N = Frame.RotateVector(FVector::ForwardVector);
		const FGSTN_WallMovementPlane Plane = UGSTN_WallMovementUtils::MakeMovementPlane(Frame, N, Up);
		FVector H, V;
		UGSTN_WallControlUtils::MakePlaneAxes(N, Up, H, V);

		// The camera looks into the wall; the camera rotation is the frame's heading turned toward -N
		FCharacterDefaultInputs Inputs;
		Inputs.ControlRotation = FRotationMatrix::MakeFromXZ(-N, Up).Rotator();

		// Forward on the stick = into the wall. It must map to nothing vertical (it used to map to "up" and brake the fall to a crawl)
		const FVector Mapped = FGSTN_WallTraverseTestAccess::MapMoveIntentToWall(*Mode, Inputs, -N, Plane);
		const FVector2D MappedOnPlane = UGSTN_WallControlUtils::ToPlane(Mapped, H, V);
		TestTrue(*FString::Printf(TEXT("Pushing into the wall has no vertical part (was %.3f)"), MappedOnPlane.Y), FMath::Abs(MappedOnPlane.Y) < 1e-3);

		// The slide is the one without input: same velocity
		const FVector Prior = H * 120.f - Up * 500.f;
		const FMoverTickStartData StartState = MakeStartStateWithVelocity(Prior);
		const FVector Idle = FGSTN_WallTraverseTestAccess::ComputeWallVelocity(*Mode, StartState, Plane, FVector::ZeroVector, EdgeDt);
		const FVector Pushing = FGSTN_WallTraverseTestAccess::ComputeWallVelocity(*Mode, StartState, Plane, Mapped, EdgeDt);
		TestTrue(*FString::Printf(TEXT("Pushing into the wall changes nothing (%s vs %s)"), *Pushing.ToString(), *Idle.ToString()), Pushing.Equals(Idle, 0.05));

		// Sideways still steers: right on the stick (camera looking at the wall: along the wall) gives horizontal control
		const FVector Side = FVector::CrossProduct(Up, -N).GetSafeNormal();
		const FVector2D SideOnPlane = UGSTN_WallControlUtils::ToPlane(FGSTN_WallTraverseTestAccess::MapMoveIntentToWall(*Mode, Inputs, Side, Plane), H, V);
		TestTrue(*FString::Printf(TEXT("A sideways stick still controls the horizontal (%.2f)"), SideOnPlane.X), FMath::Abs(SideOnPlane.X) > 0.9f);
	}
	return true;
}

// -------------------------------------------------------------------------------------------------------------------------------------
// Query budget
// -------------------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2QueryBudgetTest, "GSTN.Wall.V2.QueryBudgetFlat", GSTNWallEdgeWorld::TestFlags)
bool FGSTN_WallV2QueryBudgetTest::RunTest(const FString& Parameters)
{
#if GSTN_WALL_DEBUG
	FQueryCounting Counting;
	FEdgeScene Scene;
	Scene.AddWall();
	Scene.PlaceCapsule(0, 3000);

	const FName ModeName(TEXT("V2BudgetFlat"));
	constexpr int32 NumTicks = 120;
	for (int32 i = 0; i < NumTicks; ++i)
	{
		Scene.Tick(FVector(100, 100 * ((i / 30) % 2 ? -1 : 1), -500));
		GSTNWallDebug::FinishTick(ModeName, i * EdgeDt, 500.f, 0.f);
	}

	int32 Ticks;
	double Average;
	int32 Max;
	GSTNWallDebug::GetQueryStats(ModeName, Ticks, Average, Max);
	TestEqual(TEXT("Every tick was counted"), Ticks, NumTicks);
	AddInfo(FString::Printf(TEXT("Measured wall queries per tick on a flat wall: average %.2f, max %d over %d ticks"), Average, Max, Ticks));
	TestTrue(*FString::Printf(TEXT("Average queries per tick <= 3.5 (was %.2f)"), Average), Average <= 3.5);
	TestTrue(*FString::Printf(TEXT("Max queries in a tick <= 9 (was %d)"), Max), Max <= 9);
	TestTrue(TEXT("The wall was held all along"), Scene.State.bHasWall && !Scene.State.IsLeaving());
#else
	AddInfo(TEXT("GSTN_WALL_DEBUG is off: the query counter is compiled out, nothing to check"));
#endif
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
