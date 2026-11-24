// Automation tests of the G5 pawn rig (GSTN.PawnRig.*). Headless: a throwaway game world, a bare APawn, no render, no net driver.

#include "Camera/CameraComponent.h"
#include "Camera/GSTN_CameraAnchor.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GSTN_CameraComponent.h"
#include "GSTN_CameraRuntimeTypes.h"
#include "GSTN_CameraTags.h"
#include "GSTN_InteractorComponent.h"
#include "Misc/AutomationTest.h"
#include "Pawn/GSTN_PawnRigComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNPawnRigTest
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float PawnDt = 1.f / 60.f;

	struct FRig
	{
		UWorld* World = nullptr;

		FRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNPawnRigTestWorld"));
			World->AddToRoot();
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}

		~FRig()
		{
			GEngine->DestroyWorldContext(World);
			World->DestroyWorld(false);
			World->RemoveFromRoot();
		}

		/** A pawn with a root, that has begun play (a throwaway world has no game mode to do it), so components added later begin play too */
		APawn* SpawnPawn(const FVector& Location)
		{
			APawn* Pawn = World->SpawnActor<APawn>();
			USceneComponent* Root = NewObject<USceneComponent>(Pawn, TEXT("Root"));
			Root->SetMobility(EComponentMobility::Movable);
			Pawn->SetRootComponent(Root);
			Root->RegisterComponent();
			Pawn->SetActorLocation(Location);
			if (!Pawn->HasActorBegunPlay())
			{
				Pawn->DispatchBeginPlay();
			}
			return Pawn;
		}

		UGSTN_PawnRigComponent* AddRig(APawn* Pawn)
		{
			UGSTN_PawnRigComponent* Rig = NewObject<UGSTN_PawnRigComponent>(Pawn);
			Rig->RegisterComponent();
			return Rig;
		}
	};

	FGSTN_CameraSubjectState MakeSubject(const FVector& Pivot)
	{
		FGSTN_CameraSubjectState Subject;
		Subject.bValid = true;
		Subject.Pivot = Pivot;
		return Subject;
	}
}

#define GSTN_RIG_TEST(Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNPawnRigTest_##Name, "GSTN.PawnRig." #Name, GSTNPawnRigTest::TestFlags) \
	bool FGSTNPawnRigTest_##Name::RunTest(const FString& Parameters)

using namespace GSTNPawnRigTest;

GSTN_RIG_TEST(PressTrackerTapsAndHoldsOnce)
{
	FGSTN_InteractPressTracker Tracker;
	TestFalse(TEXT("a release with no press is nothing"), Tracker.Release());

	Tracker.Press();
	TestFalse(TEXT("the hold does not fire early"), Tracker.TickHold(0.5f));
	TestTrue(TEXT("a release before the hold completes is a tap"), Tracker.Release());

	Tracker.Press();
	TestFalse(TEXT("not yet"), Tracker.TickHold(0.99f));
	TestTrue(TEXT("the hold fires when the progress reaches 1"), Tracker.TickHold(1.f));
	TestFalse(TEXT("and only once"), Tracker.TickHold(1.f));
	TestFalse(TEXT("the release after a hold is not also a tap"), Tracker.Release());

	TestFalse(TEXT("progress with no press never fires"), Tracker.TickHold(1.f));
	return true;
}

GSTN_RIG_TEST(SubjectIsRawStateWithThePresentedPivot)
{
	FGSTN_CameraSubjectState Raw = MakeSubject(FVector(10, 20, 100));
	Raw.Up = FVector(0, 1, 0);
	Raw.Velocity = FVector(300, 0, 0);
	Raw.Facing = FVector(0, 0, 1);
	Raw.bGrounded = false;

	const FGSTN_CameraSubjectState Out = UGSTN_PawnRigComponent::BuildPresentedSubject(Raw, true, FVector(11, 21, 90), false);
	TestTrue(TEXT("valid"), Out.bValid);
	TestTrue(TEXT("the pivot is the presented one, not the actor root"), Out.Pivot.Equals(FVector(11, 21, 90)));
	TestTrue(TEXT("Up, velocity and facing come from the raw state"), Out.Up.Equals(Raw.Up) && Out.Velocity.Equals(Raw.Velocity) && Out.Facing.Equals(Raw.Facing));
	TestFalse(TEXT("grounded too"), Out.bGrounded);
	TestFalse(TEXT("not suppressed"), Out.bSuppressed);

	TestTrue(TEXT("without a presented pivot the raw one stays"), UGSTN_PawnRigComponent::BuildPresentedSubject(Raw, false, FVector::ZeroVector, false).Pivot.Equals(Raw.Pivot));
	TestTrue(TEXT("a KO suppresses it"), UGSTN_PawnRigComponent::BuildPresentedSubject(Raw, true, FVector::ZeroVector, true).bSuppressed);
	Raw.bSuppressed = true;
	TestTrue(TEXT("and a raw suppressed state stays suppressed"), UGSTN_PawnRigComponent::BuildPresentedSubject(Raw, true, FVector::ZeroVector, false).bSuppressed);
	return true;
}

GSTN_RIG_TEST(RigRunsOnlyWhereAScreenExists)
{
	TestFalse(TEXT("never on a dedicated server"), UGSTN_PawnRigComponent::ShouldRigRunFor(NM_DedicatedServer, true));
	TestTrue(TEXT("on a listen server (the host renders)"), UGSTN_PawnRigComponent::ShouldRigRunFor(NM_ListenServer, true));
	TestTrue(TEXT("on a client"), UGSTN_PawnRigComponent::ShouldRigRunFor(NM_Client, true));
	TestTrue(TEXT("standalone"), UGSTN_PawnRigComponent::ShouldRigRunFor(NM_Standalone, true));
	TestFalse(TEXT("not in an editor preview world"), UGSTN_PawnRigComponent::ShouldRigRunFor(NM_Standalone, false));
	TestFalse(TEXT("the component is not replicated"), GetDefault<UGSTN_PawnRigComponent>()->GetIsReplicated());
	return true;
}

GSTN_RIG_TEST(RigWiresInteractorAndCameraOnAPawn)
{
	FRig Rig;
	APawn* Pawn = Rig.SpawnPawn(FVector(0, 0, 100));
	UCameraComponent* Other = NewObject<UCameraComponent>(Pawn, TEXT("OtherCamera"));
	Other->SetupAttachment(Pawn->GetRootComponent());
	Other->RegisterComponent();
	Other->Activate(true);

	UGSTN_PawnRigComponent* Component = Rig.AddRig(Pawn);
	if (!TestNotNull(TEXT("the rig added an interactor"), Component->GetInteractor()) || !TestNotNull(TEXT("and a GSTN camera"), Component->GetCamera()))
	{
		return false;
	}
	auto CountInteractors = [Pawn]()
	{
		TArray<UGSTN_InteractorComponent*> Found;
		Pawn->GetComponents(Found);
		return Found.Num();
	};
	TestEqual(TEXT("the pawn has exactly one interactor"), CountInteractors(), 1);
	TestTrue(TEXT("the GSTN camera is the active one"), Component->GetCamera()->IsActive());
	TestFalse(TEXT("the pawn's other camera was switched off"), Other->IsActive());
	TestTrue(TEXT("the rig ticks (focus and holds)"), Component->IsComponentTickEnabled());

	// A second rig on the same pawn finds what exists and adds nothing
	UGSTN_PawnRigComponent* Again = Rig.AddRig(Pawn);
	TestTrue(TEXT("the same interactor"), Again->GetInteractor() == Component->GetInteractor());
	TestTrue(TEXT("the same camera"), Again->GetCamera() == Component->GetCamera());
	TestEqual(TEXT("still one interactor"), CountInteractors(), 1);

	// A press with no focus is harmless and leaves nothing held
	Component->InteractPressed();
	Component->InteractReleased();
	return true;
}

GSTN_RIG_TEST(CameraReadsTheSubjectThroughTheRigComponent)
{
	FRig Rig;
	APawn* Pawn = Rig.SpawnPawn(FVector(0, 0, 100));
	UGSTN_PawnRigComponent* Component = Rig.AddRig(Pawn);
	if (!TestNotNull(TEXT("camera"), Component->GetCamera()))
	{
		return false;
	}

	// The adapter registry finds the interface on the component, not only on the actor (camera.md §8)
	FGSTN_CameraSubjectState State;
	TestTrue(TEXT("the subject reads"), FGSTN_CameraAdapterRegistry::Read(Pawn, State));
	TestFalse(TEXT("not suppressed"), State.bSuppressed);
	Component->SetSubjectSuppressed(true);
	TestTrue(TEXT("read again"), FGSTN_CameraAdapterRegistry::Read(Pawn, State));
	TestTrue(TEXT("only the rig can say suppressed: the read went through it"), State.bSuppressed);
	Component->SetSubjectSuppressed(false);

	// The aim stage: the rig is the tag source, so the rule table takes Shoulder, and leaves it on release
	UGSTN_CameraComponent* Camera = Component->GetCamera();
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Camera->EvaluateCamera(PawnDt);
	}
	TestFalse(TEXT("no aim, no Shoulder"), Camera->GetActiveModeTag().MatchesTagExact(GSTNCameraTags::Mode_Shoulder.GetTag()));
	Component->SetAimRequested(true);
	for (int32 Frame = 0; Frame < 90; ++Frame)
	{
		Camera->EvaluateCamera(PawnDt);
	}
	TestTrue(TEXT("the aim request reaches the rule table through the component"), Camera->GetActiveModeTag().MatchesTagExact(GSTNCameraTags::Mode_Shoulder.GetTag()));
	Component->SetAimRequested(false);
	for (int32 Frame = 0; Frame < 180; ++Frame)
	{
		Camera->EvaluateCamera(PawnDt);
	}
	TestFalse(TEXT("release leaves Shoulder"), Camera->GetActiveModeTag().MatchesTagExact(GSTNCameraTags::Mode_Shoulder.GetTag()));
	return true;
}

GSTN_RIG_TEST(RollbackSnapOfTheRootNeverReachesTheView)
{
	// C1 through the real anchor damper: the actor root snaps 40 cm up for one frame (a corrected step that is resimulated), the anchor presents
	// the eased value, the rig hands the camera that. The view's largest one-frame step stays a fraction of the snap; the raw root would not.
	FRig Rig;
	APawn* Pawn = Rig.SpawnPawn(FVector(0, 0, 100));
	UGSTN_CameraComponent* Camera = NewObject<UGSTN_CameraComponent>(Pawn);
	Camera->SetupAttachment(Pawn->GetRootComponent());
	Camera->RegisterComponent();

	FGSTN_CameraAnchorSettings Settings;
	Settings.UpHalfLife = 0.07f;
	Settings.DownHalfLife = 0.07f;
	Settings.bHoldGroundHeightInAir = false;
	Settings.MaxVerticalLag = 0.f;

	auto MaxViewStep = [&](bool bPresented)
	{
		FGSTN_CameraAnchorDamper Damper;
		Damper.Reset(FVector(0, 0, 100));
		FVector Last = Camera->GetBlendedView().Pivot;
		float MaxStep = 0.f;
		for (int32 Frame = 0; Frame < 60; ++Frame)
		{
			const FVector RawRoot(0, 0, Frame == 20 ? 140.0 : 100.0);
			const FVector Presented = Damper.Step(RawRoot, FVector::UpVector, PawnDt, Settings, false);
			const FGSTN_CameraSubjectState Raw = MakeSubject(RawRoot);
			Camera->SetSubjectOverride(UGSTN_PawnRigComponent::BuildPresentedSubject(Raw, bPresented, Presented, false));
			Camera->EvaluateCamera(PawnDt);
			const FVector Now = Camera->GetBlendedView().Pivot;
			if (Frame > 0)
			{
				MaxStep = FMath::Max(MaxStep, static_cast<float>(FVector::Dist(Now, Last)));
			}
			Last = Now;
		}
		return MaxStep;
	};

	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Camera->SetSubjectOverride(MakeSubject(FVector(0, 0, 100)));
		Camera->EvaluateCamera(PawnDt);
	}
	const float PresentedStep = MaxViewStep(true);
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Camera->SetSubjectOverride(MakeSubject(FVector(0, 0, 100)));
		Camera->EvaluateCamera(PawnDt);
	}
	const float RawStep = MaxViewStep(false);
	TestTrue(FString::Printf(TEXT("presented: the view moves %.2f cm at most in a frame"), PresentedStep), PresentedStep < 5.f);
	TestTrue(FString::Printf(TEXT("the raw root moves it more (%.2f cm)"), RawStep), RawStep > PresentedStep);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
