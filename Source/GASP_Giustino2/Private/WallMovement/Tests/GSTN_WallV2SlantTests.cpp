// Tests of the wall slide v2 on inclined walls and overhangs: GSTN.Wall.V2.* (owner request 2026-10-24: "the wall slide must also work on more or
// less inclined walls").
//
// Same headless approach as GSTN_WallV2EdgeTests.cpp: TraverseStep (the world part of the tick) in a throwaway game world with box geometry. The
// scene is built in WALL coordinates (x along the wall normal, y sideways along the wall, z "up the wall", the tangent that gravity-up projects to),
// so the geometry of the flat-wall tests carries over unchanged, and the wall is leaned by BetaDeg, the angle between its normal and gravity-up:
// 90 = vertical, 60/70/80 = an inclined wall (the normal tilts up, the capsule touches with its LOWER hemisphere), 100/110 = an overhang (the normal
// tilts down, UPPER hemisphere). The capsule is always upright with respect to gravity, as in the mode. The whole scene is then carried through the
// gravity frames of the other wall tests, so "down the wall" is never world Z.
// Not covered here (PIE): the full simulation tick (Activate, GenerateMove's Mover gravity, base riding, the sync state, the mode switches).

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "MoveLibrary/MovementRecord.h"
#include "MoveLibrary/MovementUtilsTypes.h"
#include "MoverSimulationTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "WallMovement/Modes/GSTN_WallSlideV2MovementMode.h"
#include "WallMovement/MoveLibrary/GSTN_WallControlUtils.h"
#include "WallMovement/MoveLibrary/GSTN_WallEdgeUtils.h"
#include "WallMovement/MoveLibrary/GSTN_WallQueryUtils.h"
#include "WallMovement/Transitions/GSTN_WallEntryTransition.h"
#include "GSTN_WallV2TestAccess.h"

// Friend of the entry transition for the headless tests (one definition: the class befriends this name)
struct FGSTN_WallEntryTestAccess
{
	static bool SweepForWall(const UGSTN_WallEntryTransition& Transition, const FSimulationTickParams& Params, const FVector& Direction, const FVector& GravityUp, FHitResult& OutHit)
	{
		return Transition.SweepForWall(Params, Direction, GravityUp, OutHit);
	}
};

namespace GSTNWallSlantWorld
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float SlantDt = 1.f / 60.f;
	constexpr float SlantCapRadius = 34.f;
	constexpr float SlantCapHalfHeight = 88.f;
	/** Depth and width of the second face of a corner wedge (cm) */
	constexpr float SlantWedgeDepth = 60.f;
	constexpr float SlantWedgeWidth = 200.f;
	constexpr float SlantWorldGravity = 980.f;

	/** The gravity directions the scenes are run under: the canonical frame, upside down and two skewed ones */
	static TArray<FQuat> SlantFrames()
	{
		return {
			FQuat::Identity,
			FQuat(FVector::XAxisVector, PI),
			FQuat::FindBetweenNormals(FVector::UpVector, FVector(1, 1, 1).GetSafeNormal()),
			FQuat::FindBetweenNormals(FVector::UpVector, FVector(-2, 1, 3).GetSafeNormal()),
		};
	}

	static void SlantNearlyEq(FAutomationTestBase& Test, const FString& What, double A, double B, double Tolerance)
	{
		Test.TestTrue(FString::Printf(TEXT("%s (%.4f vs %.4f)"), *What, A, B), FMath::Abs(A - B) <= Tolerance);
	}

	/**
	 * A throwaway game world holding box geometry, one upright capsule and a working state, on a wall leaned BetaDeg from gravity-up.
	 * Coordinates passed in and read out are WALL coordinates unless the name says World; Frame turns the canonical gravity (up +Z) into the
	 * gravity the physics runs under
	 */
	struct FSlantScene
	{
		UWorld* World = nullptr;
		FQuat Frame = FQuat::Identity;
		/** Wall coordinates -> the canonical gravity frame: X = wall normal, Y = along the wall, Z = up the wall */
		FQuat Slant = FQuat::Identity;
		float BetaDeg = 90.f;
		UCapsuleComponent* Capsule = nullptr;
		TStrongObjectPtr<UGSTN_WallSlideV2MovementMode> Mode;
		FGSTN_WallTickState State;
		FMovementRecord MoveRecord;

		FSlantScene(float InBetaDeg, const FQuat& InFrame = FQuat::Identity)
			: Frame(InFrame)
			, BetaDeg(InBetaDeg)
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNWallSlantTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Mode.Reset(NewObject<UGSTN_WallSlideV2MovementMode>(GetTransientPackage()));
			MoveRecord.SetDeltaSeconds(SlantDt);

			const float Beta = FMath::DegreesToRadians(BetaDeg);
			const FVector Normal(FMath::Sin(Beta), 0.f, FMath::Cos(Beta));
			const FVector UpTheWall(-FMath::Cos(Beta), 0.f, FMath::Sin(Beta));
			Slant = FRotationMatrix::MakeFromXZ(Normal, UpTheWall).ToQuat();
		}

		~FSlantScene()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		FVector ToWorld(const FVector& Wall) const { return Frame.RotateVector(Slant.RotateVector(Wall)); }
		FVector ToWall(const FVector& WorldVector) const { return Slant.UnrotateVector(Frame.UnrotateVector(WorldVector)); }
		FVector Up() const { return Frame.RotateVector(FVector::UpVector); }
		FVector Normal() const { return ToWorld(FVector::ForwardVector); }
		FVector Gravity() const { return -Up() * SlantWorldGravity; }

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
			Box->SetWorldLocationAndRotation(ToWorld(Center), Frame * Slant * FQuat(FVector::UpVector, FMath::DegreesToRadians(YawDeg)));
			Box->RegisterComponent();
		}

		/** Axis-aligned (in wall coordinates) box from two corners */
		void AddBox(const FVector& Min, const FVector& Max)
		{
			AddBoxCentered((Min + Max) * 0.5, (Max - Min) * 0.5, 0.f);
		}

		/** A box that is aligned with gravity, not with the wall: Center and HalfExtent in the canonical gravity frame (a floor, a ceiling) */
		void AddGravityBox(const FVector& Center, const FVector& HalfExtent)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("GravityGeometry"));
			Box->SetBoxExtent(HalfExtent);
			Box->SetMobility(EComponentMobility::Static);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Box->SetCollisionObjectType(ECC_WorldStatic);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Actor->SetRootComponent(Box);
			Box->SetWorldLocationAndRotation(Frame.RotateVector(Center), Frame);
			Box->RegisterComponent();
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
			AddBoxCentered(FVector(0, Edge, 0) + T2 * SlantWedgeWidth - N2 * SlantWedgeDepth, FVector(SlantWedgeDepth, SlantWedgeWidth, 4000), AngleDeg);
		}

		/** An inside edge at y = Edge: a second surface rises toward the character, turned by AngleDeg from the wall (90 = a perpendicular wall) */
		void AddConcaveCorner(float Edge, float AngleDeg)
		{
			AddWall();
			const float A = FMath::DegreesToRadians(AngleDeg);
			const FVector N2(FMath::Cos(A), -FMath::Sin(A), 0), T2(FMath::Sin(A), FMath::Cos(A), 0);
			AddBoxCentered(FVector(0, Edge, 0) + T2 * SlantWedgeWidth - N2 * SlantWedgeDepth, FVector(SlantWedgeDepth, SlantWedgeWidth, 4000), -AngleDeg);
		}

		/** How far the capsule's center is from the plane when its surface touches it: the capsule's extent along the wall normal */
		float Support() const
		{
			return UGSTN_WallEdgeUtils::CapsuleSupportAlong(SlantCapRadius, SlantCapHalfHeight, Capsule ? Capsule->GetComponentQuat() : FQuat::Identity, Normal());
		}

		/** The upright capsule, facing the wall, seated Gap (default: the mode's WallGap) from the plane at wall coordinates (Y, S) */
		void PlaceCapsule(float Y, float S, float Gap = -1.f)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			Capsule = NewObject<UCapsuleComponent>(Actor, TEXT("Capsule"));
			Capsule->InitCapsuleSize(SlantCapRadius, SlantCapHalfHeight);
			Capsule->SetMobility(EComponentMobility::Movable);
			Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Capsule->SetCollisionObjectType(ECC_Pawn);
			Capsule->SetCollisionResponseToAllChannels(ECR_Block);
			Actor->SetRootComponent(Capsule);

			const float UseGap = Gap >= 0.f ? Gap : Mode->WallGap;
			const FQuat Facing = UGSTN_WallTraverseModeBase::ComputeFacingQuat(FQuat::Identity, Normal(), Up());
			// The support depends on the capsule's up, which is the facing's up: read it from the pose before it is placed
			const float SupportOnNormal = UGSTN_WallEdgeUtils::CapsuleSupportAlong(SlantCapRadius, SlantCapHalfHeight, Facing, Normal());
			Capsule->SetWorldLocationAndRotation(ToWorld(FVector(SupportOnNormal + UseGap, Y, S)), Facing);
			Capsule->RegisterComponent();

			State = FGSTN_WallTickState();
			State.WallNormal = Normal();
			State.WallContactPoint = ToWorld(FVector(0, Y, S));
			State.GravityUp = Up();
			State.Orientation = Capsule->GetComponentQuat();
			State.bHasWall = true;
		}

		bool IsOverlapping() const
		{
			FCollisionQueryParams Params(NAME_None, false, Capsule->GetOwner());
			return World->OverlapAnyTestByChannel(Capsule->GetComponentLocation(), Capsule->GetComponentQuat(), ECC_Pawn, Capsule->GetCollisionShape(), Params);
		}

		/** Wall coordinates of the capsule's center */
		FVector Pos() const { return ToWall(Capsule->GetComponentLocation()); }
		/** Height along gravity-up */
		float Height() const { return static_cast<float>(FVector::DotProduct(Capsule->GetComponentLocation(), Up())); }
		/** Gap between the capsule's surface and the wall plane, along the wall normal (lower or upper hemisphere, whichever touches) */
		double Gap() const { return Pos().X - Support(); }
		float PlaneTurnDeg() const { return UGSTN_WallEdgeUtils::AngleBetweenNormalsDeg(State.WallNormal.GetSafeNormal(), Normal()); }

		void TickWorld(const FVector& WorldVelocity)
		{
			State.Velocity = WorldVelocity;
			FGSTN_WallTraverseTestAccess::Step(*Mode, FMovingComponentSet(Capsule), SlantDt, !WorldVelocity.IsNearlyZero(), State, MoveRecord);
		}

		void Tick(const FVector& WallVelocity) { TickWorld(ToWorld(WallVelocity)); }

		FGSTN_WallAheadResult Probe(const FVector& WallVelocity)
		{
			State.Velocity = ToWorld(WallVelocity);
			return FGSTN_WallTraverseTestAccess::ProbeAhead(*Mode, FMovingComponentSet(Capsule), State);
		}
	};

	/** What a slide run measured */
	struct FSlantSlideRun
	{
		bool bOverlapped = false;
		bool bLeft = false;
		bool bLostWall = false;
		float WorstUp = 1.f;
		double WorstGapError = 0.0;
		float PlaneTurnDeg = 0.f;
		double WorstNormalSpeed = 0.0;
		/** The capsule's displacement and the sum of the velocities handed to the ticks, times dt */
		FVector Moved = FVector::ZeroVector;
		FVector Expected = FVector::ZeroVector;
		/** Displacement along "down the wall" (-V of the plane) */
		double DownTheWall = 0.0;
		double FinalDownSpeed = 0.0;
	};

	/**
	 * Slides from rest at wall (Y, S) = (0, 0) with the control model of the mode: StepSlideVelocity on the slope-adapted settings, gravity -Up *
	 * SlantWorldGravity, an initial run along the wall of RunSpeed. Everything the model gives is fed to TraverseStep, as GenerateMove does for the tick
	 */
	static FSlantSlideRun RunSlantSlide(float BetaDeg, const FQuat& Frame, int32 NumTicks, float RunSpeed)
	{
		FSlantSlideRun Run;
		FSlantScene Scene(BetaDeg, Frame);
		Scene.AddWall();
		Scene.PlaceCapsule(0, 0);

		FVector H, V;
		UGSTN_WallControlUtils::MakePlaneAxes(Scene.Normal(), Scene.Up(), H, V);
		const FGSTN_WallSlideControlSettings Settings = UGSTN_WallControlUtils::AdaptToSlope(Scene.Mode->Control, Scene.Gravity(), V);

		const FVector Start = Scene.Capsule->GetComponentLocation();
		FVector Velocity = H * RunSpeed;
		for (int32 i = 0; i < NumTicks; ++i)
		{
			const FVector2D Next = UGSTN_WallControlUtils::StepSlideVelocity(Settings, UGSTN_WallControlUtils::ToPlane(Velocity, H, V), FVector2D::ZeroVector, false, SlantDt);
			Velocity = UGSTN_WallControlUtils::FromPlane(Next, H, V);
			Scene.TickWorld(Velocity);
			Run.Expected += Velocity * SlantDt;
			Run.WorstUp = FMath::Min(Run.WorstUp, static_cast<float>(FVector::DotProduct(Scene.Capsule->GetUpVector(), Scene.Up())));
			Run.WorstGapError = FMath::Max(Run.WorstGapError, FMath::Abs(Scene.Gap() - Scene.Mode->WallGap));
			Run.WorstNormalSpeed = FMath::Max(Run.WorstNormalSpeed, FMath::Abs(FVector::DotProduct(Scene.State.Velocity, Scene.Normal())));
			Run.bLeft |= Scene.State.IsLeaving();
			Run.bLostWall |= !Scene.State.bHasWall;
		}
		Run.PlaneTurnDeg = Scene.PlaneTurnDeg();
		Run.bOverlapped = Scene.IsOverlapping();
		Run.Moved = Scene.Capsule->GetComponentLocation() - Start;
		Run.DownTheWall = FVector::DotProduct(Run.Moved, -V);
		Run.FinalDownSpeed = FVector::DotProduct(Scene.State.Velocity, -V);
		return Run;
	}

	static void ExpectGoodSlantSlide(FAutomationTestBase& Test, const FString& What, const FSlantSlideRun& Run, float WallGapTolerance = 0.3f)
	{
		Test.TestFalse(*(What + TEXT(": the capsule never overlaps the wall")), Run.bOverlapped);
		Test.TestFalse(*(What + TEXT(": the mode is not left")), Run.bLeft);
		Test.TestFalse(*(What + TEXT(": the wall is never lost")), Run.bLostWall);
		Test.TestTrue(*FString::Printf(TEXT("%s: the capsule up stays gravity-up (worst dot %.6f)"), *What, Run.WorstUp), Run.WorstUp > 0.99999f);
		Test.TestTrue(*FString::Printf(TEXT("%s: the gap holds at WallGap all along (worst error %.3f)"), *What, Run.WorstGapError), Run.WorstGapError <= WallGapTolerance);
		Test.TestTrue(*FString::Printf(TEXT("%s: the proxy plane does not turn (%.2f deg)"), *What, Run.PlaneTurnDeg), Run.PlaneTurnDeg < 1.f);
		Test.TestTrue(*FString::Printf(TEXT("%s: the velocity stays on the plane (%.3f along the normal)"), *What, Run.WorstNormalSpeed), Run.WorstNormalSpeed < 1.0);
		Test.TestTrue(*FString::Printf(TEXT("%s: the capsule moved by the velocity handed to the ticks (off by %.2f cm)"), *What, (Run.Moved - Run.Expected).Size()),
			(Run.Moved - Run.Expected).Size() <= 1.0);
	}

	static float SlantAllowedAt(const FGSTN_WallEdgeSettings& Edges, EGSTN_WallEdgeKind Kind, float Angle, float Reference, float Distance)
	{
		return UGSTN_WallEdgeUtils::AllowedSpeedAtDistance(Edges, UGSTN_WallEdgeUtils::EdgeSpeedCap(Edges, Kind, Angle, Reference), Distance);
	}
}

using namespace GSTNWallSlantWorld;

// -------------------------------------------------------------------------------------------------------------------------------------
// Sliding down inclined walls and overhangs
// -------------------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2SlantSlidesTest, "GSTN.Wall.V2.SlantedWallSlidesDown", GSTNWallSlantWorld::TestFlags)
bool FGSTN_WallV2SlantSlidesTest::RunTest(const FString& Parameters)
{
	for (const FQuat& Frame : SlantFrames())
	{
		for (float Beta : { 60.f, 70.f, 80.f })
		{
			const FSlantSlideRun Run = RunSlantSlide(Beta, Frame, 120, 80.f);
			const FString What = FString::Printf(TEXT("Wall leaning back %.0f deg"), Beta);
			ExpectGoodSlantSlide(*this, What, Run);
			TestTrue(*FString::Printf(TEXT("%s: it slides down the wall (%.0f cm) and speeds up (%.0f cm/s)"), *What, Run.DownTheWall, Run.FinalDownSpeed),
				Run.DownTheWall > 100.0 && Run.FinalDownSpeed > 100.0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2OverhangTest, "GSTN.Wall.V2.OverhangKeepsContact", GSTNWallSlantWorld::TestFlags)
bool FGSTN_WallV2OverhangTest::RunTest(const FString& Parameters)
{
	for (const FQuat& Frame : SlantFrames())
	{
		for (float Beta : { 100.f, 110.f })
		{
			const FString What = FString::Printf(TEXT("Overhang %.0f deg"), Beta);
			const FSlantSlideRun Run = RunSlantSlide(Beta, Frame, 120, 80.f);
			ExpectGoodSlantSlide(*this, What, Run);
			TestTrue(*FString::Printf(TEXT("%s: it slides down along the underside (%.0f cm)"), *What, Run.DownTheWall), Run.DownTheWall > 100.0);

			// The contact is the upper hemisphere: a capsule that starts too far under the surface is pulled in at the adhesion rate and holds
			FSlantScene Scene(Beta, Frame);
			Scene.AddWall();
			Scene.PlaceCapsule(0, 0, Scene.Mode->WallGap + 8.f);
			TestTrue(*(What + TEXT(": the nearest part of the capsule is its upper hemisphere")), FVector::DotProduct(Scene.Normal(), Scene.Up()) < 0.f);
			const double MaxStep = Scene.Mode->MaxAdhesionSpeed * SlantDt;
			double Previous = Scene.Gap();
			for (int32 i = 0; i < 8; ++i)
			{
				Scene.Tick(FVector::ZeroVector);
				const double Gap = Scene.Gap();
				TestTrue(*FString::Printf(TEXT("%s, pull-in tick %d: closer by no more than the rate limit (%.3f -> %.3f)"), *What, i, Previous, Gap),
					Gap <= Previous + 1e-3 && Previous - Gap <= MaxStep + 0.05);
				Previous = Gap;
			}
			SlantNearlyEq(*this, What + TEXT(": settled at WallGap"), Previous, Scene.Mode->WallGap, 0.15);
			TestTrue(*(What + TEXT(": the wall is held")), Scene.State.bHasWall && !Scene.State.IsLeaving());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2SlantSlowerTest, "GSTN.Wall.V2.SlantSlowerThanVertical", GSTNWallSlantWorld::TestFlags)
bool FGSTN_WallV2SlantSlowerTest::RunTest(const FString& Parameters)
{
	// Pure: "down the wall" is gravity projected on the plane, under any gravity direction
	for (const FQuat& Frame : SlantFrames())
	{
		const FVector Up = Frame.RotateVector(FVector::UpVector);
		const FVector Gravity = -Up * SlantWorldGravity;
		const FGSTN_WallSlideControlSettings Defaults;

		double Previous = 0.0;
		double VerticalDistance = 0.0;
		TArray<TPair<float, double>> Distances;
		for (float Beta : { 60.f, 70.f, 80.f, 90.f })
		{
			const float SinBeta = FMath::Sin(FMath::DegreesToRadians(Beta));
			const FVector N = Frame.RotateVector(FVector(SinBeta, 0.f, FMath::Cos(FMath::DegreesToRadians(Beta))));
			FVector H, V;
			UGSTN_WallControlUtils::MakePlaneAxes(N, Up, H, V);

			const FString What = FString::Printf(TEXT("Beta %.0f"), Beta);
			SlantNearlyEq(*this, What + TEXT(": gravity along the plane is sin(beta) of it"), UGSTN_WallControlUtils::GravityAlongPlane(Gravity, V), SlantWorldGravity * SinBeta, 0.05);
			const FGSTN_WallSlideControlSettings Adapted = UGSTN_WallControlUtils::AdaptToSlope(Defaults, Gravity, V);
			SlantNearlyEq(*this, What + TEXT(": acceleration along the wall"), Adapted.GravityAcceleration, SlantWorldGravity * SinBeta, 0.05);
			SlantNearlyEq(*this, What + TEXT(": terminal speed follows the slope"), Adapted.TerminalDownSpeed, Defaults.TerminalDownSpeed * SinBeta, 0.05);

			FGSTN_WallSlideControlSettings Unscaled = Defaults;
			Unscaled.SlopeTerminalScaling = 0.f;
			const FGSTN_WallSlideControlSettings UnscaledAdapted = UGSTN_WallControlUtils::AdaptToSlope(Unscaled, Gravity, V);
			SlantNearlyEq(*this, What + TEXT(": scaling off keeps the terminal speed"), UnscaledAdapted.TerminalDownSpeed, Defaults.TerminalDownSpeed, 1e-3);
			SlantNearlyEq(*this, What + TEXT(": scaling off still has the gravity along the wall"), UnscaledAdapted.GravityAcceleration, SlantWorldGravity * SinBeta, 0.05);

			FVector2D Velocity = FVector2D::ZeroVector;
			double Distance = 0.0;
			for (int32 i = 0; i < 90; ++i)
			{
				Velocity = UGSTN_WallControlUtils::StepSlideVelocity(Adapted, Velocity, FVector2D::ZeroVector, false, SlantDt);
				Distance += -Velocity.Y * SlantDt;
			}
			TestTrue(*FString::Printf(TEXT("%s: slower than the steeper walls (%.1f vs %.1f cm)"), *What, Distance, Previous), Distance > Previous);
			Previous = Distance;
			Distances.Emplace(Beta, Distance);
			if (Beta == 90.f)
			{
				VerticalDistance = Distance;
			}
		}
		for (const TPair<float, double>& Entry : Distances)
		{
			// Gravity and terminal speed scale together, so the whole slide scales: the distance is sin(beta) of the vertical one
			SlantNearlyEq(*this, FString::Printf(TEXT("Beta %.0f: distance in proportion to sin(beta)"), Entry.Key), Entry.Value / VerticalDistance, FMath::Sin(FMath::DegreesToRadians(Entry.Key)), 1e-3);
		}
	}

	// In the world: the capsule really travels less on the inclined wall, by the same proportion
	for (const FQuat& Frame : SlantFrames())
	{
		const FSlantSlideRun Vertical = RunSlantSlide(90.f, Frame, 120, 0.f);
		for (float Beta : { 60.f, 80.f })
		{
			const FSlantSlideRun Inclined = RunSlantSlide(Beta, Frame, 120, 0.f);
			const double Ratio = Inclined.DownTheWall / FMath::Max(Vertical.DownTheWall, 1.0);
			TestTrue(*FString::Printf(TEXT("%.0f deg: slower than the vertical wall (%.0f vs %.0f cm)"), Beta, Inclined.DownTheWall, Vertical.DownTheWall), Inclined.DownTheWall < Vertical.DownTheWall);
			SlantNearlyEq(*this, FString::Printf(TEXT("%.0f deg: the distance ratio is sin(beta)"), Beta), Ratio, FMath::Sin(FMath::DegreesToRadians(Beta)), 0.02);
		}
	}
	return true;
}

// -------------------------------------------------------------------------------------------------------------------------------------
// Floors stay floors, inclined walls are not
// -------------------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2SlantNotFloorTest, "GSTN.Wall.V2.SlantNotTreatedAsFloor", GSTNWallSlantWorld::TestFlags)
bool FGSTN_WallV2SlantNotFloorTest::RunTest(const FString& Parameters)
{
	const UGSTN_WallSlideV2MovementMode* Mode = GetDefault<UGSTN_WallSlideV2MovementMode>();
	const UGSTN_WallEntryTransition* Entry = GetDefault<UGSTN_WallEntryTransition>();

	// The accepted range is the same for the mode and the entry transition, and it holds every inclined wall and overhang, no floor
	TestEqual(TEXT("Mode and entry accept the same lowest angle"), Mode->MinSurfaceAngle, Entry->MinSurfaceAngle);
	TestEqual(TEXT("Mode and entry accept the same highest angle"), Mode->MaxSurfaceAngle, Entry->MaxSurfaceAngle);
	for (const FQuat& Frame : SlantFrames())
	{
		const FVector Up = Frame.RotateVector(FVector::UpVector);
		for (float Beta : { 50.f, 60.f, 70.f, 80.f, 90.f, 100.f, 110.f })
		{
			const FVector N = Frame.RotateVector(FVector(FMath::Sin(FMath::DegreesToRadians(Beta)), 0.f, FMath::Cos(FMath::DegreesToRadians(Beta))));
			FHitResult Hit;
			Hit.Normal = FVector_NetQuantizeNormal(N);
			Hit.ImpactNormal = FVector_NetQuantizeNormal(N);
			const FVector MovingDown = -Up * 5.f;
			TestFalse(*FString::Printf(TEXT("%.0f deg is not a floor, moving down into it"), Beta), UGSTN_WallQueryUtils::IsFloorHit(Hit, MovingDown, Up, Mode->MinSurfaceAngle));
			TestTrue(*FString::Printf(TEXT("%.0f deg is a wall for the mode"), Beta), UGSTN_WallQueryUtils::IsValidWallNormal(N, Up, Mode->MinSurfaceAngle, Mode->MaxSurfaceAngle));
			TestTrue(*FString::Printf(TEXT("%.0f deg is a wall for the entry transition"), Beta), UGSTN_WallQueryUtils::IsValidWallNormal(N, Up, Entry->MinSurfaceAngle, Entry->MaxSurfaceAngle));
		}
		for (float Beta : { 0.f, 20.f, 30.f, 40.f })
		{
			const FVector N = Frame.RotateVector(FVector(FMath::Sin(FMath::DegreesToRadians(Beta)), 0.f, FMath::Cos(FMath::DegreesToRadians(Beta))));
			FHitResult Hit;
			Hit.Normal = FVector_NetQuantizeNormal(N);
			Hit.ImpactNormal = FVector_NetQuantizeNormal(N);
			TestTrue(*FString::Printf(TEXT("%.0f deg is a floor, moving down into it"), Beta), UGSTN_WallQueryUtils::IsFloorHit(Hit, -Up * 5.f, Up, Mode->MinSurfaceAngle));
			TestFalse(*FString::Printf(TEXT("%.0f deg is not a wall for the mode"), Beta), UGSTN_WallQueryUtils::IsValidWallNormal(N, Up, Mode->MinSurfaceAngle, Mode->MaxSurfaceAngle));
			TestFalse(*FString::Printf(TEXT("%.0f deg is not a wall for the entry transition"), Beta), UGSTN_WallQueryUtils::IsValidWallNormal(N, Up, Entry->MinSurfaceAngle, Entry->MaxSurfaceAngle));
		}
	}

	// In the world: a long slide down a 60 and a 70 degree wall never leaves the mode for the floor...
	for (const FQuat& Frame : SlantFrames())
	{
		for (float Beta : { 60.f, 70.f })
		{
			const FSlantSlideRun Run = RunSlantSlide(Beta, Frame, 240, 0.f);
			TestFalse(*FString::Printf(TEXT("%.0f deg: sliding for 4 s never leaves the mode"), Beta), Run.bLeft);
			TestTrue(*FString::Printf(TEXT("%.0f deg: it slid down (%.0f cm)"), Beta, Run.DownTheWall), Run.DownTheWall > 300.0);
		}
	}

	// ...while a real floor at the foot of the same wall still takes it to the floor mode, just above that floor
	for (const FQuat& Frame : SlantFrames())
	{
		for (float Beta : { 65.f, 80.f })
		{
			constexpr float FloorHeight = -300.f;
			FSlantScene Scene(Beta, Frame);
			Scene.AddWall();
			Scene.AddGravityBox(FVector(0, 0, FloorHeight - 50.f), FVector(4000, 4000, 50));
			Scene.PlaceCapsule(0, 0);

			FVector H, V;
			UGSTN_WallControlUtils::MakePlaneAxes(Scene.Normal(), Scene.Up(), H, V);
			const FVector Down = -V * 500.f;
			const float StartHeight = Scene.Height();
			float LeftAtHeight = TNumericLimits<float>::Max();
			bool bLeftEarly = false;
			for (int32 i = 0; i < 180 && !Scene.State.IsLeaving(); ++i)
			{
				Scene.TickWorld(Down);
				// Well above the floor nothing may take it off the wall
				if (Scene.State.IsLeaving())
				{
					LeftAtHeight = Scene.Height();
					bLeftEarly = LeftAtHeight > FloorHeight + SlantCapHalfHeight + 40.f;
				}
			}
			const FString What = FString::Printf(TEXT("%.0f deg wall with a floor at its foot"), Beta);
			TestTrue(*FString::Printf(TEXT("%s: it started well above the floor (%.0f)"), *What, StartHeight), StartHeight > FloorHeight + 200.f);
			TestTrue(*(What + TEXT(": the mode is left when the floor is reached")), Scene.State.IsLeaving());
			TestFalse(*FString::Printf(TEXT("%s: not before it (left at %.0f)"), *What, LeftAtHeight), bLeftEarly);
			TestEqual(*(What + TEXT(": to the floor mode")), Scene.State.NextModeName, FName(DefaultModeNames::Walking));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2EntrySlantTest, "GSTN.Wall.V2.EntryAcceptsInclinedWalls", GSTNWallSlantWorld::TestFlags)
bool FGSTN_WallV2EntrySlantTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_WallEntryTransition> Entry(NewObject<UGSTN_WallEntryTransition>(GetTransientPackage()));
	for (const FQuat& Frame : SlantFrames())
	{
		for (float Beta : { 50.f, 60.f, 70.f, 80.f, 90.f, 100.f, 110.f })
		{
			FSlantScene Scene(Beta, Frame);
			Scene.AddWall();
			// Close to the wall (8 cm from its surface), the stick toward it: the horizontal direction the transition sweeps along
			Scene.PlaceCapsule(0, 0, 8.f);
			const FVector TowardWall = FVector::VectorPlaneProject(-Scene.Normal(), Scene.Up()).GetSafeNormal();

			FSimulationTickParams Params;
			Params.MovingComps = FMovingComponentSet(Scene.Capsule);
			FHitResult Hit;
			const bool bGrabbed = FGSTN_WallEntryTestAccess::SweepForWall(*Entry, Params, TowardWall, Scene.Up(), Hit);
			TestTrue(*FString::Printf(TEXT("A wall leaning %.0f deg from gravity-up can be grabbed"), Beta), bGrabbed);
			if (bGrabbed)
			{
				SlantNearlyEq(*this, FString::Printf(TEXT("%.0f deg: the face found is the wall's"), Beta), FVector::DotProduct(Hit.ImpactNormal, Scene.Normal()), 1.0, 1e-3);
			}
		}

		// A floor-like slope is not a wall
		FSlantScene Floorish(30.f, Frame);
		Floorish.AddWall();
		Floorish.PlaceCapsule(0, 0, 8.f);
		FSimulationTickParams Params;
		Params.MovingComps = FMovingComponentSet(Floorish.Capsule);
		FHitResult Hit;
		TestFalse(TEXT("A 30 degree slope is a floor, not grabbed"), FGSTN_WallEntryTestAccess::SweepForWall(*Entry, Params,
			FVector::VectorPlaneProject(-Floorish.Normal(), Floorish.Up()).GetSafeNormal(), Floorish.Up(), Hit));
	}
	return true;
}

// -------------------------------------------------------------------------------------------------------------------------------------
// Corners and bumps on an inclined wall
// -------------------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2SlantCornerTest, "GSTN.Wall.V2.CornerOnSlantedWallCaps", GSTNWallSlantWorld::TestFlags)
bool FGSTN_WallV2SlantCornerTest::RunTest(const FString& Parameters)
{
	constexpr float Edge = 200.f;
	constexpr float Speed = 600.f;
	const FGSTN_WallEdgeSettings Edges;
	const float Terminal = FGSTN_WallSlideControlSettings().TerminalDownSpeed;

	for (const FQuat& Frame : SlantFrames())
	{
		for (float Beta : { 60.f, 80.f, 110.f })
		{
			for (const bool bConvex : { true, false })
			{
				const EGSTN_WallEdgeKind ExpectedKind = bConvex ? EGSTN_WallEdgeKind::Convex : EGSTN_WallEdgeKind::Concave;
				const FString What = FString::Printf(TEXT("%s corner on a wall leaning %.0f deg"), bConvex ? TEXT("Convex") : TEXT("Concave"), Beta);
				auto MakeScene = [&](FSlantScene& Scene)
				{
					if (bConvex)
					{
						Scene.AddConvexCorner(Edge, 90.f);
					}
					else
					{
						Scene.AddConcaveCorner(Edge, 90.f);
					}
				};

				{
					// Far from the corner (beyond the look): nothing slows
					FSlantScene Scene(Beta, Frame);
					MakeScene(Scene);
					Scene.PlaceCapsule(-300, 0);
					Scene.Tick(FVector(0, Speed, 0));
					TestTrue(*FString::Printf(TEXT("%s, far: speed kept (was %.1f)"), *What, Scene.State.Velocity.Size()), FMath::Abs(Scene.State.Velocity.Size() - Speed) < 0.5f);
				}
				{
					// 40 cm before it: seen as the right corner, at the angle between the two surfaces' own normals (90, whatever the wall's lean)
					FSlantScene Scene(Beta, Frame);
					MakeScene(Scene);
					Scene.PlaceCapsule(Edge - SlantCapRadius - 40.f, 0);
					const FGSTN_WallAheadResult Ahead = Scene.Probe(FVector(0, Speed, 0));
					TestEqual(*(What + TEXT(": classified")), Ahead.Kind, ExpectedKind);
					TestTrue(*FString::Printf(TEXT("%s: the angle is between the wall normals, 90 (was %.1f)"), *What, Ahead.Angle), FMath::Abs(Ahead.Angle - 90.f) < 1.5f);
					TestTrue(*FString::Printf(TEXT("%s: the edge is 40 cm away (was %.1f)"), *What, Ahead.Distance), FMath::Abs(Ahead.Distance - 40.f) < 3.f);

					const float Cap = SlantAllowedAt(Edges, ExpectedKind, Ahead.Angle, Terminal, Ahead.Distance);
					Scene.Tick(FVector(0, Speed, 0));
					const float After = Scene.State.Velocity.Size();
					TestTrue(*FString::Printf(TEXT("%s, 40 cm: capped to the approach curve %.0f (was %.1f)"), *What, Cap, After), After <= Cap + 0.5f && After < Speed - 20.f);
					TestTrue(*(What + TEXT(": still on the wall")), Scene.State.bHasWall && !Scene.State.IsLeaving());
				}
			}
		}
	}

	// A run at a convex edge on a 70 degree wall with the speed regained every tick: brakes along the curve, never under the corner speed, no plane turn
	for (const FQuat& Frame : SlantFrames())
	{
		FSlantScene Scene(70.f, Frame);
		Scene.AddConvexCorner(Edge, 90.f);
		Scene.PlaceCapsule(-100, 0);
		const float Corner = UGSTN_WallEdgeUtils::EdgeSpeedCap(Edges, EGSTN_WallEdgeKind::Convex, 90.f, Terminal);
		float Slowest = Speed;
		FVector Proposed(0, Speed, 0);
		for (int32 i = 0; i < 40 && Scene.Pos().Y + SlantCapRadius < Edge; ++i)
		{
			Scene.Tick(Proposed);
			const float Now = Scene.State.Velocity.Size();
			const float DistanceToEdge = Edge - (Scene.Pos().Y + SlantCapRadius);
			TestTrue(*FString::Printf(TEXT("Slanted run: never faster than the curve at %.0f cm (was %.0f)"), DistanceToEdge, Now),
				Now <= SlantAllowedAt(Edges, EGSTN_WallEdgeKind::Convex, 90.f, Terminal, DistanceToEdge + 12.f) + 2.f);
			Slowest = FMath::Min(Slowest, Now);
			Proposed = FVector(0, FMath::Min(Now + 980.f * SlantDt, Speed), 0);
		}
		TestTrue(*FString::Printf(TEXT("Slanted run: it slowed down (slowest %.0f, corner speed %.0f)"), Slowest, Corner), Slowest < Speed - 100.f && Slowest >= Corner - 2.f);
		TestTrue(TEXT("Slanted run: the wall is kept"), Scene.State.bHasWall && !Scene.State.IsLeaving());
		TestTrue(*FString::Printf(TEXT("Slanted run: the plane did not turn before the edge (%.1f deg)"), Scene.PlaneTurnDeg()), Scene.PlaneTurnDeg() < 1.f);
	}
	return true;
}

namespace GSTNWallSlantWorld
{
	struct FSlantBumpRun
	{
		float MinProgress = TNumericLimits<float>::Max();
		float WorstTurnDeg = 0.f;
		float FinalY = 0.f;
		float FinalGap = 0.f;
		bool bLeft = false;
		bool bLostWall = false;
		bool bOverlapped = false;
	};

	/** Slides sideways at Speed across a 40 cm wide bump of the given depth, tall along the wall, on a wall leaning BetaDeg */
	static FSlantBumpRun RunSlantBump(float BetaDeg, const FQuat& Frame, float Depth, float Speed)
	{
		FSlantBumpRun Run;
		FSlantScene Scene(BetaDeg, Frame);
		Scene.AddWall();
		Scene.AddBox(FVector(0, 300, 150), FVector(Depth, 340, 450));
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
		Run.FinalGap = static_cast<float>(Scene.Gap());
		Run.bOverlapped = Scene.IsOverlapping();
		return Run;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2SlantBumpTest, "GSTN.Wall.V2.BumpOnSlantedWallSteppedOver", GSTNWallSlantWorld::TestFlags)
bool FGSTN_WallV2SlantBumpTest::RunTest(const FString& Parameters)
{
	constexpr float Speed = 400.f;
	for (const FQuat& Frame : SlantFrames())
	{
		for (float Beta : { 60.f, 80.f, 100.f })
		{
			for (float Depth : { 3.f, 5.f, 9.f })
			{
				const FSlantBumpRun Run = RunSlantBump(Beta, Frame, Depth, Speed);
				const FString What = FString::Printf(TEXT("%.0f cm bump on a wall leaning %.0f deg"), Depth, Beta);
				TestTrue(*FString::Printf(TEXT("%s: crossed (y %.0f)"), *What, Run.FinalY), Run.FinalY > 400.f);
				TestTrue(*FString::Printf(TEXT("%s: no tick lost more than 10%% of its travel (worst %.2f of %.2f)"), *What, Run.MinProgress, Speed * SlantDt), Run.MinProgress >= 0.9f * Speed * SlantDt);
				TestFalse(*(What + TEXT(": mode not left (not read as a floor or a corner)")), Run.bLeft);
				TestFalse(*(What + TEXT(": wall never lost")), Run.bLostWall);
				TestFalse(*(What + TEXT(": never inside the wall")), Run.bOverlapped);
				TestTrue(*FString::Printf(TEXT("%s: the proxy plane never turned (worst %.2f deg)"), *What, Run.WorstTurnDeg), Run.WorstTurnDeg < 1.f);
				TestTrue(*FString::Printf(TEXT("%s: seated back on the wall afterwards (gap %.2f)"), *What, Run.FinalGap), FMath::Abs(Run.FinalGap - 2.f) < 0.3f);
			}
		}
	}
	return true;
}

// -------------------------------------------------------------------------------------------------------------------------------------
// Adhesion on a tilted contact
// -------------------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2SlantAdhesionTest, "GSTN.Wall.V2.AdhesionGapOnTiltedContact", GSTNWallSlantWorld::TestFlags)
bool FGSTN_WallV2SlantAdhesionTest::RunTest(const FString& Parameters)
{
	for (const FQuat& Frame : SlantFrames())
	{
		for (float Beta : { 60.f, 80.f, 100.f, 110.f })
		{
			const FString What = FString::Printf(TEXT("Wall leaning %.0f deg"), Beta);

			{
				// Too far (a recess): pulled in by at most MaxAdhesionSpeed * dt per tick, to the gap, never past it
				FSlantScene Scene(Beta, Frame);
				Scene.AddWall();
				const float WallGap = Scene.Mode->WallGap;
				Scene.PlaceCapsule(0, 0, WallGap + 10.f);
				const double MaxStep = Scene.Mode->MaxAdhesionSpeed * SlantDt;
				double Previous = Scene.Gap();
				SlantNearlyEq(*this, What + TEXT(": starts 10 cm too far"), Previous, WallGap + 10.0, 0.1);
				for (int32 i = 0; i < 6; ++i)
				{
					Scene.Tick(FVector::ZeroVector);
					const double Gap = Scene.Gap();
					TestTrue(*FString::Printf(TEXT("%s, pull-in tick %d: closer, by no more than the rate limit (%.3f -> %.3f)"), *What, i, Previous, Gap),
						Gap <= Previous + 1e-3 && Previous - Gap <= MaxStep + 0.05);
					TestTrue(*(What + TEXT(": never past the gap")), Gap >= WallGap - 0.05);
					Previous = Gap;
				}
				SlantNearlyEq(*this, What + TEXT(": settled at WallGap"), Previous, WallGap, 0.1);
			}
			{
				// Too close: pushed out to the gap at once
				FSlantScene Scene(Beta, Frame);
				Scene.AddWall();
				Scene.PlaceCapsule(0, 0, 0.5f);
				Scene.Tick(FVector::ZeroVector);
				SlantNearlyEq(*this, What + TEXT(": pushed out to WallGap in one tick"), Scene.Gap(), Scene.Mode->WallGap, 0.1);
			}
			{
				// Moving along the wall, down it and sideways, holds the gap
				FSlantScene Scene(Beta, Frame);
				Scene.AddWall();
				Scene.PlaceCapsule(0, 0);
				for (int32 i = 0; i < 60; ++i)
				{
					Scene.Tick(FVector(0, 200, -400));
				}
				SlantNearlyEq(*this, What + TEXT(": gap after a second of sliding"), Scene.Gap(), Scene.Mode->WallGap, 0.15);
				TestTrue(*(What + TEXT(": the wall is held")), Scene.State.bHasWall && !Scene.State.IsLeaving());
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2SlantWrapTest, "GSTN.Wall.V2.WrapFindsFaceAroundEdgeOnSlant", GSTNWallSlantWorld::TestFlags)
bool FGSTN_WallV2SlantWrapTest::RunTest(const FString& Parameters)
{
	// The capsule has just left the end of an inclined wall (a 90 degree outside edge at y = 200): the face around the edge is found by diving
	// behind the old plane from the capsule's real extent along the normal, not its radius
	FGSTN_WallCheckSettings Check;
	Check.MinSurfaceAngle = 45.f;
	Check.MaxSurfaceAngle = 120.f;
	Check.MaxConvexAngle = 120.f;
	Check.WallGap = 2.f;

	for (const FQuat& Frame : SlantFrames())
	{
		for (float Beta : { 60.f, 80.f, 100.f })
		{
			FSlantScene Scene(Beta, Frame);
			Scene.AddConvexCorner(200.f, 90.f);
			Scene.PlaceCapsule(260.f, 0.f);
			const FString What = FString::Printf(TEXT("Wall leaning %.0f deg"), Beta);

			FGSTN_WallProbeResult Probe;
			const FVector MoveDelta = Scene.ToWorld(FVector(0, 5, 0));
			const bool bFound = UGSTN_WallQueryUtils::FindWallAroundConvexEdge(FMovingComponentSet(Scene.Capsule), Scene.Normal(), MoveDelta, Scene.Up(), Check, Probe, Scene.Support());
			TestTrue(*(What + TEXT(": a face is found around the edge")), bFound);
			if (bFound)
			{
				// The pillar's end face: perpendicular to the wall, facing the way the capsule came from the edge
				TestTrue(*FString::Printf(TEXT("%s: it is the end face, 90 deg from the wall (was %.1f)"), *What, UGSTN_WallEdgeUtils::AngleBetweenNormalsDeg(Probe.Normal.GetSafeNormal(), Scene.Normal())),
					FMath::Abs(UGSTN_WallEdgeUtils::AngleBetweenNormalsDeg(Probe.Normal.GetSafeNormal(), Scene.Normal()) - 90.f) < 15.f);
			}
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
