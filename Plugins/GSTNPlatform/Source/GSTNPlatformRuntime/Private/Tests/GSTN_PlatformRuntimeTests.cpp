// Automation tests of the platform runtime skeleton (many-enemies-replication.md 4.3-4.5; plan T3 skeleton).

#include "GSTN_PlatformTestRig.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "GSTN_PlatformColliders.h"
#include "GSTN_PlatformSettings.h"
#include "Modules/ModuleManager.h"
#include "MoverTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

using namespace GSTNPlatformTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformSmokeTest, "GSTNPlatform.Runtime.Smoke", Flags)
bool FGSTN_PlatformSmokeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("GSTNPlatformCore is loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("GSTNPlatformCore")));
	TestTrue(TEXT("GSTNPlatformRuntime is loaded"), FModuleManager::Get().IsModuleLoaded(TEXT("GSTNPlatformRuntime")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformChannelTest, "GSTNPlatform.Runtime.ChannelResolvedByName", Flags)
bool FGSTN_PlatformChannelTest::RunTest(const FString& Parameters)
{
	const UGSTN_PlatformSettings& Settings = UGSTN_PlatformSettings::Get();
	// The plugin's Config/DefaultEngine.ini declares the channel; the engine merges a plugin's DefaultEngine.ini into the Engine branch
	TestTrue(TEXT("GSTN_EnemyPlatform is a collision channel of the running project"), Settings.HasPlatformObjectChannel());
	if (Settings.HasPlatformObjectChannel())
	{
		TestNotEqual(TEXT("It is not the WorldDynamic fallback"), int32(Settings.GetPlatformObjectChannel()), int32(ECC_WorldDynamic));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformCollidersTest, "GSTNPlatform.Runtime.CollidersUnattachedNamedAddressable", Flags)
bool FGSTN_PlatformCollidersTest::RunTest(const FString& Parameters)
{
	FRig Rig;
	FGSTN_PlatformSpec Spec;
	Spec.Shapes = {
		MakeShape(0, EGSTN_PlatformShapeType::Box, FVector(40.0, 30.0, 10.0)),
		MakeShape(2, EGSTN_PlatformShapeType::Sphere, FVector(25.0)),
		MakeShape(5, EGSTN_PlatformShapeType::Capsule, FVector(20.0, 0.0, 60.0)) };
	UGSTN_EnemyPlatformComponent* Platform = Rig.SpawnPlatform(Spec);
	AActor* Owner = Platform->GetOwner();

	UBoxComponent* Box = Cast<UBoxComponent>(Platform->GetShapeComponent(0));
	USphereComponent* Sphere = Cast<USphereComponent>(Platform->GetShapeComponent(2));
	UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Platform->GetShapeComponent(5));
	if (!TestNotNull(TEXT("Box collider"), Box) || !TestNotNull(TEXT("Sphere collider"), Sphere) || !TestNotNull(TEXT("Capsule collider"), Capsule))
	{
		return false;
	}
	TestNull(TEXT("No collider for a missing id"), Platform->GetShapeComponent(1));
	TestTrue(TEXT("Box extent"), Box->GetUnscaledBoxExtent().Equals(FVector(40.0, 30.0, 10.0)));
	TestEqual(TEXT("Sphere radius"), Sphere->GetUnscaledSphereRadius(), 25.f);
	TestEqual(TEXT("Capsule half height"), Capsule->GetUnscaledCapsuleHalfHeight(), 60.f);

	for (UPrimitiveComponent* Collider : { (UPrimitiveComponent*)Box, (UPrimitiveComponent*)Sphere, (UPrimitiveComponent*)Capsule })
	{
		UGSTN_EnemyPlatformComponent* Found = nullptr;
		int32 FoundId = INDEX_NONE;
		TestTrue(*FString::Printf(TEXT("%s is a platform collider"), *Collider->GetName()), IGSTN_PlatformCollider::Find(Collider, Found, FoundId));
		TestTrue(TEXT("Its platform"), Found == Platform);
		TestEqual(TEXT("Its name is Surface<Id>"), Collider->GetFName(), GSTNPlatform::GetShapeComponentName(FoundId));
		TestNull(TEXT("Unattached"), Collider->GetAttachParent());
		TestTrue(TEXT("Movable"), Collider->Mobility == EComponentMobility::Movable);
		TestFalse(TEXT("Kinematic (no physics simulation)"), Collider->IsSimulatingPhysics());
		TestTrue(TEXT("Owned by the enemy"), Collider->GetOwner() == Owner);
		TestTrue(TEXT("Net addressable: a client resolves a base reference to its own collider"), Collider->IsNameStableForNetworking());
		TestEqual(TEXT("Object type is the platform channel"), int32(Collider->GetCollisionObjectType()), int32(UGSTN_PlatformSettings::Get().GetPlatformObjectChannel()));
	}

	// Unregistering frees the names, so a second registration creates them again
	Platform->UnregisterComponent();
	TestNull(TEXT("Colliders gone"), Platform->GetShapeComponent(0));
	Platform->RegisterComponent();
	TestNotNull(TEXT("Colliders back"), Platform->GetShapeComponent(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_PlatformPosePointTest, "GSTNPlatform.Runtime.PosePointFollowsOwner", Flags)
bool FGSTN_PlatformPosePointTest::RunTest(const FString& Parameters)
{
	FRig Rig;
	FGSTN_PlatformSpec Spec;
	Spec.Shapes = { MakeShape(0, EGSTN_PlatformShapeType::Box, FVector(50.0, 50.0, 10.0), FVector(0.0, 0.0, 100.0)) };
	UGSTN_EnemyPlatformComponent* Platform = Rig.SpawnPlatform(Spec, FVector(500.0, 0.0, 0.0));
	UPrimitiveComponent* Top = Platform->GetShapeComponent(0);
	AActor* Owner = Platform->GetOwner();
	TestTrue(TEXT("Registered colliders start at the owner's pose"), Top->GetComponentLocation().Equals(FVector(500.0, 0.0, 100.0)));

	// The owner moves in its own tick: the collider does not follow until the pose point
	Owner->SetActorLocationAndRotation(FVector(600.0, 0.0, 0.0), FRotator(0.0, 90.0, 0.0));
	TestTrue(TEXT("Not attached: unchanged before the pose point"), Top->GetComponentLocation().Equals(FVector(500.0, 0.0, 100.0)));
	TestTrue(TEXT("Pose point applied"), Rig.Subsystem()->ApplyPosePoint(MakeContext(10)));
	TestTrue(TEXT("Posed at the owner"), Top->GetComponentLocation().Equals(FVector(600.0, 0.0, 100.0)));
	TestTrue(TEXT("With the owner's yaw"), FMath::IsNearlyEqual(Top->GetComponentRotation().Yaw, 90.0, 1.e-3));
	TestEqual(TEXT("Frame of the pose"), Platform->GetCurrentPoseFrame(), 10);

	FGSTN_PoseSample Sample;
	TestTrue(TEXT("Recorded in the history"), Platform->GetPoseHistory().Sample(10, Sample) && Sample.Location.Equals(FVector(600.0, 0.0, 0.0)));

	// Mover's time step of frame 11 poses the end of frame 10
	FMoverTimeStep Step;
	Step.ServerFrame = 11;
	Step.BaseSimTimeMs = 176.0;
	Step.StepMs = 16.f;
	const FGSTN_PlatformPoseContext FromMover = UGSTN_PlatformSubsystem::MakePoseContext(Step);
	TestEqual(TEXT("Frame = ServerFrame - 1"), FromMover.Frame, 10);
	TestEqual(TEXT("Time = start of the frame"), FromMover.TimeMs, 176.0);
	return true;
}

#endif
