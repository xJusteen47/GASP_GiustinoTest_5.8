// Automation tests of the wall surface filter (traversal-surfaces.md §5.2, plan T5): GSTNMovement.Wall.Filter.*
//
// They call the query functions in a throwaway game world with box colliders; a Mover component and the entry transition's
// sweep need a full simulation tick, so that path is covered by the PIE scenario scenario_surface_nowallslide (PIE).
// The transition shares UGSTN_WallQueryUtils::PassesSurfaceFilter with the checks tested here.

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "GSTN_SurfaceBehaviourComponent.h"
#include "GSTN_SurfaceTags.h"
#include "GSTN_SurfaceTypes.h"
#include "GSTN_SurfaceUtils.h"
#include "Misc/AutomationTest.h"
#include "MoveLibrary/MovementUtilsTypes.h"
#include "WallMovement/GSTN_WallMovementTypes.h"
#include "WallMovement/MoveLibrary/GSTN_WallQueryUtils.h"

namespace GSTNWallFilterTest
{
	constexpr EAutomationTestFlags FilterTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A throwaway game world to own the test actors */
	struct FRig
	{
		UWorld* World = nullptr;

		FRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNWallFilterTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
		}

		~FRig()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		/** An actor with one box collider and, when Behaviours is given, a surface component offering exactly those behaviours */
		UBoxComponent* SpawnWall(const FVector& Location, const TArray<FGameplayTag>* Behaviours)
		{
			AActor* Actor = World->SpawnActor<AActor>(Location, FRotator::ZeroRotator);
			UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("Box"));
			Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Box->RegisterComponent();
			if (Behaviours)
			{
				UGSTN_SurfaceBehaviourComponent* Surface = NewObject<UGSTN_SurfaceBehaviourComponent>(Actor, TEXT("Surface"));
				for (const FGameplayTag& Tag : *Behaviours)
				{
					Surface->Default.Behaviours.AddTag(Tag);
				}
				Surface->RegisterComponent();
			}
			return Box;
		}
	};

	/** A face contact against a wall facing +X: Normal equals ImpactNormal, so no edge trace is involved */
	FHitResult MakeFaceHit(UPrimitiveComponent* Component, const FVector& Normal = FVector::ForwardVector)
	{
		FHitResult Hit(Component ? Component->GetOwner() : nullptr, Component, FVector::ZeroVector, Normal);
		Hit.Normal = Normal;
		Hit.ImpactNormal = Normal;
		return Hit;
	}

	FGSTN_WallSurfaceFilter MakeFilter(const FGameplayTag& Required, bool bUnknownPass = true)
	{
		FGSTN_WallSurfaceFilter Filter;
		if (Required.IsValid())
		{
			Filter.RequiredBehaviours = FGameplayTagQuery::MakeQuery_MatchTag(Required);
		}
		Filter.bUnknownSurfacesPass = bUnknownPass;
		return Filter;
	}

	bool IsValidWall(const UWorld* World, const FHitResult& Hit, const FGSTN_WallSurfaceFilter& Filter)
	{
		FGSTN_WallCheckSettings Settings;
		Settings.SurfaceFilter = Filter;
		return UGSTN_WallQueryUtils::IsHitOnValidWall(World, Hit, FVector::UpVector, Settings, ECC_Pawn,
			FCollisionQueryParams(), FCollisionResponseParams());
	}
}

using namespace GSTNWallFilterTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallFilterForbiddenSurfaceRejectedTest, "GSTNMovement.Wall.Filter.ForbiddenSurfaceRejected", FilterTestFlags)
bool FGSTN_WallFilterForbiddenSurfaceRejectedTest::RunTest(const FString& Parameters)
{
	FRig Rig;
	// A marked surface that offers standing but not wall slide
	const TArray<FGameplayTag> StandOnly = { GSTNSurfaceTags::Behaviour_Stand };
	UBoxComponent* Forbidden = Rig.SpawnWall(FVector::ZeroVector, &StandOnly);
	const FHitResult Hit = MakeFaceHit(Forbidden);

	TestTrue(TEXT("The surface is marked, not the unknown default"),
		UGSTN_SurfaceUtils::Resolve(Hit).Source == EGSTN_SurfaceSource::Component);

	const FGSTN_WallSurfaceFilter Slide = MakeFilter(GSTNSurfaceTags::Behaviour_WallSlide);
	TestFalse(TEXT("A surface without WallSlide is not a wall for a slide filter"), IsValidWall(Rig.World, Hit, Slide));
	TestFalse(TEXT("Unknown-pass does not rescue a marked surface"),
		IsValidWall(Rig.World, Hit, MakeFilter(GSTNSurfaceTags::Behaviour_WallSlide, /*bUnknownPass*/ true)));
	TestTrue(TEXT("Empty filter: today's behaviour, the same hit is a wall"), IsValidWall(Rig.World, Hit, FGSTN_WallSurfaceFilter()));
	TestTrue(TEXT("Standing is offered, so a Stand filter accepts it"),
		IsValidWall(Rig.World, Hit, MakeFilter(GSTNSurfaceTags::Behaviour_Stand)));

	// The angle test still comes first: an allowed surface facing up is not a wall whatever the filter says
	const TArray<FGameplayTag> SlideOk = { GSTNSurfaceTags::Behaviour_WallSlide };
	UBoxComponent* Allowed = Rig.SpawnWall(FVector(500.0, 0.0, 0.0), &SlideOk);
	TestTrue(TEXT("An allowed vertical face is a wall"), IsValidWall(Rig.World, MakeFaceHit(Allowed), Slide));
	TestFalse(TEXT("An allowed floor is still not a wall"), IsValidWall(Rig.World, MakeFaceHit(Allowed, FVector::UpVector), Slide));

	// PassesSurfaceFilter on its own, the function the entry transition shares
	TestFalse(TEXT("PassesSurfaceFilter rejects the forbidden surface"), UGSTN_WallQueryUtils::PassesSurfaceFilter(Hit, Slide));
	TestTrue(TEXT("PassesSurfaceFilter accepts the allowed one"),
		UGSTN_WallQueryUtils::PassesSurfaceFilter(MakeFaceHit(Allowed), Slide));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallFilterUnknownPassesTest, "GSTNMovement.Wall.Filter.UnknownPasses", FilterTestFlags)
bool FGSTN_WallFilterUnknownPassesTest::RunTest(const FString& Parameters)
{
	FRig Rig;
	UBoxComponent* Unmarked = Rig.SpawnWall(FVector::ZeroVector, nullptr);
	const FHitResult Hit = MakeFaceHit(Unmarked);

	TestTrue(TEXT("Nothing marks the geometry: it resolves to the default"),
		UGSTN_SurfaceUtils::Resolve(Hit).Source == EGSTN_SurfaceSource::Default);

	const FGameplayTag Climb = GSTNSurfaceTags::Behaviour_WallClimb;
	TestTrue(TEXT("Unknown surfaces pass by default"), IsValidWall(Rig.World, Hit, MakeFilter(Climb)));
	TestFalse(TEXT("Unknown surfaces are rejected when the filter says so"),
		IsValidWall(Rig.World, Hit, MakeFilter(Climb, /*bUnknownPass*/ false)));
	TestTrue(TEXT("An empty query ignores bUnknownSurfacesPass: nothing to filter"),
		IsValidWall(Rig.World, Hit, MakeFilter(FGameplayTag(), /*bUnknownPass*/ false)));

	// A hit without any collider (a synthetic or destroyed one) is unknown too
	FHitResult NoComponent = MakeFaceHit(nullptr);
	TestTrue(TEXT("A hit without a collider is unknown and passes"), IsValidWall(Rig.World, NoComponent, MakeFilter(Climb)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallFilterClimbOnlyRejectsSlideTest, "GSTNMovement.Wall.Filter.ClimbOnlyRejectsSlide", FilterTestFlags)
bool FGSTN_WallFilterClimbOnlyRejectsSlideTest::RunTest(const FString& Parameters)
{
	FRig Rig;
	const TArray<FGameplayTag> ClimbOnly = { GSTNSurfaceTags::Behaviour_WallClimb };
	const TArray<FGameplayTag> SlideOnly = { GSTNSurfaceTags::Behaviour_WallSlide };
	const FHitResult ClimbHit = MakeFaceHit(Rig.SpawnWall(FVector::ZeroVector, &ClimbOnly));
	const FHitResult SlideHit = MakeFaceHit(Rig.SpawnWall(FVector(500.0, 0.0, 0.0), &SlideOnly));

	const FGSTN_WallSurfaceFilter SlideMode = MakeFilter(GSTNSurfaceTags::Behaviour_WallSlide);
	const FGSTN_WallSurfaceFilter ClimbMode = MakeFilter(GSTNSurfaceTags::Behaviour_WallClimb);

	TestFalse(TEXT("A climb-only wall is rejected by the slide mode"), IsValidWall(Rig.World, ClimbHit, SlideMode));
	TestTrue(TEXT("A climb-only wall is accepted by the climb mode"), IsValidWall(Rig.World, ClimbHit, ClimbMode));
	TestTrue(TEXT("A slide-only wall is accepted by the slide mode"), IsValidWall(Rig.World, SlideHit, SlideMode));
	TestFalse(TEXT("A slide-only wall is rejected by the climb mode"), IsValidWall(Rig.World, SlideHit, ClimbMode));

	// A mode may accept either behaviour (a query on the container, not a single tag)
	FGameplayTagContainer Either;
	Either.AddTag(GSTNSurfaceTags::Behaviour_WallSlide);
	Either.AddTag(GSTNSurfaceTags::Behaviour_WallClimb);
	FGSTN_WallSurfaceFilter AnyOf;
	AnyOf.RequiredBehaviours = FGameplayTagQuery::BuildQuery(FGameplayTagQueryExpression().AnyTagsMatch().AddTags(Either));
	TestTrue(TEXT("An any-of query accepts the climb-only wall"), IsValidWall(Rig.World, ClimbHit, AnyOf));
	TestTrue(TEXT("An any-of query accepts the slide-only wall"), IsValidWall(Rig.World, SlideHit, AnyOf));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_WallFilterCornerOntoForbiddenRejectedTest, "GSTNMovement.Wall.Filter.CornerOntoForbiddenRejected", FilterTestFlags)
bool FGSTN_WallFilterCornerOntoForbiddenRejectedTest::RunTest(const FString& Parameters)
{
	FRig Rig;
	const TArray<FGameplayTag> SlideOk = { GSTNSurfaceTags::Behaviour_WallSlide };
	const TArray<FGameplayTag> StandOnly = { GSTNSurfaceTags::Behaviour_Stand };
	UBoxComponent* Allowed = Rig.SpawnWall(FVector(0.0, 500.0, 0.0), &SlideOk);
	UBoxComponent* Forbidden = Rig.SpawnWall(FVector(0.0, 1000.0, 0.0), &StandOnly);

	// The character stands on a wall facing +X (far away from everything) and meets a wall facing +Y: an inside corner of 90 degrees
	AActor* Character = Rig.World->SpawnActor<AActor>(FVector(5000.0, 0.0, 0.0), FRotator::ZeroRotator);
	UCapsuleComponent* Capsule = NewObject<UCapsuleComponent>(Character, TEXT("Capsule"));
	Capsule->InitCapsuleSize(34.f, 88.f);
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Capsule->RegisterComponent();
	Character->SetRootComponent(Capsule);
	const FMovingComponentSet MovingComps(Capsule);

	const FVector WallNormal = FVector::ForwardVector;
	const FVector CornerNormal = FVector::RightVector;
	const FVector MoveDelta = -CornerNormal * 10.0;
	const FQuat Orientation = FQuat::Identity;

	auto CornerOnto = [&](UBoxComponent* Box, const FGSTN_WallSurfaceFilter& Filter)
	{
		FHitResult Hit = MakeFaceHit(Box, CornerNormal);
		Hit.ImpactPoint = Capsule->GetComponentLocation() + FVector(-40.0, 40.0, 0.0);
		FGSTN_WallCheckSettings Settings;
		Settings.SurfaceFilter = Filter;
		return UGSTN_WallQueryUtils::IsConcaveCornerHit(MovingComps, Hit, MoveDelta, Orientation, WallNormal, FVector::UpVector,
			/*MinCornerAngle*/ 5.f, Settings);
	};

	const FGSTN_WallSurfaceFilter Slide = MakeFilter(GSTNSurfaceTags::Behaviour_WallSlide);
	TestTrue(TEXT("Baseline: the corner onto an allowed wall is accepted"), CornerOnto(Allowed, Slide));
	TestFalse(TEXT("The corner onto a surface without WallSlide is rejected"), CornerOnto(Forbidden, Slide));
	TestTrue(TEXT("Empty filter: the same corner is accepted, as before"), CornerOnto(Forbidden, FGSTN_WallSurfaceFilter()));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
