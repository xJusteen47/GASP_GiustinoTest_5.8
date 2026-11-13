// Tests of the shared wall traverse base and the slide v2 mode on flat walls (wall-slide-v2.md W2): GSTN.Wall.V2.*
//
// There is no Mover pawn here: a full simulation tick needs the Mover component, its rollback blackboard and the sync state
// plumbing, which only the PIE harness has. What runs headless is everything below the tick:
//  - the control model through ComputeWallVelocity (the part GenerateMove puts in the FProposedMove) and the entry velocity,
//  - TraverseStep, the world part of the tick (move sweep, facing yaw, contact + adhesion sweep, proxy plane), in a throwaway game
//    world with a box wall and a capsule, driven by a working state exactly as SimulationTick builds it.
// Not covered here (PIE): Activate, GenerateMove's stick mapping, base riding, the sync state capture and the mode switches.

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
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "WallMovement/GSTN_WallMovementDebug.h"
#include "WallMovement/MoveLibrary/GSTN_WallControlUtils.h"
#include "WallMovement/MoveLibrary/GSTN_WallMovementUtils.h"
#include "WallMovement/Modes/GSTN_WallSlideV2MovementMode.h"
#include "GSTN_WallV2TestAccess.h"

// FGSTN_WallTraverseTestAccess (friend of the v2 modes) lives in the shared header below

namespace GSTNWallV2Test
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Dt = 1.f / 60.f;
	constexpr float Radius = 34.f;
	constexpr float HalfHeight = 88.f;

	static void V2NearlyEq(FAutomationTestBase& Test, const TCHAR* What, double A, double B, double Tolerance)
	{
		Test.TestTrue(FString::Printf(TEXT("%s (%.4f vs %.4f)"), What, A, B), FMath::Abs(A - B) <= Tolerance);
	}

	static TArray<FVector> UpVectors()
	{
		return { FVector(0, 0, 1), FVector(0, 0, -1), FVector(1, 0, 0), FVector(0, -1, 0), FVector(1, 1, 1).GetSafeNormal(), FVector(-2, 1, 3).GetSafeNormal() };
	}

	/** A wall normal perpendicular to Up, tilted by TiltDeg around the wall's horizontal axis */
	static FVector WallNormalFor(const FVector& Up, float TiltDeg)
	{
		const FVector Seed = FMath::Abs(Up.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
		const FVector Side = FVector::CrossProduct(Up, Seed).GetSafeNormal();
		const FVector Axis = FVector::CrossProduct(Side, Up).GetSafeNormal();
		return FQuat(Axis, FMath::DegreesToRadians(TiltDeg)).RotateVector(Side);
	}

	/** A throwaway game world with a box wall (face through the origin, normal +X, or tilted) and a capsule next to it */
	struct FWallV2Scene
	{
		UWorld* World = nullptr;
		UBoxComponent* Wall = nullptr;
		UCapsuleComponent* Capsule = nullptr;
		TStrongObjectPtr<UGSTN_WallSlideV2MovementMode> Mode;
		FGSTN_WallTickState State;
		FMovementRecord MoveRecord;
		FVector WallNormal = FVector::ForwardVector;

		FWallV2Scene()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNWallV2TestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Mode.Reset(NewObject<UGSTN_WallSlideV2MovementMode>(GetTransientPackage()));
			MoveRecord.SetDeltaSeconds(Dt);
		}

		~FWallV2Scene()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		/** The wall: a big box whose front face contains the origin, facing Normal (perpendicular to the Y axis) */
		void MakeWall(const FVector& Normal)
		{
			WallNormal = Normal.GetSafeNormal();
			constexpr double HalfDepth = 50.0;
			AActor* Actor = World->SpawnActor<AActor>();
			Wall = NewObject<UBoxComponent>(Actor, TEXT("Wall"));
			Wall->SetBoxExtent(FVector(HalfDepth, 2000.0, 4000.0));
			Wall->SetMobility(EComponentMobility::Static);
			Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Wall->SetCollisionObjectType(ECC_WorldStatic);
			Wall->SetCollisionResponseToAllChannels(ECR_Block);
			Actor->SetRootComponent(Wall);
			Wall->SetWorldLocationAndRotation(-WallNormal * HalfDepth, FRotationMatrix::MakeFromXZ(WallNormal, FVector::UpVector).ToQuat());
			Wall->RegisterComponent();
		}

		/** The capsule, upright, with its surface Gap from the wall plane (measured at the lower hemisphere, the closest part) at height Z */
		void MakeCapsule(float Gap, float Z, bool bFaceWall = true)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			Capsule = NewObject<UCapsuleComponent>(Actor, TEXT("Capsule"));
			Capsule->InitCapsuleSize(Radius, HalfHeight);
			Capsule->SetMobility(EComponentMobility::Movable);
			Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Capsule->SetCollisionObjectType(ECC_Pawn);
			Capsule->SetCollisionResponseToAllChannels(ECR_Block);
			Actor->SetRootComponent(Capsule);

			// Lower hemisphere center at distance Radius + Gap from the plane through the origin: solve for the offset t along the
			// normal of the point (0, 0, Z), so that dot(LowerCenter, N) = Radius + Gap
			const FVector Up = FVector::UpVector;
			const double LowerCenterHeight = Z - (HalfHeight - Radius);
			const double AlongNormal = (Radius + Gap) - LowerCenterHeight * FVector::DotProduct(Up, WallNormal);
			const FVector Location = FVector(0, 0, Z) + WallNormal * AlongNormal;
			const FQuat Facing = bFaceWall ? UGSTN_WallTraverseModeBase::ComputeFacingQuat(FQuat::Identity, WallNormal, Up)
				: FQuat(Up, FMath::DegreesToRadians(90.0));
			Capsule->SetWorldLocationAndRotation(Location, Facing);
			Capsule->RegisterComponent();

			State = FGSTN_WallTickState();
			State.WallNormal = WallNormal;
			State.GravityUp = Up;
			State.Orientation = Capsule->GetComponentQuat();
			State.bHasWall = true;
		}

		bool IsOverlapping() const
		{
			FCollisionQueryParams Params(NAME_None, false, Capsule->GetOwner());
			return World->OverlapAnyTestByChannel(Capsule->GetComponentLocation(), Capsule->GetComponentQuat(), ECC_Pawn, Capsule->GetCollisionShape(), Params);
		}

		void Tick(const FVector& Velocity)
		{
			State.Velocity = Velocity;
			FGSTN_WallTraverseTestAccess::Step(*Mode, FMovingComponentSet(Capsule), Dt, !Velocity.IsNearlyZero(), State, MoveRecord);
		}

		/** Gap between the capsule surface and the wall plane through the origin along the wall normal (lower hemisphere) */
		double Gap() const
		{
			const FVector LowerCenter = Capsule->GetComponentLocation() - FVector::UpVector * (HalfHeight - Radius);
			return FVector::DotProduct(LowerCenter, WallNormal) - Radius;
		}
	};

	/** A start state whose default sync state carries Velocity: what GenerateMove reads as the prior velocity */
	static FMoverTickStartData MakeStartState(const FVector& Velocity)
	{
		FMoverTickStartData StartState;
		FMoverDefaultSyncState& Sync = StartState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
		Sync.SetTransforms_WorldSpace(FVector::ZeroVector, FRotator::ZeroRotator, Velocity, FVector::ZeroVector, nullptr);
		return StartState;
	}

	static FGSTN_WallMovementPlane MakePlane(const FVector& Normal, const FVector& Up)
	{
		return UGSTN_WallMovementUtils::MakeMovementPlane(FQuat::Identity, Normal, Up);
	}
}

using namespace GSTNWallV2Test;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2EntryTest, "GSTN.Wall.V2.EntryKeepsMomentum", GSTNWallV2Test::TestFlags)
bool FGSTN_WallV2EntryTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_WallSlideV2MovementMode> Mode(NewObject<UGSTN_WallSlideV2MovementMode>(GetTransientPackage()));

	for (const FVector& Up : UpVectors())
	{
		const FVector N = WallNormalFor(Up, 0.f);
		FVector H, V;
		UGSTN_WallControlUtils::MakePlaneAxes(N, Up, H, V);
		// A run along the wall, a fall, and a bit into the wall: the arrival of a character grabbing it mid air
		const FVector In = H * 400.f - Up * 500.f - N * 150.f;

		Mode->Control.EntrySpeedKeep = 1.f;
		const FVector Out = Mode->ComputeEntryVelocity(In, N);
		V2NearlyEq(*this, TEXT("Speed kept"), Out.Size(), In.Size(), 0.1);
		V2NearlyEq(*this, TEXT("On the plane"), FVector::DotProduct(Out, N), 0.0, 0.01);
		// The run and the fall both carry in, in the same proportion: nothing zeroed
		const FVector2D InPlane = UGSTN_WallControlUtils::ToPlane(FVector::VectorPlaneProject(In, N), H, V);
		const FVector2D OutPlane = UGSTN_WallControlUtils::ToPlane(Out, H, V);
		TestTrue(TEXT("Horizontal run survives"), OutPlane.X > 0.f && FMath::Sign(OutPlane.X) == FMath::Sign(InPlane.X));
		TestTrue(TEXT("Vertical fall survives"), OutPlane.Y < 0.f && FMath::Sign(OutPlane.Y) == FMath::Sign(InPlane.Y));
		V2NearlyEq(*this, TEXT("Direction on the plane kept"), (OutPlane.GetSafeNormal() - InPlane.GetSafeNormal()).Size(), 0.0, 1e-3);
	}

	// The tunables of the control settings are the mode's: a half keep halves the speed, the clamp caps it
	const FVector N = FVector::ForwardVector;
	const FVector Fast(0, 600, -1400);
	Mode->Control.EntrySpeedKeep = 0.5f;
	Mode->Control.MaxEntrySpeed = 5000.f;
	V2NearlyEq(*this, TEXT("Half keep"), Mode->ComputeEntryVelocity(Fast, N).Size(), Fast.Size() * 0.5, 0.1);
	Mode->Control.EntrySpeedKeep = 1.f;
	Mode->Control.MaxEntrySpeed = 900.f;
	V2NearlyEq(*this, TEXT("Clamped to MaxEntrySpeed"), Mode->ComputeEntryVelocity(Fast, N).Size(), 900.0, 0.1);
	TestTrue(TEXT("Straight into the wall: no tangent part, no speed"), Mode->ComputeEntryVelocity(FVector(-500, 0, 0), N).IsNearlyZero());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2CapsuleNeverRotatesTest, "GSTN.Wall.V2.CapsuleNeverRotates", GSTNWallV2Test::TestFlags)
bool FGSTN_WallV2CapsuleNeverRotatesTest::RunTest(const FString& Parameters)
{
	// Pure: the facing is a yaw about gravity-up whatever the wall and the gravity
	for (const FVector& Up : UpVectors())
	{
		for (float Tilt : { 0.f, 15.f, -20.f, 30.f })
		{
			const FVector N = WallNormalFor(Up, Tilt);
			const FQuat Facing = UGSTN_WallTraverseModeBase::ComputeFacingQuat(FQuat::Identity, N, Up);
			V2NearlyEq(*this, TEXT("Facing up is gravity-up"), FVector::DotProduct(Facing.GetUpVector(), Up), 1.0, 1e-4);
			const FVector Flat = FVector::VectorPlaneProject(-N, Up).GetSafeNormal();
			V2NearlyEq(*this, TEXT("Facing looks at the wall"), FVector::DotProduct(Facing.GetForwardVector(), Flat), 1.0, 1e-4);
		}
	}
	// A floor/ceiling normal has no heading: the capsule keeps the one it has, still upright
	const FQuat Current(FVector::UpVector, FMath::DegreesToRadians(40.0));
	const FQuat OnCeiling = UGSTN_WallTraverseModeBase::ComputeFacingQuat(Current, -FVector::UpVector, FVector::UpVector);
	V2NearlyEq(*this, TEXT("Ceiling: upright"), FVector::DotProduct(OnCeiling.GetUpVector(), FVector::UpVector), 1.0, 1e-4);
	V2NearlyEq(*this, TEXT("Ceiling: heading kept"), FVector::DotProduct(OnCeiling.GetForwardVector(), Current.GetForwardVector()), 1.0, 1e-4);

	// In a world: sliding down a vertical wall and a wall leaning back, starting with the wrong heading; the capsule's up never moves
	for (float LeanDeg : { 0.f, 15.f })
	{
		FWallV2Scene Scene;
		const FVector Normal = FQuat(FVector::RightVector, -FMath::DegreesToRadians(LeanDeg)).RotateVector(FVector::ForwardVector);
		Scene.MakeWall(Normal);
		Scene.MakeCapsule(4.f, 300.f, /*bFaceWall*/ false);
		if (Scene.IsOverlapping())
		{
			AddWarning(TEXT("Test pose overlaps the wall, scenario skipped"));
			continue;
		}

		FVector PlaneH, PlaneV;
		UGSTN_WallControlUtils::MakePlaneAxes(Normal, FVector::UpVector, PlaneH, PlaneV);
		float WorstUp = 1.f;
		for (int32 i = 0; i < 90; ++i)
		{
			Scene.Tick(-PlaneV * 300.f + PlaneH * 100.f);
			WorstUp = FMath::Min(WorstUp, static_cast<float>(FVector::DotProduct(Scene.Capsule->GetUpVector(), FVector::UpVector)));
		}
		TestTrue(*FString::Printf(TEXT("Lean %.0f: the capsule up stays gravity-up all along (worst dot %.6f)"), LeanDeg, WorstUp), WorstUp > 0.99999f);
		const FVector Flat = FVector::VectorPlaneProject(-Normal, FVector::UpVector).GetSafeNormal();
		TestTrue(*FString::Printf(TEXT("Lean %.0f: the facing yaw ends on the wall"), LeanDeg), FVector::DotProduct(Scene.Capsule->GetForwardVector(), Flat) > 0.99);
		TestTrue(*FString::Printf(TEXT("Lean %.0f: a wall was found"), LeanDeg), Scene.State.bHasWall);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2OneSweepTest, "GSTN.Wall.V2.OneMoveSweepPerTickOnFlatWall", GSTNWallV2Test::TestFlags)
bool FGSTN_WallV2OneSweepTest::RunTest(const FString& Parameters)
{
#if GSTN_WALL_DEBUG
	IConsoleVariable* CountCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("gstn.Wall.CountQueries"));
	if (!TestNotNull(TEXT("gstn.Wall.CountQueries exists"), CountCVar))
	{
		return false;
	}
	const int32 PreviousCount = CountCVar->GetInt();
	CountCVar->Set(1, ECVF_SetByCode);
	GSTNWallDebug::ResetQueryStats();

	const FName ModeName(TEXT("V2Test"));
	{
		FWallV2Scene Scene;
		Scene.MakeWall(FVector::ForwardVector);
		Scene.MakeCapsule(Scene.Mode->WallGap, 300.f);

		constexpr int32 NumTicks = 60;
		for (int32 i = 0; i < NumTicks; ++i)
		{
			Scene.Tick(FVector(0, 120, -500));
			GSTNWallDebug::FinishTick(ModeName, i * Dt, 500.f, 0.f);
		}

		int32 Ticks;
		double Average;
		int32 Max;
		GSTNWallDebug::GetQueryStats(ModeName, Ticks, Average, Max);
		TestEqual(TEXT("Every tick was counted"), Ticks, NumTicks);
		// One move sweep + one contact/adhesion sweep + one look-ahead line trace (W3): the flat wall costs three queries, no probe fan, no substeps
		TestTrue(*FString::Printf(TEXT("Average queries per tick on a flat wall is about 3 (was %.2f)"), Average), Average >= 2.9 && Average <= 3.2);
		TestTrue(*FString::Printf(TEXT("At most 4 in the worst tick (was %d)"), Max), Max <= 4);
		TestTrue(TEXT("The wall was held all along"), Scene.State.bHasWall);
	}

	CountCVar->Set(PreviousCount, ECVF_SetByCode);
	GSTNWallDebug::ResetQueryStats();
#else
	AddInfo(TEXT("GSTN_WALL_DEBUG is off: the query counter is compiled out, nothing to check"));
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2AdhesionTest, "GSTN.Wall.V2.AdhesionKeepsGap", GSTNWallV2Test::TestFlags)
bool FGSTN_WallV2AdhesionTest::RunTest(const FString& Parameters)
{
#if GSTN_WALL_DEBUG
	IConsoleVariable* CountCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("gstn.Wall.CountQueries"));
	const int32 PreviousCount = CountCVar ? CountCVar->GetInt() : 0;
	if (CountCVar)
	{
		CountCVar->Set(1, ECVF_SetByCode);
	}
#endif

	// Too far (a recess): pulled in by at most MaxAdhesionSpeed * dt per tick, to the gap, never past it, and the pull-in is not swept
	{
		FWallV2Scene Scene;
		Scene.MakeWall(FVector::ForwardVector);
		const float WallGap = Scene.Mode->WallGap;
		Scene.MakeCapsule(WallGap + 10.f, 300.f);

		const double MaxStep = Scene.Mode->MaxAdhesionSpeed * Dt;
		double Previous = Scene.Gap();
		for (int32 i = 0; i < 4; ++i)
		{
#if GSTN_WALL_DEBUG
			GSTNWallDebug::FinishTick(TEXT("V2AdhesionReset"), 0.0, 0.f, 0.f);
#endif
			Scene.Tick(FVector::ZeroVector);
			const double Gap = Scene.Gap();
			TestTrue(*FString::Printf(TEXT("Pull-in tick %d: closer, by no more than the rate limit (%.3f -> %.3f)"), i, Previous, Gap), Gap <= Previous + 1e-3 && Previous - Gap <= MaxStep + 0.05);
			TestTrue(TEXT("Never past the gap"), Gap >= WallGap - 0.05);
#if GSTN_WALL_DEBUG
			if (Previous - Gap > 0.1)
			{
				TestEqual(TEXT("A pull-in tick costs one query (the unswept pull-in adds none)"), GSTNWallDebug::GetTickQueryCount(), 1);
			}
#endif
			Previous = Gap;
		}
		V2NearlyEq(*this, TEXT("Settled at WallGap"), Previous, WallGap, 0.1);
	}

	// Too close: pushed out to the gap at once
	{
		FWallV2Scene Scene;
		Scene.MakeWall(FVector::ForwardVector);
		const float WallGap = Scene.Mode->WallGap;
		Scene.MakeCapsule(0.5f, 300.f);
		Scene.Tick(FVector::ZeroVector);
		V2NearlyEq(*this, TEXT("Pushed out to WallGap in one tick"), Scene.Gap(), WallGap, 0.1);
	}

	// Moving along the wall holds the gap too
	{
		FWallV2Scene Scene;
		Scene.MakeWall(FVector::ForwardVector);
		const float WallGap = Scene.Mode->WallGap;
		Scene.MakeCapsule(WallGap, 300.f);
		for (int32 i = 0; i < 60; ++i)
		{
			Scene.Tick(FVector(0, 200, -400));
		}
		V2NearlyEq(*this, TEXT("Gap after a second of sliding"), Scene.Gap(), WallGap, 0.15);
	}

#if GSTN_WALL_DEBUG
	if (CountCVar)
	{
		CountCVar->Set(PreviousCount, ECVF_SetByCode);
	}
	GSTNWallDebug::ResetQueryStats();
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2ProposedMoveTest, "GSTN.Wall.V2.ProposedMoveCarriesControlVelocity", GSTNWallV2Test::TestFlags)
bool FGSTN_WallV2ProposedMoveTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_WallSlideV2MovementMode> Mode(NewObject<UGSTN_WallSlideV2MovementMode>(GetTransientPackage()));
	const FVector Up = FVector::UpVector;
	const FVector N = FVector::ForwardVector;
	const FGSTN_WallMovementPlane Plane = MakePlane(N, Up);
	FVector H, V;
	UGSTN_WallControlUtils::MakePlaneAxes(N, Up, H, V);

	// Arriving faster than terminal, running sideways, no input: momentum decays, it is not cut
	const FVector Prior = H * 300.f - Up * 800.f;
	const FMoverTickStartData StartState = MakeStartState(Prior);
	const FVector Coast = FGSTN_WallTraverseTestAccess::ComputeWallVelocity(*Mode, StartState, Plane, FVector::ZeroVector, Dt);
	const FVector2D CoastPlane = UGSTN_WallControlUtils::ToPlane(Coast, H, V);
	V2NearlyEq(*this, TEXT("Horizontal momentum decays slowly"), CoastPlane.X, 300.0 * FMath::Exp(-Mode->Control.HorizontalDrag * Dt), 0.05);
	V2NearlyEq(*this, TEXT("Entry fall decays by gravity, not clamped to terminal"), CoastPlane.Y, -(800.0 - Mode->Control.GravityAcceleration * Mode->Control.GravityScale * Dt), 0.05);
	V2NearlyEq(*this, TEXT("Nothing along the normal"), FVector::DotProduct(Coast, N), 0.0, 1e-3);

	// It is the control model, step for step
	const FVector2D Expected = UGSTN_WallControlUtils::StepSlideVelocity(Mode->Control, UGSTN_WallControlUtils::ToPlane(Prior, H, V), FVector2D::ZeroVector, false, Dt);
	V2NearlyEq(*this, TEXT("Same as StepSlideVelocity (H)"), CoastPlane.X, Expected.X, 1e-3);
	V2NearlyEq(*this, TEXT("Same as StepSlideVelocity (V)"), CoastPlane.Y, Expected.Y, 1e-3);

	// The stick up against the fall brakes hard; the stick along the run adds a little
	const FVector Braked = FGSTN_WallTraverseTestAccess::ComputeWallVelocity(*Mode, StartState, Plane, Up, Dt);
	V2NearlyEq(*this, TEXT("Braking strips VerticalBrakeDecel * dt"), UGSTN_WallControlUtils::ToPlane(Braked, H, V).Y, -(800.0 - Mode->Control.VerticalBrakeDecel * Dt), 0.05);
	TestTrue(TEXT("Braking is stronger than coasting"), UGSTN_WallControlUtils::ToPlane(Braked, H, V).Y > CoastPlane.Y);
	const FVector Pushed = FGSTN_WallTraverseTestAccess::ComputeWallVelocity(*Mode, StartState, Plane, H, Dt);
	V2NearlyEq(*this, TEXT("Control along the run is small"), UGSTN_WallControlUtils::ToPlane(Pushed, H, V).X - CoastPlane.X, Mode->Control.HorizontalControlAccel * Dt, 0.05);

	// The proposed velocity is what the tick moves by (layered and queued moves would already be in it)
	FWallV2Scene Scene;
	Scene.MakeWall(N);
	Scene.MakeCapsule(Scene.Mode->WallGap, 300.f);
	const FVector Before = Scene.Capsule->GetComponentLocation();
	Scene.Tick(Coast);
	const FVector Moved = Scene.Capsule->GetComponentLocation() - Before;
	const FVector Wanted = Coast * Dt;
	V2NearlyEq(*this, TEXT("Moved along the wall by velocity * dt (H)"), FVector::DotProduct(Moved, H), FVector::DotProduct(Wanted, H), 0.05);
	V2NearlyEq(*this, TEXT("Moved along the wall by velocity * dt (V)"), FVector::DotProduct(Moved, V), FVector::DotProduct(Wanted, V), 0.05);
	V2NearlyEq(*this, TEXT("Speed out equals speed in (transport is length preserving, no friction term)"), Scene.State.Velocity.Size(), Coast.Size(), 0.1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
