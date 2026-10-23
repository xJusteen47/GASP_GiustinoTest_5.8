// Runtime automation tests of the camera (camera.md §11, GSTNCamera.Runtime.*). Everything runs headless in a throwaway game world:
// the camera component is driven by hand (EvaluateCamera) with a subject state the test supplies.

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GSTN_CameraComponent.h"
#include "GSTN_CameraLocalPlayerSubsystem.h"
#include "GSTN_CameraMath.h"
#include "GSTN_CameraMathLibrary.h"
#include "GSTN_CameraMode.h"
#include "GSTN_CameraRuntimeTypes.h"
#include "GSTN_CameraSection.h"
#include "GSTN_CameraSettings.h"
#include "GSTN_CameraSubjectAdapters.h"
#include "GSTN_CameraTags.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GSTNCameraRuntimeTest
{
	constexpr EAutomationTestFlags CameraRuntimeTestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Dt = 1.f / 60.f;

	/** A throwaway game world that has begun play, so spawned actors register their sections */
	struct FRig
	{
		UWorld* World = nullptr;

		FRig()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("GSTNCameraTestWorld"));
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

		AActor* SpawnSubject(const FVector& Location)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"));
			Root->SetMobility(EComponentMobility::Movable);
			Actor->SetRootComponent(Root);
			Root->RegisterComponent();
			Actor->SetActorLocation(Location);
			return Actor;
		}

		UGSTN_CameraComponent* AddCamera(AActor* Actor)
		{
			UGSTN_CameraComponent* Camera = NewObject<UGSTN_CameraComponent>(Actor);
			Camera->SetupAttachment(Actor->GetRootComponent());
			Camera->RegisterComponent();
			return Camera;
		}

		AGSTN_CameraSection* SpawnSection(const FVector& Location, const FVector& Extent, int32 Priority)
		{
			AGSTN_CameraSection* Section = World->SpawnActor<AGSTN_CameraSection>(Location, FRotator::ZeroRotator);
			Section->FallbackExtent = Extent;
			Section->Spec.Priority = Priority;
			if (!Section->HasActorBegunPlay())
			{
				// A throwaway world has no game mode to start play for actors spawned into it
				Section->DispatchBeginPlay();
			}
			return Section;
		}
	};

	FGSTN_CameraSubjectState MakeSubject(const FVector& Pivot, const FVector& Up = FVector::UpVector)
	{
		FGSTN_CameraSubjectState Subject;
		Subject.bValid = true;
		Subject.Pivot = Pivot;
		Subject.Up = Up;
		return Subject;
	}

	void Run(UGSTN_CameraComponent* Camera, float Seconds)
	{
		const int32 Frames = FMath::CeilToInt(Seconds / Dt);
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Camera->EvaluateCamera(Dt);
		}
	}

	FGameplayTagContainer AimTags()
	{
		FGameplayTagContainer Tags;
		Tags.AddTag(GSTNCameraTags::Request_Aim);
		return Tags;
	}
}

#define GSTN_CAM_RT_TEST(Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGSTNCameraRuntimeTest_##Name, "GSTNCamera.Runtime." #Name, GSTNCameraRuntimeTest::CameraRuntimeTestFlags) \
	bool FGSTNCameraRuntimeTest_##Name::RunTest(const FString& Parameters)

using namespace GSTNCameraRuntimeTest;

GSTN_CAM_RT_TEST(StackPushPopBlend)
{
	FRig Rig;
	AActor* Subject = Rig.SpawnSubject(FVector(0, 0, 100));
	UGSTN_CameraComponent* Camera = Rig.AddCamera(Subject);

	Run(Camera, 0.2f);
	TestTrue(TEXT("starts in Follow"), Camera->GetActiveModeTag().MatchesTagExact(GSTNCameraTags::Mode_Follow.GetTag()));
	TestEqual(TEXT("one entry"), Camera->GetStackDepth(), 1);

	Camera->SetRequestTags(AimTags());
	int32 MaxDepth = 0;
	for (int32 Frame = 0; Frame < 60; ++Frame)
	{
		Camera->EvaluateCamera(Dt);
		MaxDepth = FMath::Max(MaxDepth, Camera->GetStackDepth());
	}
	TestTrue(TEXT("aim pushes Shoulder"), Camera->GetActiveModeTag().MatchesTagExact(GSTNCameraTags::Mode_Shoulder.GetTag()));
	TestTrue(TEXT("a blend had two entries"), MaxDepth >= 2);
	TestEqual(TEXT("the blend completed and covered entries dropped"), Camera->GetStackDepth(), 1);
	TestTrue(TEXT("Shoulder is closer than Follow"), Camera->GetBlendedView().ArmCm < 300.f);

	Camera->SetRequestTags(FGameplayTagContainer());
	Run(Camera, 1.5f);
	TestTrue(TEXT("popping returns to Follow"), Camera->GetActiveModeTag().MatchesTagExact(GSTNCameraTags::Mode_Follow.GetTag()));
	TestEqual(TEXT("one entry again"), Camera->GetStackDepth(), 1);
	TestTrue(TEXT("far again"), Camera->GetBlendedView().ArmCm > 500.f);
	return true;
}

GSTN_CAM_RT_TEST(ModeStateIsPerEntry)
{
	FRig Rig;
	AActor* A = Rig.SpawnSubject(FVector(0, 0, 100));
	AActor* B = Rig.SpawnSubject(FVector(5000, 0, 100));
	UGSTN_CameraComponent* CameraA = Rig.AddCamera(A);
	UGSTN_CameraComponent* CameraB = Rig.AddCamera(B);
	Run(CameraA, 0.1f);
	Run(CameraB, 0.1f);

	UGSTN_CameraMode* ModeA = CameraA->GetTopModeInstance();
	UGSTN_CameraMode* ModeB = CameraB->GetTopModeInstance();
	TestTrue(TEXT("both have a mode"), ModeA && ModeB);
	TestTrue(TEXT("a stack entry owns a copy, not the asset"), ModeA != CameraA->GetTopDefinition()->Mode);
	TestTrue(TEXT("two cameras never share an instance"), ModeA != ModeB);

	// The smoothing state of one entry never leaks into the other
	A->SetActorLocation(FVector(0, 4000, 100));
	Run(CameraA, 0.05f);
	TestTrue(TEXT("A is still catching up"), FVector::Dist(CameraA->GetBlendedView().Pivot, A->GetActorLocation()) > 1.0);
	TestTrue(TEXT("B did not move"), FVector::Dist(CameraB->GetBlendedView().Pivot, B->GetActorLocation()) < 1.0);
	return true;
}

GSTN_CAM_RT_TEST(SectionRegistryUnloadRemoves)
{
	FRig Rig;
	UGSTN_CameraSectionSubsystem* Subsystem = UGSTN_CameraSectionSubsystem::Get(Rig.World);
	TestNotNull(TEXT("the world has the section subsystem"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}
	TestEqual(TEXT("empty"), Subsystem->GetNumSections(), 0);
	AGSTN_CameraSection* First = Rig.SpawnSection(FVector::ZeroVector, FVector(100), 0);
	AGSTN_CameraSection* Second = Rig.SpawnSection(FVector(1000, 0, 0), FVector(100), 0);
	TestEqual(TEXT("both register on begin play"), Subsystem->GetNumSections(), 2);
	TArray<AGSTN_CameraSection*> Inside;
	Subsystem->GatherContaining(FVector(10, 0, 0), Inside);
	TestTrue(TEXT("point in volume"), Inside.Num() == 1 && Inside[0] == First);
	First->Destroy();
	TestEqual(TEXT("a section that unloads unregisters"), Subsystem->GetNumSections(), 1);
	Subsystem->GatherContaining(FVector(10, 0, 0), Inside);
	TestEqual(TEXT("and no longer contains"), Inside.Num(), 0);
	Second->Destroy();
	TestEqual(TEXT("all gone"), Subsystem->GetNumSections(), 0);
	return true;
}

GSTN_CAM_RT_TEST(MissingTagSourceFallsBackToFollow)
{
	FRig Rig;
	AActor* Subject = Rig.SpawnSubject(FVector(0, 0, 100));
	TestFalse(TEXT("a plain actor implements no tag source"), Subject->Implements<UGSTN_CameraTagSource>());
	UGSTN_CameraComponent* Camera = Rig.AddCamera(Subject);
	Run(Camera, 1.0f);
	TestTrue(TEXT("empty container yields Follow"), Camera->GetActiveModeTag().MatchesTagExact(GSTNCameraTags::Mode_Follow.GetTag()));
	TestTrue(TEXT("a plain actor is read by the actor adapter"), FGSTN_CameraAdapterRegistry::Find(Subject) != nullptr);
	return true;
}

GSTN_CAM_RT_TEST(DedicatedServerCreatesNothing)
{
	TestFalse(TEXT("section subsystem"), UGSTN_CameraSectionSubsystem::ShouldCreateFor(NM_DedicatedServer, true, true));
	TestFalse(TEXT("local player subsystem"), UGSTN_CameraLocalPlayerSubsystem::ShouldCreateFor(NM_DedicatedServer, true, true));
	TestFalse(TEXT("component"), UGSTN_CameraComponent::ShouldRunFor(NM_DedicatedServer, true, true));
	for (const ENetMode Mode : { NM_Standalone, NM_Client, NM_ListenServer })
	{
		TestTrue(TEXT("sections everywhere else"), UGSTN_CameraSectionSubsystem::ShouldCreateFor(Mode, true, true));
		TestTrue(TEXT("local players everywhere else"), UGSTN_CameraLocalPlayerSubsystem::ShouldCreateFor(Mode, true, true));
		TestTrue(TEXT("component everywhere else, the listen host included"), UGSTN_CameraComponent::ShouldRunFor(Mode, true, true));
	}
	TestFalse(TEXT("not a game world"), UGSTN_CameraSectionSubsystem::ShouldCreateFor(NM_Standalone, false, true));
	TestFalse(TEXT("disabled"), UGSTN_CameraComponent::ShouldRunFor(NM_Standalone, true, false));
	return true;
}

GSTN_CAM_RT_TEST(ComponentNotReplicated)
{
	FRig Rig;
	AActor* Subject = Rig.SpawnSubject(FVector::ZeroVector);
	UGSTN_CameraComponent* Camera = Rig.AddCamera(Subject);
	TestFalse(TEXT("the component never replicates"), Camera->GetIsReplicated());
	TestFalse(TEXT("nor does the section"), Rig.SpawnSection(FVector::ZeroVector, FVector(10), 0)->GetIsReplicated());
	return true;
}

GSTN_CAM_RT_TEST(SubjectLostHoldsThenCuts)
{
	FRig Rig;
	AActor* Subject = Rig.SpawnSubject(FVector(0, 0, 100));
	UGSTN_CameraComponent* Camera = Rig.AddCamera(Subject);
	Camera->SetSubjectOverride(MakeSubject(FVector(0, 0, 100)));
	Run(Camera, 0.5f);
	const FGSTN_CameraPose Before = Camera->GetPose();

	// KO: the subject is suppressed and moves away; the camera keeps its last view
	FGSTN_CameraSubjectState Ko = MakeSubject(FVector(9000, 0, 100));
	Ko.bSuppressed = true;
	Camera->SetSubjectOverride(Ko);
	Run(Camera, 1.0f);
	TestTrue(TEXT("holding"), Camera->IsHoldingView());
	TestTrue(TEXT("the view did not move"), Camera->GetPose().Location.Equals(Before.Location, 1.0e-3));

	// Respawn far away: a cut, not a streak across the level
	Camera->SetSubjectOverride(MakeSubject(FVector(9000, 0, 100)));
	Camera->EvaluateCamera(Dt);
	TestFalse(TEXT("no longer holding"), Camera->IsHoldingView());
	TestTrue(TEXT("the first frame is already at the new pawn"), FVector::Dist(Camera->GetBlendedView().Pivot, FVector(9000, 0, 100)) < 1.0);
	TestEqual(TEXT("no blend left over"), Camera->GetStackDepth(), 1);
	return true;
}

GSTN_CAM_RT_TEST(TwoLocalPlayersIndependent)
{
	FRig Rig;
	AActor* SubjectA = Rig.SpawnSubject(FVector(0, 0, 100));
	AActor* SubjectB = Rig.SpawnSubject(FVector(2000, 0, 100));
	UGSTN_CameraComponent* A = Rig.AddCamera(SubjectA);
	UGSTN_CameraComponent* B = Rig.AddCamera(SubjectB);
	A->SetSubjectOverride(MakeSubject(SubjectA->GetActorLocation(), FVector::UpVector));
	B->SetSubjectOverride(MakeSubject(SubjectB->GetActorLocation(), FVector(1, 0, 0)));
	Run(A, 0.2f);
	Run(B, 0.2f);
	A->AddLookInput(FVector2D(40.0, 10.0));
	A->EvaluateCamera(Dt);
	B->EvaluateCamera(Dt);
	TestTrue(TEXT("A turned"), !FMath::IsNearlyZero(FGSTN_CameraMath::ShortestAngleDelta(A->GetOrbit().YawDeg, 0.f), 1.f));
	TestTrue(TEXT("B did not"), FMath::IsNearlyZero(B->GetOrbit().YawDeg, 1.0e-3f));
	TestTrue(TEXT("frames differ per player"), A->GetFrame().Up.Equals(FVector::UpVector, 1.0e-6) && B->GetFrame().Up.Equals(FVector(1, 0, 0), 1.0e-6));
	TestTrue(TEXT("each camera looks at its own pivot"), FVector::Dist(A->GetBlendedView().Pivot, SubjectA->GetActorLocation()) < 5.0 && FVector::Dist(B->GetBlendedView().Pivot, SubjectB->GetActorLocation()) < 5.0);
	// Under sideways gravity the view up is the gravity up
	TestTrue(TEXT("B pose up follows its gravity"), FVector::DotProduct(B->GetPose().Up, FVector(1, 0, 0)) > 0.5);
	return true;
}

GSTN_CAM_RT_TEST(FadeVersusPullIn)
{
	FRig Rig;
	AActor* Wall = Rig.SpawnSubject(FVector::ZeroVector);
	UBoxComponent* Fadeable = NewObject<UBoxComponent>(Wall, TEXT("Fadeable"));
	Fadeable->ComponentTags.Add(UGSTN_CameraSettings::Get().FadeTag);
	UBoxComponent* Hard = NewObject<UBoxComponent>(Wall, TEXT("Hard"));
	TestTrue(TEXT("tagged component fades"), UGSTN_CameraComponent::IsFadeable(Fadeable));
	TestFalse(TEXT("untagged does not"), UGSTN_CameraComponent::IsFadeable(Hard));
	TestFalse(TEXT("no component"), UGSTN_CameraComponent::IsFadeable(nullptr));

	// A tagged actor makes all its components fadeable
	Wall->Tags.Add(UGSTN_CameraSettings::Get().FadeTag);
	TestTrue(TEXT("actor tag"), UGSTN_CameraComponent::IsFadeable(Hard));

	const FGSTN_CameraOcclusionTuning Far;
	using Action = EGSTN_CameraOcclusionAction;
	TestEqual(TEXT("far camera fades a fadeable wall"), FGSTN_CameraMath::DecideOcclusion(Far.Policy, true, 400.f, Far.NearClipSafeCm), Action::Fade);
	TestEqual(TEXT("and ignores a far hard blocker"), FGSTN_CameraMath::DecideOcclusion(Far.Policy, false, 400.f, Far.NearClipSafeCm), Action::None);
	TestEqual(TEXT("a close hard blocker pulls in"), FGSTN_CameraMath::DecideOcclusion(Far.Policy, false, 10.f, Far.NearClipSafeCm), Action::PullIn);
	TestEqual(TEXT("Shoulder default is plain pull-in"), FGSTN_CameraMath::DecideOcclusion(UGSTN_CameraSettings::Get().ShoulderOcclusion.Policy, true, 400.f, 50.f), Action::PullIn);

	// Without physics the probe finds nothing and the view is untouched
	UGSTN_CameraComponent* Camera = Rig.AddCamera(Rig.SpawnSubject(FVector(0, 0, 100)));
	Run(Camera, 0.2f);
	TestEqual(TEXT("nothing fades without blockers"), Camera->GetNumFadedComponents(), 0);
	return true;
}

GSTN_CAM_RT_TEST(LookConsumedWhenManualOff)
{
	FRig Rig;
	AGSTN_CameraSection* Section = Rig.SpawnSection(FVector::ZeroVector, FVector(2000), 1);
	Section->Spec.bAllowManualControl = false;
	Section->Spec.DesiredYawDeg = 30.f;
	Section->Spec.DesiredPitchDeg = -35.f;
	Section->Spec.bCutOnEnter = true;
	AActor* Subject = Rig.SpawnSubject(FVector(0, 0, 100));
	UGSTN_CameraComponent* Camera = Rig.AddCamera(Subject);
	Camera->SetSubjectOverride(MakeSubject(FVector(0, 0, 100)));
	Run(Camera, 0.3f);

	TestTrue(TEXT("the section is active"), Camera->GetActiveSection() == Section);
	TestTrue(TEXT("and runs the section mode"), Camera->GetActiveModeTag().MatchesTagExact(GSTNCameraTags::Mode_Section.GetTag()));
	TestTrue(TEXT("orbit is at the designer yaw"), FMath::IsNearlyEqual(Camera->GetOrbit().YawDeg, 30.f, 1.0e-2f));
	TestTrue(TEXT("and pitch"), FMath::IsNearlyEqual(Camera->GetOrbit().PitchDeg, -35.f, 1.0e-2f));

	Camera->AddLookInput(FVector2D(500.0, -80.0));
	Run(Camera, 0.1f);
	TestTrue(TEXT("look input is consumed, the view stays"), FMath::IsNearlyEqual(Camera->GetOrbit().YawDeg, 30.f, 1.0e-2f) && FMath::IsNearlyEqual(Camera->GetOrbit().PitchDeg, -35.f, 1.0e-2f));

	// Manual control on: the orbit moves, inside the limits around the desired angle
	Section->Spec.bAllowManualControl = true;
	Section->Spec.Limits.YawLimitDeg = 20.f;
	Section->Spec.Limits.PitchMinOffsetDeg = -5.f;
	Section->Spec.Limits.PitchMaxOffsetDeg = 5.f;
	Camera->AddLookInput(FVector2D(10.0, 2.0));
	Camera->EvaluateCamera(Dt);
	TestTrue(TEXT("look moves the orbit"), Camera->GetOrbit().YawDeg > 30.f + 1.f);
	Camera->AddLookInput(FVector2D(120.0, 100.0));
	Camera->EvaluateCamera(Dt);
	TestTrue(TEXT("yaw clamped around the desired angle"), FMath::IsNearlyEqual(Camera->GetOrbit().YawDeg, 50.f, 1.0e-2f));
	TestTrue(TEXT("pitch clamped around the desired angle"), FMath::IsNearlyEqual(Camera->GetOrbit().PitchDeg, -30.f, 1.0e-2f));
	return true;
}

GSTN_CAM_RT_TEST(SectionPriorityAndShoulderPolicy)
{
	FRig Rig;
	AGSTN_CameraSection* Low = Rig.SpawnSection(FVector::ZeroVector, FVector(2000), 1);
	AGSTN_CameraSection* High = Rig.SpawnSection(FVector::ZeroVector, FVector(1000), 5);
	High->Spec.ShoulderPolicy = EGSTN_CameraShoulderPolicy::Forbid;
	UGSTN_CameraComponent* Camera = Rig.AddCamera(Rig.SpawnSubject(FVector(0, 0, 100)));
	Camera->SetSubjectOverride(MakeSubject(FVector(0, 0, 100)));
	Run(Camera, 0.2f);
	TestTrue(TEXT("the higher priority wins"), Camera->GetActiveSection() == High);

	// The section forbids Shoulder: the aim tag does not take the camera
	Camera->SetRequestTags(AimTags());
	Run(Camera, 1.0f);
	TestTrue(TEXT("Shoulder is blocked"), Camera->GetActiveModeTag().MatchesTagExact(GSTNCameraTags::Mode_Section.GetTag()));

	// Leave the small volume: the lower priority one takes over, Shoulder is allowed again
	Camera->SetSubjectOverride(MakeSubject(FVector(1500, 0, 100)));
	Run(Camera, 1.0f);
	TestTrue(TEXT("the outer section is active"), Camera->GetActiveSection() == Low);
	TestTrue(TEXT("aim takes Shoulder where it is allowed"), Camera->GetActiveModeTag().MatchesTagExact(GSTNCameraTags::Mode_Shoulder.GetTag()));
	return true;
}

GSTN_CAM_RT_TEST(FrameChangeRaisedOnSectionCutNotOnManualOrbit)
{
	FRig Rig;
	AGSTN_CameraSection* Section = Rig.SpawnSection(FVector(5000, 0, 0), FVector(500), 1);
	Section->Spec.DesiredYawDeg = 170.f;
	Section->Spec.bCutOnEnter = true;
	UGSTN_CameraComponent* Camera = Rig.AddCamera(Rig.SpawnSubject(FVector(0, 0, 100)));
	int32 Events = 0;
	FGSTN_CameraFrameChange Last;
	Camera->OnReferenceFrameChangedNative.AddLambda([&Events, &Last](const FGSTN_CameraFrameChange& Change)
	{
		++Events;
		Last = Change;
	});
	Camera->SetSubjectOverride(MakeSubject(FVector(0, 0, 100)));
	Run(Camera, 0.5f);

	// Manual orbit never raises it, however far it turns
	Camera->AddLookInput(FVector2D(160.0, 0.0));
	Run(Camera, 0.2f);
	TestEqual(TEXT("manual orbit raises nothing"), Events, 0);

	// Entering a section that turns the heading by about 90 degrees from the orbit (80 to 170) does
	Camera->SetSubjectOverride(MakeSubject(FVector(5000, 0, 100)));
	Run(Camera, 0.2f);
	TestEqual(TEXT("one event on the section cut"), Events, 1);
	TestTrue(TEXT("it is a cut for a section entry"), Last.bCut && Last.Reason == EGSTN_CameraFrameChangeReason::SectionEntered);
	TestTrue(TEXT("and turns the reference"), FMath::Abs(FGSTN_CameraMath::ShortestAngleDelta(
		FGSTN_CameraMath::DirectionToHeading(Camera->GetFrame(), Last.FromForward), FGSTN_CameraMath::DirectionToHeading(Camera->GetFrame(), Last.ToForward))) >= 20.f);
	return true;
}

GSTN_CAM_RT_TEST(DuelAndFreeModes)
{
	// Duel: both fighters parallel to the screen, centre between them
	UGSTN_CameraMode_Duel* Duel = NewObject<UGSTN_CameraMode_Duel>(GetTransientPackage());
	for (const FVector& Up : { FVector(0, 0, 1), FVector(1, 1, 1).GetSafeNormal(), FVector(0, 1, 0) })
	{
		Duel->ResetState();
		FGSTN_CameraModeContext Context;
		Context.Frame = FGSTN_CameraMath::MakeFrame(Up, FVector::ZeroVector);
		Context.DeltaSeconds = Dt;
		const FVector Axis = FGSTN_CameraMath::HeadingToDirection(Context.Frame, 20.f);
		Context.Subject = MakeSubject(FVector(10, 20, 30), Up);
		Context.Opponent = MakeSubject(FVector(10, 20, 30) + Axis * 500.0, Up);
		Context.bHasOpponent = true;
		const FGSTN_CameraView View = Duel->ComputeView(Context);
		const FVector Look = FGSTN_CameraMath::HeadingToDirection(Context.Frame, View.HeadingDeg);
		TestTrue(TEXT("looks perpendicular to the line between the fighters"), FMath::IsNearlyZero(FVector::DotProduct(Look, Axis), 1.0e-4));
		TestTrue(TEXT("pivot is the midpoint"), View.Pivot.Equals(FVector(10, 20, 30) + Axis * 250.0, 1.0e-2));
		TestTrue(TEXT("arm inside the range"), View.ArmCm >= Duel->Tuning.DuelMinArmCm && View.ArmCm <= Duel->Tuning.DuelMaxArmCm);
	}

	// Free: the pivot height follows the last ground, not the jump arc
	UGSTN_CameraMode_Free* Free = NewObject<UGSTN_CameraMode_Free>(GetTransientPackage());
	FGSTN_CameraModeContext Context;
	Context.Frame = FGSTN_CameraMath::MakeFrame(FVector::UpVector, FVector::ZeroVector);
	Context.DeltaSeconds = Dt;
	Context.Orbit.PitchDeg = -25.f;
	Context.Subject = MakeSubject(FVector(0, 0, 100));
	Context.Subject.bGrounded = true;
	Free->ComputeView(Context);
	Context.Subject = MakeSubject(FVector(0, 0, 300));
	Context.Subject.bGrounded = false;
	const FGSTN_CameraView Jump = Free->ComputeView(Context);
	TestTrue(TEXT("a jump inside the dead zone leaves the pivot on the ground"), FMath::Abs(Jump.Pivot.Z - 100.f) < 1.0);
	Context.Subject = MakeSubject(FVector(0, 0, 100));
	Context.Subject.bGrounded = true;
	const FGSTN_CameraView Landed = Free->ComputeView(Context);
	TestTrue(TEXT("landing keeps the pivot at the ground"), FMath::Abs(Landed.Pivot.Z - 100.f) < 40.0);
	return true;
}

GSTN_CAM_RT_TEST(BuiltinTagsAndMathLibrary)
{
	TestTrue(TEXT("mode tags are valid and under Camera.Mode"), GSTNCameraTags::Mode_Duel.GetTag().IsValid() && GSTNCameraTags::Mode_Free.GetTag().MatchesTag(GSTNCameraTags::Mode_Follow.GetTag().RequestDirectParent()));
	TestTrue(TEXT("synthetic state tags exist"), GSTNCameraTags::State_InSection.GetTag().IsValid() && GSTNCameraTags::State_ShoulderBlocked.GetTag().IsValid());
	for (const FVector& Up : { FVector(0, 0, 1), FVector(0.2, -0.7, 0.4).GetSafeNormal() })
	{
		const FVector Direction = UGSTN_CameraMath::DirectionFromHeading(Up, 57.f);
		TestTrue(TEXT("the library heading round trips"), FMath::IsNearlyEqual(UGSTN_CameraMath::HeadingAboutUp(Up, Direction), 57.f, 1.0e-3f));
	}
	UGSTN_CameraModeDefinition* Definition = NewObject<UGSTN_CameraModeDefinition>(GetTransientPackage());
	TestFalse(TEXT("an empty definition is invalid"), Definition->IsValidDefinition());
	Definition->ModeTag = GSTNCameraTags::Mode_Duel.GetTag();
	Definition->Mode = NewObject<UGSTN_CameraMode_Follow>(Definition);
	TestTrue(TEXT("a Camera.Mode tag and a mode are valid"), Definition->IsValidDefinition());
	Definition->ModeTag = GSTNCameraTags::Request_Aim.GetTag();
	TestFalse(TEXT("a tag outside Camera.Mode is invalid"), Definition->IsValidDefinition());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
