// Automation tests of the level objects (M4 H11b).

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GSTN_SurfaceTags.h"
#include "GSTN_SurfaceUtils.h"
#include "GSTN_Trampoline.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNLevelObjectsTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A throwaway game world to own the test actors */
	struct FRig
	{
		UWorld* World = nullptr;

		FRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNLevelObjectsTestWorld"));
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

		AGSTN_Trampoline* Spawn(const FTransform& Transform, const FVector& Size, const FGSTN_SurfaceLaunchParams* Launch = nullptr)
		{
			AGSTN_Trampoline* Actor = World->SpawnActorDeferred<AGSTN_Trampoline>(AGSTN_Trampoline::StaticClass(), Transform);
			Actor->Size = Size;
			if (Launch)
			{
				Actor->Launch = *Launch;
			}
			Actor->FinishSpawning(Transform);
			return Actor;
		}
	};

	FGSTN_SurfaceQuery QueryWithNormal(const FVector& Normal)
	{
		FGSTN_SurfaceQuery Query;
		Query.ContactNormal = Normal;
		return Query;
	}

	FGSTN_SurfaceBehaviour Ask(AGSTN_Trampoline* Actor, const FVector& Normal)
	{
		return IGSTN_SurfaceProvider::Execute_GetSurfaceBehaviour(Actor, QueryWithNormal(Normal));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TrampolineSizeTest, "GSTNLevelObjects.SizeDrivesBoxExtent", GSTNLevelObjectsTest::Flags)
bool FGSTN_TrampolineSizeTest::RunTest(const FString& Parameters)
{
	GSTNLevelObjectsTest::FRig Rig;
	AGSTN_Trampoline* Actor = Rig.Spawn(FTransform::Identity, FVector(300.0, 100.0, 50.0));
	TestTrue(TEXT("The box extent is half the size"), Actor->GetBox()->GetUnscaledBoxExtent().Equals(FVector(150.0, 50.0, 25.0)));
	TestTrue(TEXT("The box is the root"), Actor->GetRootComponent() == Actor->GetBox());

	// The ClampMin of the property is enforced in code too: a script or a Blueprint default cannot make a flat box
	Actor->Size = FVector(2.0, 100.0, -5.0);
	Actor->RerunConstructionScripts();
	TestTrue(TEXT("Axes below 10 cm clamp to 10"), Actor->GetBox()->GetUnscaledBoxExtent().Equals(FVector(5.0, 50.0, 5.0)));
	TestTrue(TEXT("The pure helper agrees"), AGSTN_Trampoline::SizeToBoxExtent(FVector(0.0, 40.0, 11.0)).Equals(FVector(5.0, 20.0, 5.5)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TrampolineMeshFitTest, "GSTNLevelObjects.MeshFitStretch", GSTNLevelObjectsTest::Flags)
bool FGSTN_TrampolineMeshFitTest::RunTest(const FString& Parameters)
{
	const FVector Size(400.0, 200.0, 40.0);
	TestTrue(TEXT("Stretch divides the box size by the mesh bounds"),
		AGSTN_Trampoline::ComputeMeshScale(EGSTN_TrampolineMeshFit::StretchToBox, Size, FVector(100.0, 100.0, 100.0)).Equals(FVector(4.0, 2.0, 0.4)));
	TestTrue(TEXT("Keep leaves the scale alone"),
		AGSTN_Trampoline::ComputeMeshScale(EGSTN_TrampolineMeshFit::Keep, Size, FVector(100.0, 100.0, 100.0)).Equals(FVector::OneVector));
	TestTrue(TEXT("A flat mesh is not stretched along its empty axis"),
		AGSTN_Trampoline::ComputeMeshScale(EGSTN_TrampolineMeshFit::StretchToBox, Size, FVector(100.0, 100.0, 0.0)).Equals(FVector(4.0, 2.0, 1.0)));

	// On an instance: the default cube (100 cm) fills the box
	GSTNLevelObjectsTest::FRig Rig;
	AGSTN_Trampoline* Actor = Rig.Spawn(FTransform::Identity, Size);
	TestNotNull(TEXT("The instance loaded the default mesh"), Actor->GetMesh()->GetStaticMesh().Get());
	TestTrue(TEXT("The cube is stretched to the box"), Actor->GetMesh()->GetRelativeScale3D().Equals(FVector(4.0, 2.0, 0.4), 1.e-3));

	Actor->MeshFit = EGSTN_TrampolineMeshFit::Keep;
	Actor->RerunConstructionScripts();
	TestTrue(TEXT("Keep leaves the instance scale at 1"), Actor->GetMesh()->GetRelativeScale3D().Equals(FVector::OneVector, 1.e-3));

	Actor->MeshAsset.Reset();
	Actor->RerunConstructionScripts();
	TestNull(TEXT("No mesh asset, no mesh"), Actor->GetMesh()->GetStaticMesh().Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TrampolineDirectionTest, "GSTNLevelObjects.LaunchDirectionModes", GSTNLevelObjectsTest::Flags)
bool FGSTN_TrampolineDirectionTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLevelObjectsTest;
	FRig Rig;
	constexpr double Tolerance = 1.e-4;

	// Flat, local direction: along the actor's up
	AGSTN_Trampoline* Flat = Rig.Spawn(FTransform::Identity, FVector(200.0, 200.0, 40.0));
	const FGSTN_SurfaceBehaviour Top = Ask(Flat, FVector::UpVector);
	TestTrue(TEXT("The top face launches"), Top.HasBehaviour(GSTNSurfaceTags::Behaviour_Launch));
	TestTrue(TEXT("It can be stood on"), Top.HasBehaviour(GSTNSurfaceTags::Behaviour_Stand));
	TestEqual(TEXT("It is a trampoline for the impact table"), Top.SurfaceType.ToString(), FString(TEXT("Surface.Type.Trampoline")));
	TestTrue(TEXT("Local up is world up on a flat trampoline"),
		UGSTN_SurfaceUtils::ComputeLaunchDirection(Top.Launch, Flat->GetActorTransform(), FVector::UpVector).Equals(FVector::UpVector, Tolerance));

	const FGSTN_SurfaceBehaviour Side = Ask(Flat, FVector::ForwardVector);
	TestFalse(TEXT("A side face does not launch"), Side.HasBehaviour(GSTNSurfaceTags::Behaviour_Launch));
	TestEqual(TEXT("A side face is still a trampoline for a tumbling victim"), Side.SurfaceType.ToString(), FString(TEXT("Surface.Type.Trampoline")));
	TestFalse(TEXT("The underside does not launch"), Ask(Flat, FVector::DownVector).HasBehaviour(GSTNSurfaceTags::Behaviour_Launch));
	TestTrue(TEXT("A query without a normal is the whole object"),
		IGSTN_SurfaceProvider::Execute_GetSurfaceBehaviour(Flat, FGSTN_SurfaceQuery()).HasBehaviour(GSTNSurfaceTags::Behaviour_Launch));

	// Mounted on a wall (rolled 90 degrees): the launch follows the actor, away from the wall
	AGSTN_Trampoline* Wall = Rig.Spawn(FTransform(FRotator(0.0, 0.0, 90.0), FVector(500.0, 0.0, 0.0)), FVector(200.0, 200.0, 40.0));
	const FVector WallUp = Wall->GetActorUpVector();
	const FGSTN_SurfaceBehaviour WallTop = Ask(Wall, WallUp);
	TestTrue(TEXT("The wall mounted top face launches"), WallTop.HasBehaviour(GSTNSurfaceTags::Behaviour_Launch));
	TestFalse(TEXT("Its world up face does not"), Ask(Wall, FVector::UpVector).HasBehaviour(GSTNSurfaceTags::Behaviour_Launch));
	TestTrue(TEXT("The launch follows the rotation"),
		UGSTN_SurfaceUtils::ComputeLaunchDirection(WallTop.Launch, Wall->GetActorTransform(), WallUp).Equals(WallUp, Tolerance));
	TestTrue(TEXT("The wall launch is not vertical"), FMath::Abs(WallUp.Z) < 0.01);

	// Tilt mode: leans from the actor's up by the pitch, towards its right at 90 degrees of yaw
	FGSTN_SurfaceLaunchParams Tilt;
	Tilt.DirectionMode = EGSTN_SurfaceLaunchDirection::Tilt;
	Tilt.TiltPitch = 45.f;
	Tilt.TiltYaw = 90.f;
	AGSTN_Trampoline* Tilted = Rig.Spawn(FTransform::Identity, FVector(200.0, 200.0, 40.0), &Tilt);
	const FGSTN_SurfaceBehaviour TiltTop = Ask(Tilted, FVector::UpVector);
	TestTrue(TEXT("Tilt 45 at yaw 90 leans towards local right"),
		UGSTN_SurfaceUtils::ComputeLaunchDirection(TiltTop.Launch, Tilted->GetActorTransform(), FVector::UpVector).Equals(FVector(0.0, 1.0, 1.0).GetSafeNormal(), Tolerance));

	// SurfaceNormal mode: along the touched face, which a trampoline only offers on the top
	FGSTN_SurfaceLaunchParams ByNormalParams;
	ByNormalParams.DirectionMode = EGSTN_SurfaceLaunchDirection::SurfaceNormal;
	AGSTN_Trampoline* ByNormal = Rig.Spawn(FTransform(FRotator(30.0, 0.0, 0.0)), FVector(200.0, 200.0, 40.0), &ByNormalParams);
	const FVector TopNormal = ByNormal->GetActorUpVector();
	const FGSTN_SurfaceBehaviour NormalTop = Ask(ByNormal, TopNormal);
	TestTrue(TEXT("SurfaceNormal follows the contact normal"),
		UGSTN_SurfaceUtils::ComputeLaunchDirection(NormalTop.Launch, ByNormal->GetActorTransform(), TopNormal).Equals(TopNormal, Tolerance));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TrampolineSpeedTest, "GSTNLevelObjects.SpeedModes", GSTNLevelObjectsTest::Flags)
bool FGSTN_TrampolineSpeedTest::RunTest(const FString& Parameters)
{
	using namespace GSTNLevelObjectsTest;
	FRig Rig;

	FGSTN_SurfaceLaunchParams Fixed;
	Fixed.SpeedMode = EGSTN_SurfaceLaunchSpeed::Fixed;
	Fixed.Speed = 1800.f;
	const FGSTN_SurfaceBehaviour FixedView = Ask(Rig.Spawn(FTransform::Identity, FVector(200.0, 200.0, 40.0), &Fixed), FVector::UpVector);
	TestEqual(TEXT("Fixed ignores a soft arrival"), UGSTN_SurfaceUtils::ComputeLaunchSpeed(FixedView.Launch, 100.f), 1800.f);
	TestEqual(TEXT("Fixed ignores a hard arrival"), UGSTN_SurfaceUtils::ComputeLaunchSpeed(FixedView.Launch, 9000.f), 1800.f);

	FGSTN_SurfaceLaunchParams Scaled;
	Scaled.SpeedMode = EGSTN_SurfaceLaunchSpeed::ScaledByIncoming;
	Scaled.Restitution = 0.8f;
	Scaled.MinSpeed = 900.f;
	Scaled.MaxSpeed = 2000.f;
	const FGSTN_SurfaceBehaviour ScaledView = Ask(Rig.Spawn(FTransform::Identity, FVector(200.0, 200.0, 40.0), &Scaled), FVector::UpVector);
	TestEqual(TEXT("Scaled follows the arrival speed"), UGSTN_SurfaceUtils::ComputeLaunchSpeed(ScaledView.Launch, 2000.f), 1600.f);
	TestEqual(TEXT("A soft landing still launches at the minimum"), UGSTN_SurfaceUtils::ComputeLaunchSpeed(ScaledView.Launch, 50.f), 900.f);
	TestEqual(TEXT("A hard landing is capped"), UGSTN_SurfaceUtils::ComputeLaunchSpeed(ScaledView.Launch, 10000.f), 2000.f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTN_TrampolineCdoTest, "GSTNLevelObjects.SoftMeshNotLoadedInCDO", GSTNLevelObjectsTest::Flags)
bool FGSTN_TrampolineCdoTest::RunTest(const FString& Parameters)
{
	const AGSTN_Trampoline* Cdo = GetDefault<AGSTN_Trampoline>();
	TestFalse(TEXT("The default mesh is named"), Cdo->MeshAsset.IsNull());
	TestNull(TEXT("The CDO's mesh component holds no loaded mesh"), Cdo->GetMesh()->GetStaticMesh().Get());
	TestTrue(TEXT("The default is the engine cube"), Cdo->MeshAsset.ToSoftObjectPath().ToString().Contains(TEXT("BasicShapes/Cube")));
	TestTrue(TEXT("Static by default (the rollback rule)"), Cdo->GetRootComponent()->Mobility == EComponentMobility::Static);

	// An instance loads its own mesh and leaves the CDO's component alone
	GSTNLevelObjectsTest::FRig Rig;
	Rig.Spawn(FTransform::Identity, FVector(200.0, 200.0, 40.0));
	TestNull(TEXT("An instance does not load the mesh into the CDO"), Cdo->GetMesh()->GetStaticMesh().Get());
	return true;
}

#endif
