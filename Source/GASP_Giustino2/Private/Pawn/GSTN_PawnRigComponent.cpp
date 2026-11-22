#include "Pawn/GSTN_PawnRigComponent.h"

#include "Camera/CameraComponent.h"
#include "Camera/GSTN_CameraAnchor.h"
#include "Camera/GSTN_CameraAnchorLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GSTN_CameraComponent.h"
#include "GSTN_CameraProfile.h"
#include "GSTN_CameraSubjectAdapters.h"
#include "GSTN_CameraTags.h"
#include "GSTN_InteractorComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace GSTNPawnRig
{
	static TAutoConsoleVariable<int32> CVarAutoRig(TEXT("gstn.G5.AutoRig"), 0,
		TEXT("1 = every locally viewed pawn gets a GSTN pawn rig (interact focus + prompt, GSTN camera). Default 0: the GASP camera stays the view."), ECVF_Default);
}

UGSTN_PawnRigComponent::UGSTN_PawnRigComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// With the camera and the anchor: the press tracker and the camera order are settled before the view of the frame is built
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(false);
}

bool UGSTN_PawnRigComponent::ShouldRigRunFor(ENetMode NetMode, bool bGameWorld)
{
	return bGameWorld && NetMode != NM_DedicatedServer;
}

FGSTN_CameraSubjectState UGSTN_PawnRigComponent::BuildPresentedSubject(const FGSTN_CameraSubjectState& Raw, bool bHasPresentedPivot, const FVector& PresentedPivot, bool bSuppressed)
{
	FGSTN_CameraSubjectState Out = Raw;
	Out.bValid = true;
	if (bHasPresentedPivot)
	{
		Out.Pivot = PresentedPivot;
	}
	Out.bSuppressed = Raw.bSuppressed || bSuppressed;
	return Out;
}

bool UGSTN_PawnRigComponent::GetCameraSubjectState_Implementation(FGSTN_CameraSubjectState& OutState) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	// The raw state goes through the adapter directly, never through FGSTN_CameraAdapterRegistry::Read: that would find this component again
	FGSTN_CameraSubjectState Raw;
	const UGSTN_CameraSubjectAdapter* Adapter = FGSTN_CameraAdapterRegistry::Find(Owner);
	if (!Adapter || !Adapter->Read(Owner, Raw))
	{
		Raw.bValid = true;
		Raw.Pivot = Owner->GetActorLocation();
		Raw.Velocity = Owner->GetVelocity();
		Raw.Facing = Owner->GetActorForwardVector();
	}

	// C1: the standing root of the drawn, Mover smoothed body, free of capsule resizes and of the 62.5 Hz steps of the actor root, never the actor
	// root. The anchor's UNdamped value on purpose (as the P1a camera wiring does): the Free mode already holds the ground height in the air and
	// lags the pivot, and damping the anchor too would double it. The anchor knows the air bias, so it wins over the stateless library
	FVector Presented = Owner->GetActorLocation();
	const UGSTN_CameraAnchorComponent* Anchor = Owner->FindComponentByClass<UGSTN_CameraAnchorComponent>();
	if (Anchor && Anchor->IsActiveForOwner())
	{
		Presented = Anchor->GetStandingRoot();
	}
	else
	{
		UGSTN_CameraAnchorLibrary::GetCameraPivot(Owner, Presented);
	}
	OutState = BuildPresentedSubject(Raw, true, Presented, bSubjectSuppressed);
	return true;
}

void UGSTN_PawnRigComponent::GetCameraTags_Implementation(FGameplayTagContainer& OutTags) const
{
	if (bAimRequested)
	{
		OutTags.AddTag(GSTNCameraTags::Request_Aim);
	}
}

void UGSTN_PawnRigComponent::BeginPlay()
{
	Super::BeginPlay();
	const UWorld* World = GetWorld();
	if (!World || !ShouldRigRunFor(World->GetNetMode(), World->IsGameWorld()))
	{
		return;
	}
	BuildRig();
	SetComponentTickEnabled(true);
}

void UGSTN_PawnRigComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Press.bDown)
	{
		InteractReleased();
	}
	Super::EndPlay(EndPlayReason);
}

void UGSTN_PawnRigComponent::BuildRig()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (bAddInteractor)
	{
		UGSTN_InteractorComponent* Found = Owner->FindComponentByClass<UGSTN_InteractorComponent>();
		if (!Found)
		{
			Found = NewObject<UGSTN_InteractorComponent>(Owner, TEXT("GSTN_RigInteractor"));
			Found->RegisterComponent();
		}
		Interactor = Found;
	}

	if (bAddCamera)
	{
		UGSTN_CameraComponent* Found = Owner->FindComponentByClass<UGSTN_CameraComponent>();
		if (!Found)
		{
			Found = NewObject<UGSTN_CameraComponent>(Owner, TEXT("GSTN_RigCamera"));
			if (USceneComponent* Root = Owner->GetRootComponent())
			{
				Found->SetupAttachment(Root);
			}
			Found->RegisterComponent();
			bAddedCamera = true;
		}
		if (CameraProfile && !Found->Profile)
		{
			Found->Profile = CameraProfile;
		}
		Camera = Found;
		if (bDeactivateOtherCameras)
		{
			TInlineComponentArray<UCameraComponent*> Cameras(Owner);
			for (UCameraComponent* Other : Cameras)
			{
				if (Other && Other != Found && Other->IsActive())
				{
					Other->Deactivate();
				}
			}
		}
		Found->Activate(true);
	}
	WireTickOrder();
}

void UGSTN_PawnRigComponent::WireTickOrder()
{
	const AActor* Owner = GetOwner();
	UGSTN_CameraAnchorComponent* Anchor = Owner ? Owner->FindComponentByClass<UGSTN_CameraAnchorComponent>() : nullptr;
	if (!Anchor || Anchor == WiredAnchor.Get())
	{
		return;
	}
	WiredAnchor = Anchor;
	// Anchor, then the rig, then the camera: all three are TG_PostPhysics, so the order is a prerequisite chain
	AddTickPrerequisiteComponent(Anchor);
	if (UGSTN_CameraComponent* Cam = Camera.Get())
	{
		Cam->AddTickPrerequisiteComponent(this);
	}
}

void UGSTN_PawnRigComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// The camera anchor is added at runtime by the playtest subsystem to locally viewed pawns: pick it up when it appears
	if (!WiredAnchor.IsValid())
	{
		WireTickOrder();
	}
	if (UGSTN_InteractorComponent* Interact = Interactor.Get())
	{
		if (Press.TickHold(Interact->GetHoldProgress()))
		{
			Interact->Interact(EGSTN_InteractVerb::Hold);
		}
	}
}

void UGSTN_PawnRigComponent::InteractPressed()
{
	if (UGSTN_InteractorComponent* Interact = Interactor.Get())
	{
		Press.Press();
		Interact->SetInteractHeld(true);
	}
}

void UGSTN_PawnRigComponent::InteractReleased()
{
	if (UGSTN_InteractorComponent* Interact = Interactor.Get())
	{
		Interact->SetInteractHeld(false);
		if (Press.Release())
		{
			Interact->Interact(EGSTN_InteractVerb::Tap);
		}
	}
	else
	{
		Press.Release();
	}
}

bool UGSTN_PawnRigSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && UGSTN_PawnRigComponent::ShouldRigRunFor(World->GetNetMode(), World->IsGameWorld());
}

void UGSTN_PawnRigSubsystem::Tick(float DeltaTime)
{
	if (GSTNPawnRig::CVarAutoRig.GetValueOnGameThread() == 0)
	{
		return;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		APawn* Viewed = Controller && Controller->IsLocalController() ? Cast<APawn>(Controller->GetViewTarget()) : nullptr;
		if (Viewed && !Viewed->FindComponentByClass<UGSTN_PawnRigComponent>())
		{
			UGSTN_PawnRigComponent* Rig = NewObject<UGSTN_PawnRigComponent>(Viewed, TEXT("GSTN_PawnRig"), RF_Transient);
			Rig->RegisterComponent();
		}
	}
}

TStatId UGSTN_PawnRigSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGSTN_PawnRigSubsystem, STATGROUP_Tickables);
}
