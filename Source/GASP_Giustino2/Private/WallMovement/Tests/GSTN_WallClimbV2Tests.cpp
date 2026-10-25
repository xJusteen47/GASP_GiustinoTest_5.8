// Tests of the climb v2 mode and the surface-driven wall mode choice (wall-slide-v2.md 6.3 / 6.4 W4b): GSTN.Wall.V2Climb.*
//
// Same split as the other v2 tests: no Mover pawn, so what runs headless is everything below the tick.
//  - the control model through ComputeWallVelocity (what GenerateMove puts in the FProposedMove), on plain vectors,
//  - the world part of the tick (TraverseStep) plus the vote of the tick (UpdateSurfaceVote) in a throwaway game world with box walls that carry a
//    UGSTN_SurfaceBehaviourComponent (climb-only, slide-only), driven by a working state exactly as SimulationTick builds it,
//  - the entry transition's choice (ChooseModeForHit) on hits against those boxes, and the pure PickMode / StepVote.
// A mode with no Mover component treats every listed mode as registered, so the registered-ness of the climb is tested through PickMode.
// Not covered here (PIE): Activate (the in-place hand-over keeps the sync state), the sync state capture, the transition's full Evaluate.

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
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "WallMovement/MoveLibrary/GSTN_WallControlUtils.h"
#include "WallMovement/MoveLibrary/GSTN_WallMovementUtils.h"
#include "WallMovement/MoveLibrary/GSTN_WallSurfaceModeUtils.h"
#include "WallMovement/Modes/GSTN_WallClimbV2MovementMode.h"
#include "WallMovement/Modes/GSTN_WallSlideV2MovementMode.h"
#include "WallMovement/Transitions/GSTN_WallSurfaceModeTransition.h"
#include "GSTN_WallV2TestAccess.h"

namespace GSTNWallClimbTest
{
	constexpr EAutomationTestFlags ClimbTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float ClimbDt = 1.f / 60.f;
	constexpr float ClimbRadius = 34.f;
	constexpr float ClimbHalfHeight = 88.f;

	static FMoverTickStartData MakeClimbStartState(const FVector& Velocity)
	{
		FMoverTickStartData StartState;
		FMoverDefaultSyncState& Sync = StartState.SyncState.SyncStateCollection.FindOrAddMutableDataByType<FMoverDefaultSyncState>();
		Sync.SetTransforms_WorldSpace(FVector::ZeroVector, FRotator::ZeroRotator, Velocity, FVector::ZeroVector, nullptr);
		return StartState;
	}

	/** A wall normal perpendicular to Up, leaning by TiltDeg around the wall's horizontal axis */
	static FVector ClimbWallNormal(const FVector& Up, float TiltDeg)
	{
		const FVector Seed = FMath::Abs(Up.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
		const FVector Side = FVector::CrossProduct(Up, Seed).GetSafeNormal();
		const FVector Axis = FVector::CrossProduct(Side, Up).GetSafeNormal();
		return FQuat(Axis, FMath::DegreesToRadians(TiltDeg)).RotateVector(Side);
	}

	/** One tick of the climb control model on the plane, the way GenerateMove calls it: the prior velocity is the start state's */
	static FVector ClimbStep(const UGSTN_WallClimbV2MovementMode& Mode, const FVector& Velocity, const FVector& Normal, const FVector& Up, const FVector& Intent)
	{
		const FGSTN_WallMovementPlane Plane = UGSTN_WallMovementUtils::MakeMovementPlane(FQuat::Identity, Normal, Up);
		return FGSTN_WallTraverseTestAccess::ComputeWallVelocity(Mode, MakeClimbStartState(Velocity), Plane, Intent, ClimbDt);
	}

	static void SetBehaviours(UGSTN_SurfaceBehaviourComponent* Surface, const TArray<FGameplayTag>& Tags)
	{
		Surface->Default.Behaviours.Reset();
		for (const FGameplayTag& Tag : Tags)
		{
			Surface->Default.Behaviours.AddTag(Tag);
		}
	}

	/** A throwaway game world with a big box wall (front face through the origin, normal +X) and the climb v2 mode */
	struct FClimbScene
	{
		UWorld* World = nullptr;
		UBoxComponent* Wall = nullptr;
		UGSTN_SurfaceBehaviourComponent* Surface = nullptr;
		UCapsuleComponent* Capsule = nullptr;
		TStrongObjectPtr<UGSTN_WallClimbV2MovementMode> Mode;
		FGSTN_WallTickState State;
		FMovementRecord MoveRecord;
		FVector WallNormal = FVector::ForwardVector;

		FClimbScene()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNWallClimbTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			Mode.Reset(NewObject<UGSTN_WallClimbV2MovementMode>(GetTransientPackage()));
			MoveRecord.SetDeltaSeconds(ClimbDt);
		}

		~FClimbScene()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		/** The wall; with Behaviours it carries a surface component offering exactly those, without it is unmarked geometry */
		void MakeWall(const TArray<FGameplayTag>* Behaviours)
		{
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
			if (Behaviours)
			{
				Surface = NewObject<UGSTN_SurfaceBehaviourComponent>(Actor, TEXT("Surface"));
				SetBehaviours(Surface, *Behaviours);
				Surface->RegisterComponent();
			}
		}

		/** The capsule, upright, facing the wall, its lower hemisphere Gap from the plane, at height Z */
		void MakeCapsule(float Gap, float Z)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			Capsule = NewObject<UCapsuleComponent>(Actor, TEXT("Capsule"));
			Capsule->InitCapsuleSize(ClimbRadius, ClimbHalfHeight);
			Capsule->SetMobility(EComponentMobility::Movable);
			Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Capsule->SetCollisionObjectType(ECC_Pawn);
			Capsule->SetCollisionResponseToAllChannels(ECR_Block);
			Actor->SetRootComponent(Capsule);

			const FVector Up = FVector::UpVector;
			const FQuat Facing = UGSTN_WallTraverseModeBase::ComputeFacingQuat(FQuat::Identity, WallNormal, Up);
			Capsule->SetWorldLocationAndRotation(FVector(ClimbRadius + Gap, 0.0, Z), Facing);
			Capsule->RegisterComponent();

			State = FGSTN_WallTickState();
			State.WallNormal = WallNormal;
			State.GravityUp = Up;
			State.Orientation = Capsule->GetComponentQuat();
			State.bHasWall = true;
		}

		/** The world part of a tick, then the vote of the tick (as SimulationTick does), with this velocity */
		void Tick(const FVector& Velocity)
		{
			State.Velocity = Velocity;
			FGSTN_WallTraverseTestAccess::Step(*Mode, FMovingComponentSet(Capsule), ClimbDt, !Velocity.IsNearlyZero(), State, MoveRecord);
			FGSTN_WallTraverseTestAccess::Vote(*Mode, FMoverSyncState(), State);
		}

		FVector AlongWall(float Speed) const
		{
			FVector H, V;
			UGSTN_WallControlUtils::MakePlaneAxes(WallNormal, FVector::UpVector, H, V);
			return H * Speed;
		}
	};

	static FHitResult MakeClimbFaceHit(UPrimitiveComponent* Component)
	{
		FHitResult Hit(Component ? Component->GetOwner() : nullptr, Component, FVector::ZeroVector, FVector::ForwardVector);
		Hit.Normal = FVector::ForwardVector;
		Hit.ImpactNormal = FVector::ForwardVector;
		return Hit;
	}

	static const TArray<FGameplayTag>& ClimbOnly()
	{
		static const TArray<FGameplayTag> Tags = { GSTNSurfaceTags::Behaviour_WallClimb };
		return Tags;
	}

	static const TArray<FGameplayTag>& SlideOnly()
	{
		static const TArray<FGameplayTag> Tags = { GSTNSurfaceTags::Behaviour_WallSlide };
		return Tags;
	}
}

using namespace GSTNWallClimbTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2ClimbStickAllDirectionsTest, "GSTN.Wall.V2Climb.StickMovesInAllDirections", GSTNWallClimbTest::ClimbTestFlags)
bool FGSTN_WallV2ClimbStickAllDirectionsTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_WallClimbV2MovementMode> Mode(NewObject<UGSTN_WallClimbV2MovementMode>(GetTransientPackage()));
	const float Speed = Mode->Control.ClimbSpeed;

	for (const FVector& Up : { FVector(0, 0, 1), FVector(1, 1, 1).GetSafeNormal() })
	{
		for (float Tilt : { 0.f, 20.f })
		{
			const FVector N = ClimbWallNormal(Up, Tilt);
			FVector H, V;
			UGSTN_WallControlUtils::MakePlaneAxes(N, Up, H, V);

			// Eight directions of the stick on the plane, up and down included: each one reaches its own full speed in that direction
			for (int32 Dir = 0; Dir < 8; ++Dir)
			{
				const float Angle = FMath::DegreesToRadians(Dir * 45.f);
				const FVector Intent = H * FMath::Cos(Angle) + V * FMath::Sin(Angle);
				FVector Velocity = FVector::ZeroVector;
				for (int32 i = 0; i < 90; ++i)
				{
					Velocity = ClimbStep(*Mode, Velocity, N, Up, Intent);
				}
				const FString Label = FString::Printf(TEXT("tilt %.0f dir %d"), Tilt, Dir * 45);
				TestTrue(*(Label + TEXT(": full climb speed")), FMath::Abs(Velocity.Size() - Speed) < 0.5f);
				TestTrue(*(Label + TEXT(": along the stick")), FVector::DotProduct(Velocity.GetSafeNormal(), Intent) > 0.9999f);
				TestTrue(*(Label + TEXT(": on the plane")), FMath::Abs(FVector::DotProduct(Velocity, N)) < 0.01f);
			}

			// Half a stick is half the speed
			FVector Velocity = FVector::ZeroVector;
			for (int32 i = 0; i < 90; ++i)
			{
				Velocity = ClimbStep(*Mode, Velocity, N, Up, V * 0.5f);
			}
			TestTrue(TEXT("Half stick up: half the speed"), FMath::Abs(FVector::DotProduct(Velocity, V) - Speed * 0.5f) < 0.5f);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2ClimbNoSlideTest, "GSTN.Wall.V2Climb.NoSlideWhileHolding", GSTNWallClimbTest::ClimbTestFlags)
bool FGSTN_WallV2ClimbNoSlideTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_WallClimbV2MovementMode> Mode(NewObject<UGSTN_WallClimbV2MovementMode>(GetTransientPackage()));
	const FVector Up = FVector::UpVector;
	const FVector N = FVector::ForwardVector;
	FVector H, V;
	UGSTN_WallControlUtils::MakePlaneAxes(N, Up, H, V);

	// Hanging still: no stick, no speed, for two seconds. A slide would have picked up the wall's gravity
	FVector Velocity = FVector::ZeroVector;
	for (int32 i = 0; i < 120; ++i)
	{
		Velocity = ClimbStep(*Mode, Velocity, N, Up, FVector::ZeroVector);
	}
	TestTrue(TEXT("Hanging: still"), Velocity.IsNearlyZero(1e-3f));

	// Holding a partial stick: the speed settles and stays, with no drift toward the ground
	for (int32 i = 0; i < 60; ++i)
	{
		Velocity = ClimbStep(*Mode, Velocity, N, Up, H * 0.4f);
	}
	const FVector Settled = Velocity;
	for (int32 i = 0; i < 120; ++i)
	{
		Velocity = ClimbStep(*Mode, Velocity, N, Up, H * 0.4f);
	}
	TestTrue(TEXT("Holding sideways: the speed does not change"), (Velocity - Settled).IsNearlyZero(1e-3f));
	TestTrue(TEXT("Holding sideways: no fall"), FMath::Abs(FVector::DotProduct(Velocity, V)) < 1e-3f);

	// Holding up at full stick and then down: never slower than asked and never pulled
	for (int32 i = 0; i < 60; ++i)
	{
		Velocity = ClimbStep(*Mode, Velocity, N, Up, V);
	}
	TestTrue(TEXT("Climbing up: the full speed up"), FMath::Abs(FVector::DotProduct(Velocity, V) - Mode->Control.ClimbSpeed) < 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2ClimbReleaseBrakesTest, "GSTN.Wall.V2Climb.ReleaseBrakesToStop", GSTNWallClimbTest::ClimbTestFlags)
bool FGSTN_WallV2ClimbReleaseBrakesTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_WallClimbV2MovementMode> Mode(NewObject<UGSTN_WallClimbV2MovementMode>(GetTransientPackage()));
	const FVector Up = FVector::UpVector;
	const FVector N = FVector::ForwardVector;
	FVector H, V;
	UGSTN_WallControlUtils::MakePlaneAxes(N, Up, H, V);

	// A diagonal climb at full speed, then the stick is let go
	const FVector Heading = (H + V).GetSafeNormal();
	FVector Velocity = Heading * Mode->Control.ClimbSpeed;
	const float ExpectedTicks = Mode->Control.ClimbSpeed / Mode->Control.ClimbBrake / ClimbDt;

	int32 TicksToStop = 0;
	float PrevSpeed = Velocity.Size();
	bool bNeverFaster = true;
	bool bNeverReversed = true;
	while (!Velocity.IsNearlyZero(1e-3f) && TicksToStop < 120)
	{
		Velocity = ClimbStep(*Mode, Velocity, N, Up, FVector::ZeroVector);
		++TicksToStop;
		bNeverFaster &= Velocity.Size() <= PrevSpeed + 1e-3f;
		bNeverReversed &= FVector::DotProduct(Velocity, Heading) >= -1e-3f;
		PrevSpeed = Velocity.Size();
	}
	TestTrue(TEXT("Released: it comes to a complete stop"), Velocity.IsNearlyZero(1e-3f));
	TestTrue(*FString::Printf(TEXT("Released: the stop is strong (%d ticks, expected about %.1f)"), TicksToStop, ExpectedTicks),
		TicksToStop <= FMath::CeilToInt(ExpectedTicks) + 1);
	TestTrue(TEXT("Released: the speed only falls"), bNeverFaster);
	TestTrue(TEXT("Released: it never rolls back"), bNeverReversed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2ClimbOnlyClimbSurfacesTest, "GSTN.Wall.V2Climb.OnlyOnClimbSurfaces", GSTNWallClimbTest::ClimbTestFlags)
bool FGSTN_WallV2ClimbOnlyClimbSurfacesTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_WallClimbV2MovementMode> Defaults(NewObject<UGSTN_WallClimbV2MovementMode>(GetTransientPackage()));
	TestFalse(TEXT("The default filter asks for WallClimb"), Defaults->SurfaceFilter.RequiredBehaviours.IsEmpty());
	TestFalse(TEXT("Unmarked surfaces are not climb walls"), Defaults->SurfaceFilter.bUnknownSurfacesPass);
	TestFalse(TEXT("The capsule tilt opt-in is off by default"), Defaults->bAllowCapsuleTilt);

	struct FCase
	{
		const TCHAR* Label;
		const TArray<FGameplayTag>* Behaviours;
		bool bExpectWall;
	};
	const TArray<FGameplayTag> Both = { GSTNSurfaceTags::Behaviour_WallClimb, GSTNSurfaceTags::Behaviour_WallSlide };
	const TArray<FGameplayTag> StandOnly = { GSTNSurfaceTags::Behaviour_Stand };
	const FCase Cases[] = {
		{ TEXT("climb-only surface"), &ClimbOnly(), true },
		{ TEXT("climb and slide surface"), &Both, true },
		{ TEXT("slide-only surface"), &SlideOnly(), false },
		{ TEXT("stand-only surface"), &StandOnly, false },
		{ TEXT("unmarked surface"), nullptr, false },
	};

	for (const FCase& Case : Cases)
	{
		FClimbScene Scene;
		// Stand-alone: no surface mode list, so the mode's own filter is the only judge
		Scene.Mode->SurfaceModes.bEnabled = false;
		Scene.MakeWall(Case.Behaviours);
		Scene.MakeCapsule(4.f, 300.f);
		Scene.Tick(FVector::ZeroVector);
		TestTrue(*FString::Printf(TEXT("%s: wall found = %d"), Case.Label, Case.bExpectWall ? 1 : 0), Scene.State.bHasWall == Case.bExpectWall);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2ClimbTransitionPicksClimbTest, "GSTN.Wall.V2Climb.TransitionPicksClimbOnClimbSurface", GSTNWallClimbTest::ClimbTestFlags)
bool FGSTN_WallV2ClimbTransitionPicksClimbTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_WallSurfaceModeTransition> Transition(NewObject<UGSTN_WallSurfaceModeTransition>(GetTransientPackage()));

	const TArray<FGameplayTag> Both = { GSTNSurfaceTags::Behaviour_WallClimb, GSTNSurfaceTags::Behaviour_WallSlide };
	{
		FClimbScene Scene;
		Scene.MakeWall(&ClimbOnly());
		TestEqual(TEXT("A climb-only surface is grabbed by the climb"), Transition->ChooseModeForHit(MakeClimbFaceHit(Scene.Wall), nullptr),
			UGSTN_WallSurfaceModeUtils::ClimbModeName);
	}
	{
		FClimbScene Scene;
		Scene.MakeWall(&Both);
		TestEqual(TEXT("A surface offering both: the list order puts the climb first"), Transition->ChooseModeForHit(MakeClimbFaceHit(Scene.Wall), nullptr),
			UGSTN_WallSurfaceModeUtils::ClimbModeName);
	}

	// The list is data: a project can put the slide first, and a registered-ness check skips a mode that is not there
	FGSTN_WallSurfaceModeList Swapped = UGSTN_WallSurfaceModeUtils::MakeDefaultList();
	TestEqual(TEXT("The default list has the two entries"), Swapped.Entries.Num(), 2);
	if (Swapped.Entries.Num() == 2)
	{
		TestEqual(TEXT("Default order: climb first"), Swapped.Entries[0].ModeName, UGSTN_WallSurfaceModeUtils::ClimbModeName);
		Swapped.Entries.Swap(0, 1);
		Transition->SurfaceModes = Swapped;
		FClimbScene Scene;
		Scene.MakeWall(&Both);
		TestEqual(TEXT("Swapped list: the slide wins on a surface offering both"), Transition->ChooseModeForHit(MakeClimbFaceHit(Scene.Wall), nullptr),
			UGSTN_WallSurfaceModeUtils::SlideModeName);
	}

	const FGSTN_WallSurfaceModeList Defaults = UGSTN_WallSurfaceModeUtils::MakeDefaultList();
	FGameplayTagContainer BothTags;
	BothTags.AddTag(GSTNSurfaceTags::Behaviour_WallClimb);
	BothTags.AddTag(GSTNSurfaceTags::Behaviour_WallSlide);
	bool bBlocked = false;
	const auto NeverBlocked = [](const FGameplayTagQuery&) { return false; };
	TestEqual(TEXT("Climb mode not registered: the slide takes the surface"),
		UGSTN_WallSurfaceModeUtils::PickMode(Defaults, BothTags, false, [](FName Mode) { return Mode == UGSTN_WallSurfaceModeUtils::SlideModeName; }, NeverBlocked, bBlocked),
		UGSTN_WallSurfaceModeUtils::SlideModeName);

	// The stamina hook: a blocked climb is skipped; if nothing else takes the surface the caller is told it was blocked
	FGSTN_WallSurfaceModeList Stamina = Defaults;
	Stamina.Entries[0].BlockedWhen = FGameplayTagQuery::MakeQuery_MatchTag(GSTNSurfaceTags::Behaviour_Slippery);
	const auto AllRegistered = [](FName) { return true; };
	const auto BlockedIfSet = [](const FGameplayTagQuery& Query) { return !Query.IsEmpty(); };
	TestEqual(TEXT("Exhausted on a surface that also slides: the slide"),
		UGSTN_WallSurfaceModeUtils::PickMode(Stamina, BothTags, false, AllRegistered, BlockedIfSet, bBlocked), UGSTN_WallSurfaceModeUtils::SlideModeName);
	TestFalse(TEXT("... which is not a blocked result"), bBlocked);
	FGameplayTagContainer ClimbTags;
	ClimbTags.AddTag(GSTNSurfaceTags::Behaviour_WallClimb);
	TestTrue(TEXT("Exhausted on a climb-only surface: nothing takes it"),
		UGSTN_WallSurfaceModeUtils::PickMode(Stamina, ClimbTags, false, AllRegistered, BlockedIfSet, bBlocked).IsNone());
	TestTrue(TEXT("... and it is reported as blocked"), bBlocked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2ClimbTransitionPicksSlideTest, "GSTN.Wall.V2Climb.TransitionPicksSlideOtherwise", GSTNWallClimbTest::ClimbTestFlags)
bool FGSTN_WallV2ClimbTransitionPicksSlideTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGSTN_WallSurfaceModeTransition> Transition(NewObject<UGSTN_WallSurfaceModeTransition>(GetTransientPackage()));

	{
		FClimbScene Scene;
		Scene.MakeWall(&SlideOnly());
		TestEqual(TEXT("A slide-only surface is grabbed by the slide"), Transition->ChooseModeForHit(MakeClimbFaceHit(Scene.Wall), nullptr),
			UGSTN_WallSurfaceModeUtils::SlideModeName);
	}
	{
		// Nothing marked it: the catch-all default offers WallClimb too, and must not turn every unauthored wall into a climb wall
		FClimbScene Scene;
		Scene.MakeWall(nullptr);
		TestEqual(TEXT("An unmarked surface is grabbed by the slide"), Transition->ChooseModeForHit(MakeClimbFaceHit(Scene.Wall), nullptr),
			UGSTN_WallSurfaceModeUtils::SlideModeName);
	}
	{
		const TArray<FGameplayTag> StandOnly = { GSTNSurfaceTags::Behaviour_Stand };
		FClimbScene Scene;
		Scene.MakeWall(&StandOnly);
		TestTrue(TEXT("A surface that offers neither is not a wall"), Transition->ChooseModeForHit(MakeClimbFaceHit(Scene.Wall), nullptr).IsNone());
	}
	{
		// Unmarked surfaces can be refused too
		Transition->SurfaceModes.UnknownSurfaceModeName = NAME_None;
		FClimbScene Scene;
		Scene.MakeWall(nullptr);
		TestTrue(TEXT("No unknown-surface mode: unmarked geometry is refused"), Transition->ChooseModeForHit(MakeClimbFaceHit(Scene.Wall), nullptr).IsNone());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2ClimbSwitchInPlaceTest, "GSTN.Wall.V2Climb.SwitchInPlaceKeepsVelocity", GSTNWallClimbTest::ClimbTestFlags)
bool FGSTN_WallV2ClimbSwitchInPlaceTest::RunTest(const FString& Parameters)
{
	FClimbScene Scene;
	Scene.MakeWall(&ClimbOnly());
	Scene.MakeCapsule(4.f, 300.f);
	FGSTN_WallTraverseTestAccess::SetRegisteredName(*Scene.Mode, UGSTN_WallSurfaceModeUtils::ClimbModeName);
	const int32 Hysteresis = Scene.Mode->SurfaceModes.HysteresisTicks;
	const FVector Velocity = Scene.AlongWall(200.f);

	// Mover hands over in place only between the modes of the list
	TestTrue(TEXT("From the slide is an in-place switch"), Scene.Mode->IsInPlaceSwitchFrom(UGSTN_WallSurfaceModeUtils::SlideModeName));
	TestFalse(TEXT("From Falling is a grab, not a switch"), Scene.Mode->IsInPlaceSwitchFrom(DefaultModeNames::Falling));
	TestFalse(TEXT("From itself is not a switch"), Scene.Mode->IsInPlaceSwitchFrom(UGSTN_WallSurfaceModeUtils::ClimbModeName));

	// On its own kind of surface the climb stays
	for (int32 i = 0; i < 10; ++i)
	{
		Scene.Tick(Velocity);
	}
	TestFalse(TEXT("Climb surface: no switch"), Scene.State.IsLeaving());
	TestTrue(TEXT("Climb surface: the wall is held"), Scene.State.bHasWall);
	TestEqual(TEXT("Climb surface: no vote"), static_cast<int32>(Scene.State.SurfaceSwitchTicks), 0);

	// The contact crosses onto a slide-only surface: the probes still hold the wall (the climb accepts what it can hand over to), the vote counts
	SetBehaviours(Scene.Surface, SlideOnly());
	for (int32 i = 1; i < Hysteresis; ++i)
	{
		Scene.Tick(Velocity);
		TestFalse(*FString::Printf(TEXT("Tick %d of %d on the slide surface: not yet"), i, Hysteresis), Scene.State.IsLeaving());
		TestEqual(*FString::Printf(TEXT("Tick %d: the vote counts"), i), static_cast<int32>(Scene.State.SurfaceSwitchTicks), i);
		TestTrue(*FString::Printf(TEXT("Tick %d: the wall is held"), i), Scene.State.bHasWall);
	}
	Scene.Tick(Velocity);
	TestTrue(TEXT("Hysteresis reached: the slide is requested"), Scene.State.IsLeaving());
	TestEqual(TEXT("... which is the slide"), Scene.State.NextModeName, UGSTN_WallSurfaceModeUtils::SlideModeName);
	TestEqual(TEXT("... at once, no time given back"), Scene.State.UnusedSeconds, 0.f);
	TestTrue(TEXT("The wall is still held when it switches"), Scene.State.bHasWall);
	// Velocity is kept: the transport is length preserving on a flat wall and the switch itself touches nothing
	TestTrue(*FString::Printf(TEXT("Velocity kept across the switch (%.2f)"), Scene.State.Velocity.Size()), FMath::Abs(Scene.State.Velocity.Size() - 200.f) < 0.5f);
	TestTrue(TEXT("... in the same direction"), FVector::DotProduct(Scene.State.Velocity.GetSafeNormal(), Velocity.GetSafeNormal()) > 0.9999f);

	// And back: the slide mode on a climb-only surface asks for the climb the same way
	{
		FClimbScene Back;
		Back.MakeWall(&ClimbOnly());
		Back.MakeCapsule(4.f, 300.f);
		TStrongObjectPtr<UGSTN_WallSlideV2MovementMode> Slide(NewObject<UGSTN_WallSlideV2MovementMode>(GetTransientPackage()));
		FGSTN_WallTraverseTestAccess::SetRegisteredName(*Slide, UGSTN_WallSurfaceModeUtils::SlideModeName);
		for (int32 i = 0; i < Hysteresis; ++i)
		{
			Back.State.Velocity = Velocity;
			FGSTN_WallTraverseTestAccess::Step(*Slide, FMovingComponentSet(Back.Capsule), ClimbDt, true, Back.State, Back.MoveRecord);
			FGSTN_WallTraverseTestAccess::Vote(*Slide, FMoverSyncState(), Back.State);
		}
		TestEqual(TEXT("The slide on a climb surface requests the climb"), Back.State.NextModeName, UGSTN_WallSurfaceModeUtils::ClimbModeName);
	}

	// A mode that is not registered (no name) never votes
	{
		FClimbScene Loose;
		Loose.MakeWall(&SlideOnly());
		Loose.MakeCapsule(4.f, 300.f);
		for (int32 i = 0; i < Hysteresis + 2; ++i)
		{
			Loose.Tick(Velocity);
		}
		TestFalse(TEXT("Not registered: never switches"), Loose.State.IsLeaving());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallV2ClimbNoFlappingTest, "GSTN.Wall.V2Climb.NoFlappingOnMixedSeam", GSTNWallClimbTest::ClimbTestFlags)
bool FGSTN_WallV2ClimbNoFlappingTest::RunTest(const FString& Parameters)
{
	// The pure hysteresis: only an unbroken run of N asks switches
	{
		bool bSwitch = false;
		uint8 Votes = 0;
		int32 Switches = 0;
		for (int32 i = 0; i < 100; ++i)
		{
			Votes = UGSTN_WallSurfaceModeUtils::StepVote(Votes, (i % 2) == 0, 6, bSwitch);
			Switches += bSwitch ? 1 : 0;
		}
		TestEqual(TEXT("A vote that flips every tick never switches"), Switches, 0);

		Votes = 0;
		for (int32 i = 0; i < 100; ++i)
		{
			Votes = UGSTN_WallSurfaceModeUtils::StepVote(Votes, (i % 6) != 5, 6, bSwitch);
			Switches += bSwitch ? 1 : 0;
		}
		TestEqual(TEXT("Five asks and a break, over and over, never switches"), Switches, 0);

		Votes = 0;
		int32 TickOfSwitch = -1;
		for (int32 i = 0; i < 20 && TickOfSwitch < 0; ++i)
		{
			Votes = UGSTN_WallSurfaceModeUtils::StepVote(Votes, true, 6, bSwitch);
			TickOfSwitch = bSwitch ? i + 1 : TickOfSwitch;
		}
		TestEqual(TEXT("A sustained ask switches on the Nth tick"), TickOfSwitch, 6);
		Votes = UGSTN_WallSurfaceModeUtils::StepVote(250, true, 1, bSwitch);
		TestTrue(TEXT("Hysteresis 1 switches at once"), bSwitch);
		TestEqual(TEXT("The count saturates"), static_cast<int32>(UGSTN_WallSurfaceModeUtils::StepVote(255, true, 300, bSwitch)), 255);
	}

	// The mode, on a seam: the contact surface changes kind from one tick to the next (the component is re-marked between ticks, which is
	// what a capsule straddling two kinds of geometry looks like to the vote)
	FClimbScene Scene;
	Scene.MakeWall(&ClimbOnly());
	Scene.MakeCapsule(4.f, 300.f);
	FGSTN_WallTraverseTestAccess::SetRegisteredName(*Scene.Mode, UGSTN_WallSurfaceModeUtils::ClimbModeName);
	const int32 Hysteresis = Scene.Mode->SurfaceModes.HysteresisTicks;
	const FVector Velocity = Scene.AlongWall(150.f);

	int32 MaxVote = 0;
	bool bEverLeft = false;
	bool bAlwaysOnWall = true;
	for (int32 i = 0; i < 60; ++i)
	{
		SetBehaviours(Scene.Surface, (i % 2) == 0 ? SlideOnly() : ClimbOnly());
		Scene.Tick(Velocity);
		MaxVote = FMath::Max(MaxVote, static_cast<int32>(Scene.State.SurfaceSwitchTicks));
		bEverLeft |= Scene.State.IsLeaving();
		bAlwaysOnWall &= Scene.State.bHasWall;
	}
	TestFalse(TEXT("Alternating every tick: the mode never switches"), bEverLeft);
	TestTrue(*FString::Printf(TEXT("Alternating every tick: the vote never builds (max %d)"), MaxVote), MaxVote <= 1);
	TestTrue(TEXT("Alternating every tick: the wall is never lost on the seam"), bAlwaysOnWall);

	// Mostly one kind with a break: still no switch
	for (int32 i = 0; i < 60; ++i)
	{
		SetBehaviours(Scene.Surface, (i % Hysteresis) == Hysteresis - 1 ? ClimbOnly() : SlideOnly());
		Scene.Tick(Velocity);
		bEverLeft |= Scene.State.IsLeaving();
	}
	TestFalse(TEXT("Runs shorter than the hysteresis never switch"), bEverLeft);

	// Settling on the other kind does switch, once
	SetBehaviours(Scene.Surface, SlideOnly());
	for (int32 i = 0; i < Hysteresis && !Scene.State.IsLeaving(); ++i)
	{
		Scene.Tick(Velocity);
	}
	TestTrue(TEXT("A settled change of surface switches"), Scene.State.IsLeaving());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
