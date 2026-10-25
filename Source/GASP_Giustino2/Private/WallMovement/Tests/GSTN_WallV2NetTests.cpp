// Network and compatibility tests of the v2 wall modes (wall-slide-v2.md W5): GSTN.Wall.V2Net.*
//
// Same headless approach as the other v2 tests: no Mover pawn, so what runs here is everything below the tick, in a throwaway game world.
// One addition: a rollback blackboard driven frame by frame (BeginSimulationFrame / EndSimulationFrame), which is what lets the tick's base
// bookkeeping (ApplyMovementBase, UpdateMovementBase: the ridden wall travels between ticks through GSTNWallBlackboard::BaseInfo) run as in
// SimulationTick.
//  - PawnsNeverWalls: other pawns are never wall geometry (finding 15) in the contact sweep, the look-ahead, the wrap and the corner logic
//  - LockedSkipsExitsKeepsAdhesion: IsMovementLocked holds the mode (no exit by wall loss, no surface switch, no timers) while adhesion works
//  - RidesMovingWall / ExitVelocityIncludesBase: a kinematic wall that moves, rotates; the velocity of the base is added when leaving
//  - CorrectionOnMovingWallConverges: a correction (rollback of the blackboard, the authority's state restored) while riding converges (6.14)
//  - SyncStateRoundTripWithSurfaceSwitch: the sync state with the surface-switch vote survives the wire, tick after tick
// Not covered here (PIE): Activate, the sync state capture, the real freeze layered move (the Combat.Mover.Frozen tag), a base that is a TS-A collider.

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "GSTN_SurfaceBehaviourComponent.h"
#include "GSTN_SurfaceTags.h"
#include "Misc/AutomationTest.h"
#include "MoveLibrary/MovementRecord.h"
#include "MoveLibrary/MovementUtilsTypes.h"
#include "MoveLibrary/RollbackBlackboard.h"
#include "MoveLibrary/RollbackBlackboardWrappers.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "Serialization/BitReader.h"
#include "Serialization/BitWriter.h"
#include "UObject/StrongObjectPtr.h"
#include "WallMovement/MoveLibrary/GSTN_WallMovementUtils.h"
#include "WallMovement/MoveLibrary/GSTN_WallQueryUtils.h"
#include "WallMovement/MoveLibrary/GSTN_WallSurfaceModeUtils.h"
#include "WallMovement/Modes/GSTN_WallClimbV2MovementMode.h"
#include "WallMovement/Modes/GSTN_WallSlideV2MovementMode.h"
#include "GSTN_WallV2TestAccess.h"

namespace GSTNWallNetTest
{
	constexpr EAutomationTestFlags NetTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float NetDt = 1.f / 60.f;
	constexpr float NetRadius = 34.f;
	constexpr float NetHalfHeight = 88.f;
	constexpr double WallHalfDepth = 50.0;

	static void SetNetBehaviours(UGSTN_SurfaceBehaviourComponent* Surface, const FGameplayTag& Tag)
	{
		Surface->Default.Behaviours.Reset();
		Surface->Default.Behaviours.AddTag(Tag);
	}

	/** A throwaway game world: a box wall (front face through the origin, normal +X) and an upright capsule next to it, a mode, and a blackboard to ride bases with */
	struct FNetScene
	{
		UWorld* World = nullptr;
		UBoxComponent* Wall = nullptr;
		UGSTN_SurfaceBehaviourComponent* Surface = nullptr;
		UCapsuleComponent* Capsule = nullptr;
		TStrongObjectPtr<UGSTN_WallTraverseModeBase> Mode;
		TStrongObjectPtr<URollbackBlackboard> Blackboard;
		FSimulationTickParams Params;
		FGSTN_WallTickState State;
		FMovementRecord MoveRecord;
		FVector WallNormal = FVector::ForwardVector;
		int32 Frame = 0;

		explicit FNetScene(TSubclassOf<UGSTN_WallTraverseModeBase> ModeClass = UGSTN_WallSlideV2MovementMode::StaticClass())
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNWallNetTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Mode.Reset(NewObject<UGSTN_WallTraverseModeBase>(GetTransientPackage(), ModeClass));
			MoveRecord.SetDeltaSeconds(NetDt);
		}

		~FNetScene()
		{
			Params = FSimulationTickParams();
			Blackboard.Reset();
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		/** The wall. bMovable: a dynamic base (kinematic, moved by the test). Behaviour: a surface component offering exactly this behaviour */
		void MakeWall(bool bMovable = false, const FGameplayTag* Behaviour = nullptr)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			Wall = NewObject<UBoxComponent>(Actor, TEXT("Wall"));
			Wall->SetBoxExtent(FVector(WallHalfDepth, 2000.0, 4000.0));
			Wall->SetMobility(bMovable ? EComponentMobility::Movable : EComponentMobility::Static);
			Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Wall->SetCollisionObjectType(ECC_WorldStatic);
			Wall->SetCollisionResponseToAllChannels(ECR_Block);
			Actor->SetRootComponent(Wall);
			Wall->SetWorldLocationAndRotation(-WallNormal * WallHalfDepth, FRotationMatrix::MakeFromXZ(WallNormal, FVector::UpVector).ToQuat());
			Wall->RegisterComponent();
			if (Behaviour)
			{
				Surface = NewObject<UGSTN_SurfaceBehaviourComponent>(Actor, TEXT("Surface"));
				SetNetBehaviours(Surface, *Behaviour);
				Surface->RegisterComponent();
			}
		}

		UCapsuleComponent* MakePawnShape(const FVector& Location, float Radius, float HalfHeight)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UCapsuleComponent* Shape = NewObject<UCapsuleComponent>(Actor, TEXT("Capsule"));
			Shape->InitCapsuleSize(Radius, HalfHeight);
			Shape->SetMobility(EComponentMobility::Movable);
			Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Shape->SetCollisionObjectType(ECC_Pawn);
			Shape->SetCollisionResponseToAllChannels(ECR_Block);
			Actor->SetRootComponent(Shape);
			Shape->SetWorldLocation(Location);
			Shape->RegisterComponent();
			return Shape;
		}

		/** The character: upright, facing the wall, its surface Gap from the wall plane, at height Z */
		void MakeCapsule(float Gap, float Z)
		{
			Capsule = MakePawnShape(FVector(NetRadius + Gap, 0.0, Z), NetRadius, NetHalfHeight);
			const FVector Up = FVector::UpVector;
			Capsule->SetWorldRotation(UGSTN_WallTraverseModeBase::ComputeFacingQuat(FQuat::Identity, WallNormal, Up));

			State = FGSTN_WallTickState();
			State.WallNormal = WallNormal;
			State.GravityUp = Up;
			State.Orientation = Capsule->GetComponentQuat();
			State.bHasWall = true;

			Blackboard.Reset(NewObject<URollbackBlackboard>(GetTransientPackage()));
			Params = FSimulationTickParams();
			Params.SimContext.Blackboard = FRollbackBlackboardSimWrapper(*Blackboard);
			Params.MovingComps = FMovingComponentSet(Capsule);
			GSTNWallBlackboard::EnsureEntries(Params.SimContext.Blackboard);
		}

		FMovingComponentSet Comps() const { return FMovingComponentSet(Capsule); }

		/** The world part of a tick, as SimulationTick calls it, with the surface vote after it */
		void Tick(const FVector& Velocity)
		{
			State.Velocity = Velocity;
			FGSTN_WallTraverseTestAccess::Step(*Mode, Comps(), NetDt, !Velocity.IsNearlyZero(), State, MoveRecord);
			FGSTN_WallTraverseTestAccess::Vote(*Mode, FMoverSyncState(), State);
		}

		/** A tick with the base bookkeeping: ride the ridden wall, the world part, then store (or add the velocity of, when leaving) the base */
		void RideTick(const FVector& Velocity, bool bRide = true, bool bLeave = false)
		{
			FMoverTimeStep Step;
			Step.ServerFrame = Frame;
			Step.BaseSimTimeMs = Frame * NetDt * 1000.0;
			Step.StepMs = NetDt * 1000.f;
			Blackboard->BeginSimulationFrame(Step);

			State.Velocity = Velocity;
			if (bRide)
			{
				FGSTN_WallTraverseTestAccess::ApplyBase(*Mode, Params, State, MoveRecord);
			}
			if (bLeave)
			{
				State.LeaveTo(FName("Falling"), 0.f);
			}
			else
			{
				FGSTN_WallTraverseTestAccess::Step(*Mode, Comps(), NetDt, !State.Velocity.IsNearlyZero(), State, MoveRecord);
			}
			if (bRide)
			{
				FGSTN_WallTraverseTestAccess::UpdateBase(*Mode, Params, State);
			}

			Blackboard->EndSimulationFrame();
			++Frame;
		}

		/** The base the next frame would ride: the blackboard as the next tick reads it */
		FGSTN_WallBaseInfo ReadBaseInfo()
		{
			FMoverTimeStep Step;
			Step.ServerFrame = Frame;
			Step.BaseSimTimeMs = Frame * NetDt * 1000.0;
			Step.StepMs = NetDt * 1000.f;
			Blackboard->BeginSimulationFrame(Step);
			FGSTN_WallBaseInfo Info;
			Params.SimContext.Blackboard.TryGet(GSTNWallBlackboard::BaseInfo, Info);
			Blackboard->EndSimulationFrame();
			++Frame;
			return Info;
		}

		/** Moves (and turns about the vertical through Pivot) the kinematic wall as a physics step would have left it */
		void MoveWall(const FVector& Delta, float YawDeg, const FVector& Pivot)
		{
			FTransform T = Wall->GetComponentTransform();
			const FQuat Rot(FVector::UpVector, FMath::DegreesToRadians(YawDeg));
			T.SetLocation(Pivot + Rot.RotateVector(T.GetLocation() - Pivot) + Delta);
			T.SetRotation((Rot * T.GetRotation()).GetNormalized());
			Wall->SetWorldTransform(T, false, nullptr, ETeleportType::TeleportPhysics);
		}

		/** Gap between the capsule surface and the wall's current front face along its current normal (upright capsule, vertical wall) */
		double GapToWall() const
		{
			const FVector Normal = Wall->GetForwardVector();
			const FVector FacePoint = Wall->GetComponentLocation() + Normal * WallHalfDepth;
			return FVector::DotProduct(Capsule->GetComponentLocation() - FacePoint, Normal) - NetRadius;
		}

		bool IsOverlappingPawnChannel() const
		{
			FCollisionQueryParams QueryParams(NAME_None, false, Capsule->GetOwner());
			return World->OverlapAnyTestByChannel(Capsule->GetComponentLocation(), Capsule->GetComponentQuat(), ECC_Pawn, Capsule->GetCollisionShape(), QueryParams);
		}

		double GapToFixedWall() const
		{
			return Capsule->GetComponentLocation().X - NetRadius;
		}
	};

	static FHitResult MakeFaceHitOn(UPrimitiveComponent* Component, const FVector& Normal)
	{
		FHitResult Hit(Component->GetOwner(), Component, Component->GetComponentLocation(), Normal);
		Hit.Normal = Normal;
		Hit.ImpactNormal = Normal;
		return Hit;
	}
}

using namespace GSTNWallNetTest;

// ------------------------------------------------------------------------------------------------
// Pawns
// ------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2NetPawnsTest, "GSTN.Wall.V2Net.PawnsNeverWalls", GSTNWallNetTest::NetTestFlags)
bool FGSTN_WallV2NetPawnsTest::RunTest(const FString& Parameters)
{
	// The setting reaches every query: the v2 mode asks for it by default, the legacy modes (base GetWallCheckSettings) do not
	{
		FNetScene Scene;
		TestTrue(TEXT("v2 ignores pawns by default"), Scene.Mode->GetWallCheckSettings().bIgnorePawns);
		Scene.Mode->Edges.bIgnorePawns = false;
		TestFalse(TEXT("The edge setting is the switch"), Scene.Mode->GetWallCheckSettings().bIgnorePawns);
		TestFalse(TEXT("The query settings default to the legacy behaviour"), FGSTN_WallCheckSettings().bIgnorePawns);
	}

	// 1. The contact sweep: another character where a wall would be is not a wall. No wall at all here, only a second capsule 4 cm in front of the character
	{
		FNetScene Scene;
		Scene.MakeCapsule(2.f, 300.f);
		UCapsuleComponent* Other = Scene.MakePawnShape(FVector(NetRadius + 2.f - (2.f * NetRadius + 4.f), 0.0, 300.0), NetRadius, NetHalfHeight);

		FGSTN_WallCheckSettings Check = Scene.Mode->GetWallCheckSettings();
		FHitResult Contact;
		TestFalse(TEXT("A pawn is not a wall for the contact sweep"), UGSTN_WallMovementUtils::AdhereAndSense(Scene.Comps(), Scene.State.Orientation,
			Scene.WallNormal, FVector::UpVector, Check, Scene.Mode->MaxAdhesionSpeed, NetDt, Scene.MoveRecord, Contact));

		Check.bIgnorePawns = false;
		TestTrue(TEXT("Control: without the setting its side is a valid wall"), UGSTN_WallMovementUtils::AdhereAndSense(Scene.Comps(), Scene.State.Orientation,
			Scene.WallNormal, FVector::UpVector, Check, Scene.Mode->MaxAdhesionSpeed, NetDt, Scene.MoveRecord, Contact));
		TestTrue(TEXT("Control: and the contact is the other pawn"), Contact.GetComponent() == Other);
	}

	// 2. A pawn between the character and the real wall does not hide it: the sweep goes through and names the wall
	{
		FNetScene Scene;
		Scene.MakeWall();
		Scene.MakeCapsule(15.f, 300.f);
		UCapsuleComponent* Pole = Scene.MakePawnShape(FVector(6.0, 0.0, 300.0), 3.f, 30.f);
		const FVector Start = Scene.Capsule->GetComponentLocation();

		FGSTN_WallCheckSettings Check = Scene.Mode->GetWallCheckSettings();
		FHitResult Contact;
		const bool bFound = UGSTN_WallMovementUtils::AdhereAndSense(Scene.Comps(), Scene.State.Orientation, Scene.WallNormal, FVector::UpVector, Check,
			Scene.Mode->MaxAdhesionSpeed, NetDt, Scene.MoveRecord, Contact);
		TestTrue(TEXT("The wall behind a pawn is found"), bFound);
		TestTrue(TEXT("The contact is the wall, not the pawn"), Contact.GetComponent() == Scene.Wall);

		Scene.Capsule->SetWorldLocation(Start);
		Check.bIgnorePawns = false;
		UGSTN_WallMovementUtils::AdhereAndSense(Scene.Comps(), Scene.State.Orientation, Scene.WallNormal, FVector::UpVector, Check,
			Scene.Mode->MaxAdhesionSpeed, NetDt, Scene.MoveRecord, Contact);
		TestTrue(TEXT("Control: without the setting the pawn is what the sweep meets"), Contact.GetComponent() == Pole);
	}

	// 3. The look-ahead: a pawn standing on the wall below does not read as a bump ahead
	{
		FNetScene Scene;
		Scene.MakeWall();
		Scene.MakeCapsule(2.f, 300.f);
		// The look ahead of a 400 cm/s slide ends 60 cm below the feet (z = 152); a thin pole against the wall there is 8 cm tall as seen by the trace
		Scene.MakePawnShape(FVector(4.0, 0.0, 152.0), 4.f, 20.f);
		Scene.State.Velocity = FVector(0, 0, -400);

		const FGSTN_WallAheadResult Filtered = FGSTN_WallTraverseTestAccess::ProbeAhead(*Scene.Mode, Scene.Comps(), Scene.State);
		TestEqual(TEXT("A pawn ahead is not a feature of the wall"), static_cast<int32>(Filtered.Kind), static_cast<int32>(EGSTN_WallEdgeKind::None));

		Scene.Mode->Edges.bIgnorePawns = false;
		const FGSTN_WallAheadResult Control = FGSTN_WallTraverseTestAccess::ProbeAhead(*Scene.Mode, Scene.Comps(), Scene.State);
		TestTrue(TEXT("Control: without the setting the look-ahead sees it"), Control.Kind != EGSTN_WallEdgeKind::None);
	}

	// 4. The wrap around an outside edge: the face "around the edge" is not another character
	{
		FNetScene Scene;
		Scene.MakeCapsule(2.f, 300.f);
		Scene.MakePawnShape(FVector(NetRadius + 2.f - (2.f * NetRadius + 4.f), 0.0, 300.0), NetRadius, NetHalfHeight);

		FGSTN_WallCheckSettings Check = Scene.Mode->GetWallCheckSettings();
		FGSTN_WallProbeResult Probe;
		TestFalse(TEXT("The wrap finds no wall in a pawn"), UGSTN_WallQueryUtils::FindWallAroundConvexEdge(Scene.Comps(), Scene.WallNormal,
			FVector(0, 0, -6), FVector::UpVector, Check, Probe, NetRadius));

		Check.bIgnorePawns = false;
		TestTrue(TEXT("Control: without the setting it does"), UGSTN_WallQueryUtils::FindWallAroundConvexEdge(Scene.Comps(), Scene.WallNormal,
			FVector(0, 0, -6), FVector::UpVector, Check, Probe, NetRadius));
	}

	// 5. Every acceptance goes through IsHitOnValidWall: a pawn's side is refused, the same hit on a prop is accepted
	{
		FNetScene Scene;
		Scene.MakeWall();
		Scene.MakeCapsule(2.f, 300.f);
		UCapsuleComponent* Other = Scene.MakePawnShape(FVector(0.0, 300.0, 300.0), NetRadius, NetHalfHeight);
		FCollisionQueryParams QueryParams;
		FCollisionResponseParams ResponseParams;

		FGSTN_WallCheckSettings Check;
		TestTrue(TEXT("IsPawnHit: a pawn"), UGSTN_WallQueryUtils::IsPawnHit(MakeFaceHitOn(Other, FVector::ForwardVector)));
		TestFalse(TEXT("IsPawnHit: a wall"), UGSTN_WallQueryUtils::IsPawnHit(MakeFaceHitOn(Scene.Wall, FVector::ForwardVector)));

		const FHitResult PawnHit = MakeFaceHitOn(Other, FVector::ForwardVector);
		const FHitResult WallHit = MakeFaceHitOn(Scene.Wall, FVector::ForwardVector);
		Check.bIgnorePawns = true;
		TestFalse(TEXT("A pawn hit is no wall when pawns are ignored"), UGSTN_WallQueryUtils::IsHitOnValidWall(Scene.World, PawnHit, FVector::UpVector, Check, ECC_Pawn, QueryParams, ResponseParams));
		TestTrue(TEXT("A wall hit still is"), UGSTN_WallQueryUtils::IsHitOnValidWall(Scene.World, WallHit, FVector::UpVector, Check, ECC_Pawn, QueryParams, ResponseParams));
		Check.bIgnorePawns = false;
		TestTrue(TEXT("Control: the legacy rule accepts the pawn's side"), UGSTN_WallQueryUtils::IsHitOnValidWall(Scene.World, PawnHit, FVector::UpVector, Check, ECC_Pawn, QueryParams, ResponseParams));
	}

	// 6. End to end: sliding sideways into a character standing on the same wall. The move is stopped by it, but its side is neither an inside
	// corner nor the new wall: the proxy plane stays where it was
	{
		auto RunInto = [this](bool bIgnorePawns, double& OutMinDot)
		{
			FNetScene Scene;
			Scene.MakeWall();
			Scene.MakeCapsule(2.f, 300.f);
			Scene.Mode->Edges.bIgnorePawns = bIgnorePawns;
			Scene.MakePawnShape(FVector(NetRadius + 2.f, 2.f * NetRadius + 14.f, 300.0), NetRadius, NetHalfHeight);
			OutMinDot = 1.0;
			bool bStillOnWall = true;
			for (int32 i = 0; i < 24; ++i)
			{
				Scene.Tick(FVector(0, 400, 0));
				OutMinDot = FMath::Min(OutMinDot, static_cast<double>(FVector::DotProduct(Scene.State.WallNormal, FVector::ForwardVector)));
				bStillOnWall &= Scene.State.bHasWall && !Scene.State.IsLeaving();
			}
			return bStillOnWall;
		};

		double MinDot = 0.0;
		TestTrue(TEXT("Blocked by a pawn: still on the wall"), RunInto(true, MinDot));
		TestTrue(*FString::Printf(TEXT("Blocked by a pawn: the plane does not turn toward it (min dot %.4f)"), MinDot), MinDot > 0.999);

		double ControlMinDot = 0.0;
		RunInto(false, ControlMinDot);
		TestTrue(*FString::Printf(TEXT("Control: the legacy rule turns the plane toward the pawn (min dot %.4f)"), ControlMinDot), ControlMinDot < 0.99);
	}
	return true;
}

// ------------------------------------------------------------------------------------------------
// Movement lock
// ------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2NetLockedTest, "GSTN.Wall.V2Net.LockedSkipsExitsKeepsAdhesion", GSTNWallNetTest::NetTestFlags)
bool FGSTN_WallV2NetLockedTest::RunTest(const FString& Parameters)
{
	// Not frozen by default: no Mover state, no lock
	{
		FNetScene Scene;
		TestFalse(TEXT("Nothing freezes a mode without the tag"), FGSTN_WallTraverseTestAccess::IsMovementLocked(*Scene.Mode, FMoverTickStartData()));
	}

	// 1. The surface vote: a climb mode on slide-only geometry asks to leave after the hysteresis. Locked, it never does, the count does not move,
	// and the capsule is still pulled back to the wall (adhesion). Unlocked, the same state leaves on the Nth tick
	{
		FNetScene Scene(UGSTN_WallClimbV2MovementMode::StaticClass());
		const FGameplayTag SlideTag = GSTNSurfaceTags::Behaviour_WallSlide;
		Scene.MakeWall(false, &SlideTag);
		Scene.MakeCapsule(8.f, 300.f);
		FGSTN_WallTraverseTestAccess::SetRegisteredName(*Scene.Mode, UGSTN_WallSurfaceModeUtils::ClimbModeName);
		const int32 Hysteresis = Scene.Mode->SurfaceModes.HysteresisTicks;
		const FVector Velocity(0, 60, 0);

		Scene.State.bMovementLocked = true;
		bool bEverLeft = false;
		int32 MaxVote = 0;
		for (int32 i = 0; i < Hysteresis * 3; ++i)
		{
			Scene.Tick(Velocity);
			bEverLeft |= Scene.State.IsLeaving();
			MaxVote = FMath::Max(MaxVote, static_cast<int32>(Scene.State.SurfaceSwitchTicks));
		}
		TestFalse(TEXT("Locked: the surface never switches the mode"), bEverLeft);
		TestEqual(TEXT("Locked: the vote does not count"), MaxVote, 0);
		TestTrue(*FString::Printf(TEXT("Locked: adhesion pulled the capsule to the wall (gap %.2f)"), Scene.GapToFixedWall()), FMath::Abs(Scene.GapToFixedWall() - Scene.Mode->WallGap) < 0.3);
		TestTrue(TEXT("Locked: still on the wall"), Scene.State.bHasWall);

		Scene.State.bMovementLocked = false;
		for (int32 i = 0; i < Hysteresis && !Scene.State.IsLeaving(); ++i)
		{
			Scene.Tick(Velocity);
		}
		TestTrue(TEXT("Control: unlocked, the same surface switches the mode"), Scene.State.IsLeaving());
	}

	// 2. The wall is gone (a frozen character whose wall went away): no timer, no exit while locked; unlocked, it leaves after the grace time
	{
		FNetScene Scene;
		Scene.MakeWall();
		Scene.MakeCapsule(2.f, 300.f);
		Scene.Mode->NoWallModeName = FName("Falling");
		Scene.Mode->NoWallGraceTime = 0.05f;
		Scene.Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		Scene.State.bMovementLocked = true;
		for (int32 i = 0; i < 30; ++i)
		{
			Scene.Tick(FVector::ZeroVector);
		}
		TestFalse(TEXT("Locked: losing the wall does not end the mode"), Scene.State.IsLeaving());
		TestTrue(TEXT("Locked: the wall is still the wall"), Scene.State.bHasWall);
		TestEqual(TEXT("Locked: TimeWithoutWall stands still"), Scene.State.TimeWithoutWall, 0.f);

		Scene.State.bMovementLocked = false;
		for (int32 i = 0; i < 10 && !Scene.State.IsLeaving(); ++i)
		{
			Scene.Tick(FVector::ZeroVector);
		}
		TestTrue(TEXT("Control: unlocked, the same loss leaves after the grace time"), Scene.State.IsLeaving());
		TestTrue(TEXT("Control: to the no-wall mode"), Scene.State.NextModeName == FName("Falling"));
	}
	return true;
}

// ------------------------------------------------------------------------------------------------
// Moving walls
// ------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2NetRidesTest, "GSTN.Wall.V2Net.RidesMovingWall", GSTNWallNetTest::NetTestFlags)
bool FGSTN_WallV2NetRidesTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		const TCHAR* Name;
		FVector WallVelocity;
		float YawRate;
	};
	// Away from the character (faster than the adhesion can pull), sideways along the wall, rotating about a vertical axis 4 m along the wall
	// (the face in front of the character then moves along its normal at about 140 cm/s while its normal turns 20 degrees every second)
	const FCase Cases[] = {
		{ TEXT("receding 150"), FVector(-150, 0, 0), 0.f },
		{ TEXT("receding 600"), FVector(-600, 0, 0), 0.f },
		{ TEXT("sideways 300"), FVector(0, 300, 0), 0.f },
		{ TEXT("rotating 20 deg/s"), FVector::ZeroVector, 20.f },
	};

	for (const FCase& Case : Cases)
	{
		FNetScene Scene;
		Scene.MakeWall(/*bMovable*/ true);
		Scene.MakeCapsule(Scene.Mode->WallGap, 300.f);
		const FVector Pivot(0, 400, 0);
		const FVector Slide(0, 0, -100);

		// Two settling ticks on the still wall: the base is recorded
		Scene.RideTick(Slide);
		Scene.RideTick(Slide);
		TestTrue(*FString::Printf(TEXT("%s: the wall is the ridden base"), Case.Name), Scene.ReadBaseInfo().Base.Get() == Scene.Wall);

		double MaxGapError = 0.0;
		bool bOnWall = true;
		bool bClear = true;
		for (int32 i = 0; i < 90; ++i)
		{
			Scene.MoveWall(Case.WallVelocity * NetDt, Case.YawRate * NetDt, Pivot);
			Scene.RideTick(Slide);
			MaxGapError = FMath::Max(MaxGapError, FMath::Abs(Scene.GapToWall() - Scene.Mode->WallGap));
			bOnWall &= Scene.State.bHasWall && !Scene.State.IsLeaving();
			bClear &= !Scene.IsOverlappingPawnChannel();
		}
		TestTrue(*FString::Printf(TEXT("%s: stays on the wall"), Case.Name), bOnWall);
		TestTrue(*FString::Printf(TEXT("%s: keeps the gap (max error %.2f cm)"), Case.Name, MaxGapError), MaxGapError < 1.0);
		TestTrue(*FString::Printf(TEXT("%s: never inside the wall"), Case.Name), bClear);
	}

	// Control: the same retreating wall without riding is lost (the adhesion alone cannot follow 600 cm/s)
	{
		FNetScene Scene;
		Scene.MakeWall(/*bMovable*/ true);
		Scene.MakeCapsule(Scene.Mode->WallGap, 300.f);
		bool bEverLost = false;
		double MaxGap = 0.0;
		for (int32 i = 0; i < 30; ++i)
		{
			Scene.MoveWall(FVector(-600, 0, 0) * NetDt, 0.f, FVector::ZeroVector);
			Scene.RideTick(FVector(0, 0, -100), /*bRide*/ false);
			bEverLost |= !Scene.State.bHasWall;
			MaxGap = FMath::Max(MaxGap, Scene.GapToWall());
		}
		TestTrue(*FString::Printf(TEXT("Control: without riding the receding wall is lost or left behind (max gap %.1f)"), MaxGap), bEverLost || MaxGap > 10.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2NetCorrectionOnMovingWallTest, "GSTN.Wall.V2Net.CorrectionOnMovingWallConverges", GSTNWallNetTest::NetTestFlags)
bool FGSTN_WallV2NetCorrectionOnMovingWallTest::RunTest(const FString& Parameters)
{
	// wall-slide-v2.md 6.14. The "server" rides a moving wall from a start Offset away from the "client"'s. At frame CorrectAt the client receives the
	// server's state of the frame before: Mover's rollback puts the capsule (and the rest of the sync state) there and rewinds the rollback blackboard,
	// whose base record of that frame is still the CLIENT's own prediction. The client then re-simulates. Converged = from then on the client's
	// capsule and base record equal the server's. The legacy ride (bRideFromSyncedLocation off, the control) goes back to the client's recorded place
	// on the wall and keeps the offset for good: what the PIE harness showed as ~20 cm and a reconcile every few frames.
	constexpr int32 Frames = 40;
	constexpr int32 CorrectAt = 20;
	const FVector Offset(0.0, 20.0, 3.0);
	const FVector Slide(0, 0, -100);
	const FVector WallVelocity(0, 300, 0);
	constexpr float YawRate = 20.f;
	const FVector Pivot(0, 400, 0);

	struct FFrameSnap
	{
		FTransform Capsule;
		FTransform Wall;
		FGSTN_WallTickState State;
	};

	// Every frame: the wall moves as the physics step would have left it, then the tick rides it. Same sequence on both machines
	auto RunFrame = [&](GSTNWallNetTest::FNetScene& Scene)
	{
		Scene.MoveWall(WallVelocity * NetDt, YawRate * NetDt, Pivot);
		Scene.RideTick(Slide);
	};
	auto Snap = [](const GSTNWallNetTest::FNetScene& Scene)
	{
		return FFrameSnap{ Scene.Capsule->GetComponentTransform(), Scene.Wall->GetComponentTransform(), Scene.State };
	};

	// The server, never corrected: its frames are the reference
	TArray<FFrameSnap> Server;
	FGSTN_WallBaseInfo ServerRecord;
	bool bServerRidesWall = false;
	{
		FNetScene Scene;
		Scene.MakeWall(/*bMovable*/ true);
		Scene.MakeCapsule(Scene.Mode->WallGap, 300.f);
		Scene.Capsule->SetWorldLocation(Scene.Capsule->GetComponentLocation() + Offset);
		Scene.RideTick(Slide);   // the grab: first record
		for (int32 i = 0; i < Frames; ++i)
		{
			RunFrame(Scene);
			Server.Add(Snap(Scene));
		}
		ServerRecord = Scene.ReadBaseInfo();
		bServerRidesWall = ServerRecord.Base.Get() == Scene.Wall;
	}

	for (const bool bRideFromSynced : { true, false })
	{
		FNetScene Scene;
		Scene.Mode->bRideFromSyncedLocation = bRideFromSynced;
		Scene.MakeWall(/*bMovable*/ true);
		Scene.MakeCapsule(Scene.Mode->WallGap, 300.f);
		Scene.RideTick(Slide);
		const int32 FirstFrame = Scene.Frame;
		TArray<FFrameSnap> Client;
		for (int32 i = 0; i < Frames; ++i)
		{
			RunFrame(Scene);
			Client.Add(Snap(Scene));
		}
		const double PredictedError = (Client[CorrectAt - 1].Capsule.GetLocation() - Server[CorrectAt - 1].Capsule.GetLocation()).Size();

		// The correction: the authority's state of frame CorrectAt - 1 is restored, the blackboard and the wall go back to the end of that frame
		FMoverTimeStep Rewind;
		Rewind.ServerFrame = FirstFrame + CorrectAt;
		Rewind.BaseSimTimeMs = Rewind.ServerFrame * NetDt * 1000.0;
		Rewind.StepMs = NetDt * 1000.f;
		Rewind.bIsResimulating = true;
		Scene.Blackboard->BeginRollback(Rewind);
		Scene.Blackboard->EndRollback();
		Scene.Frame = Rewind.ServerFrame;
		const FFrameSnap& Authority = Server[CorrectAt - 1];
		Scene.Capsule->SetWorldTransform(Authority.Capsule, false, nullptr, ETeleportType::TeleportPhysics);
		Scene.Wall->SetWorldTransform(Client[CorrectAt - 1].Wall, false, nullptr, ETeleportType::TeleportPhysics);
		// What a sync state carries (MakeTickState); the probe and the contact are tick-local and point into the server's world
		Scene.State = FGSTN_WallTickState();
		Scene.State.WallNormal = Authority.State.WallNormal;
		Scene.State.WallContactPoint = Authority.State.WallContactPoint;
		Scene.State.bHasWall = Authority.State.bHasWall;
		Scene.State.TimeWithoutWall = Authority.State.TimeWithoutWall;
		Scene.State.PullAwayTime = Authority.State.PullAwayTime;
		Scene.State.SurfaceSwitchTicks = Authority.State.SurfaceSwitchTicks;
		Scene.State.Orientation = Authority.Capsule.GetRotation();
		Scene.State.GravityUp = Authority.State.GravityUp;

		// Re-simulation up to the present
		double MaxError = 0.0;
		for (int32 i = CorrectAt; i < Frames; ++i)
		{
			RunFrame(Scene);
			MaxError = FMath::Max(MaxError, (Scene.Capsule->GetComponentLocation() - Server[i].Capsule.GetLocation()).Size());
		}
		const FGSTN_WallBaseInfo Record = Scene.ReadBaseInfo();
		const double RecordError = (Record.LocalPosition - ServerRecord.LocalPosition).Size();
		const bool bSameBase = Record.Base.Get() == Scene.Wall && bServerRidesWall;

		AddInfo(FString::Printf(TEXT("%s: predicted error %.2f cm, after the correction max %.3f cm, base record %.3f cm"),
			bRideFromSynced ? TEXT("ride from synced location") : TEXT("legacy ride"), PredictedError, MaxError, RecordError));
		TestTrue(TEXT("The client's prediction really differed before the correction (not vacuous)"), PredictedError > 15.0);
		TestTrue(TEXT("The wall is still the ridden base after the re-simulation"), bSameBase);
		if (bRideFromSynced)
		{
			TestTrue(*FString::Printf(TEXT("Re-simulated frames equal the server's (max error %.3f cm)"), MaxError), MaxError < 0.1);
			TestTrue(*FString::Printf(TEXT("Base record equals the server's (error %.3f cm)"), RecordError), RecordError < 0.1);
		}
		else
		{
			TestTrue(*FString::Printf(TEXT("Control (legacy ride): the correction is undone, the offset stays (max error %.1f cm)"), MaxError), MaxError > 15.0);
			TestTrue(*FString::Printf(TEXT("Control (legacy ride): the base record keeps the client's own place (error %.1f cm)"), RecordError), RecordError > 15.0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2NetExitBaseVelocityTest,"GSTN.Wall.V2Net.ExitVelocityIncludesBase", GSTNWallNetTest::NetTestFlags)
bool FGSTN_WallV2NetExitBaseVelocityTest::RunTest(const FString& Parameters)
{
	// A wall with a velocity (a body simulated without gravity, so the headless world reports it at once). Leaving it adds the velocity of the
	// wall at the character, linear plus angular, to the wall-relative velocity of the character
	const FVector Linear(0, 200, 50);
	const FVector AngularDeg(0, 0, 30);
	const FVector Own(0, 0, -300);

	for (const bool bAddBaseVelocity : { true, false })
	{
		FNetScene Scene;
		Scene.MakeWall(/*bMovable*/ true);
		Scene.Wall->SetSimulatePhysics(true);
		Scene.Wall->SetEnableGravity(false);
		Scene.Wall->SetPhysicsLinearVelocity(Linear);
		Scene.Wall->SetPhysicsAngularVelocityInDegrees(AngularDeg);
		Scene.MakeCapsule(Scene.Mode->WallGap, 300.f);
		Scene.Mode->bAddBaseVelocityOnExit = bAddBaseVelocity;

		// On the wall one tick, so the base is recorded; then the tick that leaves (a wall jump, a detach: the world part is skipped)
		Scene.RideTick(Own);
		TestTrue(TEXT("The moving wall is the base"), Scene.ReadBaseInfo().Base.Get() == Scene.Wall);
		Scene.RideTick(Own, /*bRide*/ true, /*bLeave*/ true);
		TestTrue(TEXT("The tick leaves"), Scene.State.IsLeaving());

		FVector Expected = Own;
		if (bAddBaseVelocity)
		{
			const FVector R = Scene.Capsule->GetComponentLocation() - Scene.Wall->GetComponentLocation();
			const FVector W = FMath::DegreesToRadians(1.0) * AngularDeg;
			Expected += Linear + FVector::CrossProduct(W, R);
		}
		const double Error = (Scene.State.Velocity - Expected).Size();
		TestTrue(*FString::Printf(TEXT("Exit velocity %s base (%s, expected %s)"), bAddBaseVelocity ? TEXT("includes the") : TEXT("without the"),
			*Scene.State.Velocity.ToString(), *Expected.ToString()), Error < 3.0);
		if (bAddBaseVelocity)
		{
			TestTrue(TEXT("The base really moved the character's exit (not vacuous)"), (Scene.State.Velocity - Own).Size() > 100.0);
		}

		// Whatever happened, a character that left no longer rides anything
		TestTrue(TEXT("The base is forgotten on exit"), !Scene.ReadBaseInfo().Base.IsValid());
	}
	return true;
}

// ------------------------------------------------------------------------------------------------
// Sync state
// ------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2NetSyncStateTest, "GSTN.Wall.V2Net.SyncStateRoundTripWithSurfaceSwitch", GSTNWallNetTest::NetTestFlags)
bool FGSTN_WallV2NetSyncStateTest::RunTest(const FString& Parameters)
{
	auto RoundTrip = [](const FGSTN_WallMovementSyncState& In, FGSTN_WallMovementSyncState& Out)
	{
		FGSTN_WallMovementSyncState Source = In;
		FBitWriter Writer(4096);
		bool bSuccess = false;
		Source.NetSerialize(Writer, nullptr, bSuccess);
		FBitReader Reader(Writer.GetData(), Writer.GetNumBits());
		Out.NetSerialize(Reader, nullptr, bSuccess);
		return bSuccess && !Reader.IsError();
	};

	auto MakeState = [](uint8 Ticks)
	{
		FGSTN_WallMovementSyncState State;
		State.WallNormal = FVector(0.0, -1.0, 0.0);
		State.WallContactPoint = FVector(100.0, 200.0, 300.0);
		State.bHasWall = true;
		State.TimeWithoutWall = 0.04f;
		State.SurfaceSwitchTicks = Ticks;
		return State;
	};

	// The vote of a surface change, tick after tick, with the state going over the wire after every tick (what the server sends and the client resumes
	// from after a correction): the count and the tick that switches are the ones the direct run gives
	const int32 Hysteresis = FGSTN_WallSurfaceModeList().HysteresisTicks;
	uint8 Direct = 0;
	uint8 Wired = 0;
	int32 DirectSwitchTick = -1;
	int32 WiredSwitchTick = -1;
	for (int32 Tick = 1; Tick <= Hysteresis + 4; ++Tick)
	{
		bool bSwitch = false;
		Direct = UGSTN_WallSurfaceModeUtils::StepVote(Direct, true, Hysteresis, bSwitch);
		DirectSwitchTick = (bSwitch && DirectSwitchTick < 0) ? Tick : DirectSwitchTick;

		bSwitch = false;
		Wired = UGSTN_WallSurfaceModeUtils::StepVote(Wired, true, Hysteresis, bSwitch);
		WiredSwitchTick = (bSwitch && WiredSwitchTick < 0) ? Tick : WiredSwitchTick;
		FGSTN_WallMovementSyncState Received;
		TestTrue(TEXT("Round trip succeeds mid vote"), RoundTrip(MakeState(Wired), Received));
		TestEqual(TEXT("The count survives the wire"), static_cast<int32>(Received.SurfaceSwitchTicks), static_cast<int32>(Wired));
		TestFalse(TEXT("A state is equal to its received copy"), MakeState(Wired).ShouldReconcile(Received));
		Wired = Received.SurfaceSwitchTicks;
	}
	TestEqual(TEXT("The wire does not change when the mode switches"), WiredSwitchTick, DirectSwitchTick);
	TestEqual(TEXT("The switch happens on the Nth tick"), WiredSwitchTick, Hysteresis);

	// The count is a counter, not a reconcile reason: a client one tick into a vote the server has not started agrees on the wall, so no correction
	// (a different mode name is what reconciles, and that is Mover's, not this state's)
	TestFalse(TEXT("A different vote count alone does not reconcile"), MakeState(3).ShouldReconcile(MakeState(0)));
	{
		FGSTN_WallMovementSyncState Other = MakeState(3);
		Other.bHasWall = false;
		TestTrue(TEXT("Control: a different wall state does"), MakeState(3).ShouldReconcile(Other));
	}

	// Clone and interpolation carry it (the simulated proxies and the rollback buffer copy states this way)
	{
		const FGSTN_WallMovementSyncState Original = MakeState(5);
		TUniquePtr<FMoverDataStructBase> Clone(Original.Clone());
		TestEqual(TEXT("Clone keeps the count"), static_cast<int32>(static_cast<FGSTN_WallMovementSyncState*>(Clone.Get())->SurfaceSwitchTicks), 5);

		FGSTN_WallMovementSyncState Interpolated;
		Interpolated.Interpolate(MakeState(2), MakeState(6), 0.5f);
		TestEqual(TEXT("Interpolation takes the target's count"), static_cast<int32>(Interpolated.SurfaceSwitchTicks), 6);
	}

	// A zero count over a stale one is restored, and the wire is the smaller one while nothing is voted
	{
		FGSTN_WallMovementSyncState Stale = MakeState(4);
		FGSTN_WallMovementSyncState Zero = MakeState(0);
		TestTrue(TEXT("Round trip succeeds at zero"), RoundTrip(Zero, Stale));
		TestEqual(TEXT("A zero vote is restored over a stale one"), static_cast<int32>(Stale.SurfaceSwitchTicks), 0);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
