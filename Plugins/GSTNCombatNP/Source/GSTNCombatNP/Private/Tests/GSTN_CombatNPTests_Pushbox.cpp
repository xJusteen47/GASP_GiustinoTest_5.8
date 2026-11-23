// Pushbox plan (2026-11-15): fighters' movement capsules ignore each other from registration, ref-counted so a link partner ignore ending does
// not turn the blocking back on, and a swept capsule passes through another fighter's capsule.

#include "Components/CapsuleComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "World/GSTN_CombatWorldSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

#define GSTN_NP_TEST_FLAGS (EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	struct FPushboxFakeSource : IGSTN_SnapshotSource
	{
		virtual bool ReadSnapshotEntry(int32 Frame, FGSTN_SnapshotEntry& Out) override { return true; }
	};

	/** A transient world with two actors whose root is a pawn-blocking capsule */
	struct FPushboxRig
	{
		FPushboxRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNCombatNPPushboxTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			A = Spawn(FVector(0.0, 0.0, 1000.0));
			B = Spawn(FVector(200.0, 0.0, 1000.0));
		}

		~FPushboxRig()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		AActor* Spawn(const FVector& Location)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UCapsuleComponent* Capsule = NewObject<UCapsuleComponent>(Actor);
			Capsule->InitCapsuleSize(35.f, 90.f);
			Capsule->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
			Actor->SetRootComponent(Capsule);
			Capsule->RegisterComponent();
			Actor->SetActorLocation(Location);
			return Actor;
		}

		static bool Ignores(const AActor* Owner, const AActor* Other)
		{
			const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
			return Root && Root->GetMoveIgnoreActors().Contains(Other);
		}

		UWorld* World = nullptr;
		AActor* A = nullptr;
		AActor* B = nullptr;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_NPPushboxIgnoreTest, "GSTNCombat.NP.Pushbox.CapsuleIgnore", GSTN_NP_TEST_FLAGS)
bool FGSTN_NPPushboxIgnoreTest::RunTest(const FString& Parameters)
{
	FPushboxRig Rig;
	UGSTN_CombatWorldSubsystem* Subsystem = NewObject<UGSTN_CombatWorldSubsystem>(GetTransientPackage());
	FPushboxFakeSource SourceA, SourceB;

	// Before: B blocks A's sweep
	FHitResult Hit;
	Rig.A->GetRootComponent()->SetWorldLocation(FVector(0.0, 0.0, 1000.0));
	Rig.A->GetRootComponent()->SetWorldLocation(FVector(400.0, 0.0, 1000.0), true, &Hit);
	TestTrue(TEXT("unregistered: B blocks A"), Hit.bBlockingHit && Hit.GetActor() == Rig.B);
	Rig.A->GetRootComponent()->SetWorldLocation(FVector(0.0, 0.0, 1000.0));

	// Owners are the fighters' components, as the combat component registers itself
	TestTrue(TEXT("A registers"), Subsystem->RegisterEntity(1, SourceA, Rig.A->GetRootComponent(), FGenericTeamId(1), nullptr));
	TestTrue(TEXT("B registers"), Subsystem->RegisterEntity(2, SourceB, Rig.B->GetRootComponent(), FGenericTeamId(2), nullptr));
	TestTrue(TEXT("A ignores B"), FPushboxRig::Ignores(Rig.A, Rig.B));
	TestTrue(TEXT("B ignores A"), FPushboxRig::Ignores(Rig.B, Rig.A));

	Hit = FHitResult();
	Rig.A->GetRootComponent()->SetWorldLocation(FVector(400.0, 0.0, 1000.0), true, &Hit);
	TestFalse(TEXT("registered: A's capsule sweeps through B"), Hit.bBlockingHit);
	TestTrue(TEXT("A reached the end"), Rig.A->GetActorLocation().Equals(FVector(400.0, 0.0, 1000.0), 0.1));

	// A grab: the link presentation adds its own ignore and removes it at the end; the registration's stays
	Subsystem->AddMoveIgnore(Rig.A, Rig.B);
	TestEqual(TEXT("two references"), Subsystem->GetMoveIgnoreCount(Rig.A, Rig.B), 2);
	Subsystem->RemoveMoveIgnore(Rig.A, Rig.B);
	TestTrue(TEXT("grab end keeps the ignore"), FPushboxRig::Ignores(Rig.A, Rig.B));

	Subsystem->UnregisterEntity(2);
	TestFalse(TEXT("unregistered B: A blocks again"), FPushboxRig::Ignores(Rig.A, Rig.B));
	TestFalse(TEXT("unregistered B: B blocks again"), FPushboxRig::Ignores(Rig.B, Rig.A));
	TestEqual(TEXT("no references left"), Subsystem->GetMoveIgnoreCount(Rig.A, Rig.B), 0);
	Subsystem->UnregisterEntity(1);
	return true;
}

#endif
