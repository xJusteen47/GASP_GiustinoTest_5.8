// Automation tests of the Mover adapter of the depth marker (GSTNDepthMarker.Mover.*). Headless: a throwaway game world, a Mover component
// that is never registered (no backend, no simulation), the smoothing written by hand the way UMoverComponent::FinalizeSmoothingFrame does.

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GSTN_DepthSubject.h"
#include "GSTN_DepthSubjectAdapter_Mover.h"
#include "Misc/AutomationTest.h"
#include "MoverComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNDepthMarkerMoverTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	struct FRig
	{
		UWorld* World = nullptr;
		AActor* Actor = nullptr;
		UCapsuleComponent* Capsule = nullptr;
		UStaticMeshComponent* Visual = nullptr;
		UMoverComponent* Mover = nullptr;

		FRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNDepthMarkerMoverTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());

			Actor = World->SpawnActor<AActor>(FVector(0, 0, 90), FRotator::ZeroRotator);
			Capsule = NewObject<UCapsuleComponent>(Actor, TEXT("Capsule"));
			Capsule->InitCapsuleSize(34.f, 90.f);
			Actor->SetRootComponent(Capsule);
			Capsule->RegisterComponent();
			Capsule->SetWorldLocation(FVector(0, 0, 90));
			// GASP layout: the mesh root at the capsule bottom
			Visual = NewObject<UStaticMeshComponent>(Actor, TEXT("Mesh"));
			Visual->SetupAttachment(Capsule);
			Visual->SetRelativeLocation(FVector(0, 0, -90));
			Visual->RegisterComponent();
			Mover = NewObject<UMoverComponent>(Actor, TEXT("Mover"));
			Mover->SetUpdatedComponent(Capsule);
			Mover->SetPrimaryVisualComponent(Visual);
		}

		~FRig()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		/** What FinalizeSmoothingFrame writes for a smoothed root location */
		void PresentAt(const FVector& SmoothedRoot) const
		{
			Visual->SetWorldTransform(Mover->GetBaseVisualComponentTransform() * FTransform(Capsule->GetComponentQuat(), SmoothedRoot));
		}
	};
}

#define GSTN_DMM_TEST(Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNDepthMarkerMoverTest_##Name, "GSTNDepthMarker.Mover." #Name, GSTNDepthMarkerMoverTest::TestFlags) \
	bool FGSTNDepthMarkerMoverTest_##Name::RunTest(const FString& Parameters)

using namespace GSTNDepthMarkerMoverTest;

GSTN_DMM_TEST(PresentedRootMath)
{
	const FTransform Base(FQuat(FVector::UpVector, FMath::DegreesToRadians(-90.0)), FVector(0, 0, -90));
	const FTransform Root(FQuat(FVector::UpVector, FMath::DegreesToRadians(30.0)), FVector(120, -40, 300));
	const FTransform Visual = Base * Root;
	const FTransform Back = UGSTN_DepthSubjectAdapter_Mover::PresentedRootFromVisual(Visual, Base);
	TestTrue(TEXT("location comes back"), Back.GetLocation().Equals(Root.GetLocation(), 0.01));
	TestTrue(TEXT("rotation comes back"), Back.GetRotation().Equals(Root.GetRotation(), 1e-4));
	return true;
}

GSTN_DMM_TEST(ResolvesForMoverPawn)
{
	FRig Rig;
	const UGSTN_DepthSubjectAdapter* Adapter = FGSTN_DepthAdapterRegistry::Resolve(Rig.Actor);
	TestTrue(TEXT("a Mover pawn gets the Mover adapter (beats the Actor adapter)"), Adapter && Adapter->IsA<UGSTN_DepthSubjectAdapter_Mover>());
	AActor* Plain = Rig.World->SpawnActor<AActor>();
	const UGSTN_DepthSubjectAdapter* ForPlain = FGSTN_DepthAdapterRegistry::Resolve(Plain);
	TestFalse(TEXT("a plain actor does not"), ForPlain && ForPlain->IsA<UGSTN_DepthSubjectAdapter_Mover>());
	return true;
}

GSTN_DMM_TEST(FeetFollowSmoothedMesh)
{
	FRig Rig;
	const UGSTN_DepthSubjectAdapter_Mover* Adapter = GetDefault<UGSTN_DepthSubjectAdapter_Mover>();

	// Run at 600 cm/s, rendered at 144 fps, simulated at 62.5 Hz: the root moves in steps, the mesh is smoothed between the last two ticks
	const double Speed = 600.0;
	const double Frame = 1.0 / 144.0;
	const double Tick = 1.0 / 62.5;
	double MaxError = 0.0;
	double MaxRootLead = 0.0;
	for (int32 Index = 0; Index < 60; ++Index)
	{
		const double Time = Index * Frame;
		const int32 Ticks = FMath::FloorToInt32(Time / Tick);
		const double Alpha = (Time - Ticks * Tick) / Tick;
		const FVector Simulated(Speed * Ticks * Tick, 0, 90);
		const FVector Previous(Speed * FMath::Max(Ticks - 1, 0) * Tick, 0, 90);
		const FVector Smoothed = FMath::Lerp(Previous, Simulated, Alpha);
		Rig.Capsule->SetWorldLocation(Simulated);
		Rig.PresentAt(Smoothed);

		FGSTN_DepthSubjectState State;
		if (!TestTrue(TEXT("reads"), Adapter->ReadState(Rig.Actor, State)))
		{
			return false;
		}
		MaxError = FMath::Max(MaxError, FVector::Dist(State.Feet, Smoothed - FVector(0, 0, 90)));
		MaxRootLead = FMath::Max(MaxRootLead, FMath::Abs(Simulated.X - Smoothed.X));
	}
	TestTrue(TEXT("sanity: the simulated root is visibly apart from the mesh"), MaxRootLead > 5.0);
	TestTrue(TEXT("the feet are under the presented mesh"), MaxError < 0.01);

	// The per-move capsule moves the base and the mesh together (FGSTN_BodyCapsuleModifier): the feet do not jump
	FTransform Base = Rig.Mover->GetBaseVisualComponentTransform();
	Base.SetLocation(FVector(0, 0, -60));
	Rig.Mover->SetBaseVisualComponentTransform(Base);
	Rig.Capsule->SetWorldLocation(FVector(500, 0, 90));
	Rig.PresentAt(FVector(490, 0, 90));
	FGSTN_DepthSubjectState Shifted;
	Adapter->ReadState(Rig.Actor, Shifted);
	TestTrue(TEXT("a base change at run time is not read as smoothing"), Shifted.Feet.Equals(FVector(490, 0, 0), 0.01));
	TestTrue(TEXT("up from Mover"), Shifted.Up.Z > 0.99);
	TestEqual(TEXT("foot radius from the capsule"), Shifted.FootRadius, 34.f);

	// Opting out, or no visual offset smoothing: the simulated root
	UGSTN_DepthSubjectAdapter_Mover* Raw = NewObject<UGSTN_DepthSubjectAdapter_Mover>(GetTransientPackage());
	Raw->bFollowPresentedMesh = false;
	FGSTN_DepthSubjectState RawState;
	Raw->ReadState(Rig.Actor, RawState);
	TestTrue(TEXT("opted out: the root"), RawState.Feet.Equals(FVector(500, 0, 0), 0.01));
	Rig.Mover->SmoothingMode = EMoverSmoothingMode::None;
	Adapter->ReadState(Rig.Actor, RawState);
	TestTrue(TEXT("no visual smoothing: the root"), RawState.Feet.Equals(FVector(500, 0, 0), 0.01));
	return true;
}

#endif
