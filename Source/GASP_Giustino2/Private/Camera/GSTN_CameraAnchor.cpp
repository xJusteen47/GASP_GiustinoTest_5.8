#include "Camera/GSTN_CameraAnchor.h"

#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "MoverComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_CameraAnchor)

namespace GSTNCameraAnchor
{
	static TAutoConsoleVariable<int32> CVarEnable(TEXT("gstn.Camera.Anchor"), 1,
		TEXT("1 = the camera follows the damped, capsule-independent anchor (GSTN_CameraAnchor.h). 0 = GASP's actor-root camera (A/B). Presentation only"));
	static TAutoConsoleVariable<int32> CVarDebug(TEXT("gstn.Camera.Anchor.Debug"), 0,
		TEXT("1 = draw the camera anchor: red raw standing root, green damped anchor, yellow actor root"));

	/** Class names searched on the owner, in order, when no component carries UGSTN_CameraAnchorComponent::CameraComponentTag. By name: no
	 *  module dependency on GameplayCameras, and GASP may swap its camera component without breaking this */
	static const FName CameraClassNames[] = { TEXT("GameplayCameraComponent"), TEXT("SpringArmComponent"), TEXT("CameraComponent") };

	static bool IsOfClassNamed(const UObject* Object, FName ClassName)
	{
		for (const UClass* Class = Object ? Object->GetClass() : nullptr; Class; Class = Class->GetSuperClass())
		{
			if (Class->GetFName() == ClassName)
			{
				return true;
			}
		}
		return false;
	}

	/** The standing relative transform of a component: its template's (FGSTN_BodyCapsuleModifier rewrites the live one), else the live one */
	static FTransform StandingRelative(const USceneComponent& Component)
	{
		const USceneComponent* Archetype = Cast<USceneComponent>(Component.GetArchetype());
		if (Archetype && Archetype != &Component && !Archetype->HasAnyFlags(RF_ClassDefaultObject))
		{
			return Archetype->GetRelativeTransform();
		}
		return Component.GetRelativeTransform();
	}
}

const FName UGSTN_CameraAnchorComponent::CameraComponentTag(TEXT("GSTN_CameraAnchor"));

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Damper

void FGSTN_CameraAnchorDamper::Reset(const FVector& Target)
{
	Location = Target;
	VerticalSpeed = 0.0;
	HorizontalSpeed = FVector::ZeroVector;
	bValid = true;
}

double FGSTN_CameraAnchorDamper::SpringStep(double X, double Goal, double& InOutSpeed, float HalfLife, float DeltaTime)
{
	if (HalfLife <= KINDA_SMALL_NUMBER || DeltaTime <= 0.f)
	{
		if (HalfLife <= KINDA_SMALL_NUMBER)
		{
			InOutSpeed = 0.0;
			return Goal;
		}
		return X;
	}
	// Critically damped: never overshoots a still goal from rest, and HalfLife is the time to cover half of the distance
	const double Y = 2.0 * 0.69314718055994531 / HalfLife;
	const double J0 = X - Goal;
	const double J1 = InOutSpeed + J0 * Y;
	const double E = FMath::Exp(-Y * DeltaTime);
	InOutSpeed = E * (InOutSpeed - J1 * Y * DeltaTime);
	return E * (J0 + J1 * DeltaTime) + Goal;
}

FVector FGSTN_CameraAnchorDamper::Step(const FVector& Target, const FVector& InUp, float DeltaTime, const FGSTN_CameraAnchorSettings& Settings)
{
	if (!bValid || FVector::Dist(Target, Location) > Settings.SnapDistance)
	{
		Reset(Target);
		return Location;
	}
	const FVector Up = InUp.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);

	// Vertical: dead band, then the spring (up and down half lives differ), then the lag clamp
	const double Current = FVector::DotProduct(Location, Up);
	const double Wanted = FVector::DotProduct(Target, Up);
	const double Error = Wanted - Current;
	const double Goal = FMath::Abs(Error) <= Settings.DeadBand ? Current : Wanted - FMath::Sign(Error) * Settings.DeadBand;
	const float HalfLife = Goal >= Current ? Settings.UpHalfLife : Settings.DownHalfLife;
	double Vertical = SpringStep(Current, Goal, VerticalSpeed, HalfLife, DeltaTime);
	if (Settings.MaxVerticalLag > 0.f)
	{
		const double Clamped = FMath::Clamp(Vertical, Wanted - Settings.MaxVerticalLag, Wanted + Settings.MaxVerticalLag);
		if (Clamped != Vertical)
		{
			Vertical = Clamped;
			VerticalSpeed = 0.0;
		}
	}

	// Horizontal: the plane perpendicular to Up
	const FVector CurrentPlane = Location - Current * Up;
	const FVector WantedPlane = Target - Wanted * Up;
	FVector Plane = WantedPlane;
	if (Settings.HorizontalHalfLife > KINDA_SMALL_NUMBER)
	{
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			Plane[Axis] = SpringStep(CurrentPlane[Axis], WantedPlane[Axis], HorizontalSpeed[Axis], Settings.HorizontalHalfLife, DeltaTime);
		}
		Plane -= FVector::DotProduct(Plane, Up) * Up;
	}
	else
	{
		HorizontalSpeed = FVector::ZeroVector;
	}

	Location = Plane + Vertical * Up;
	return Location;
}

FVector FGSTN_CameraAnchorDamper::StandingRootFromVisual(const FTransform& VisualWorld, const FTransform& VisualStandingRelative, const FVector& ParentScale)
{
	// VisualWorld = StandingRelative * Parent with Parent the root as drawn: rotation from the mesh, location solved for
	const FQuat ParentRotation = VisualWorld.GetRotation() * VisualStandingRelative.GetRotation().Inverse();
	return VisualWorld.GetLocation() - ParentRotation.RotateVector(VisualStandingRelative.GetLocation() * ParentScale);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// Component

UGSTN_CameraAnchorComponent::UGSTN_CameraAnchorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// After Mover placed and smoothed the mesh and FGSTN_BodyCapsuleModifier shifted it, before the camera managers evaluate the view
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(false);
}

const FGSTN_CameraAnchorSettings& UGSTN_CameraAnchorComponent::GetSettings() const
{
	return SettingsAsset ? SettingsAsset->Settings : Settings;
}

FVector UGSTN_CameraAnchorComponent::GetViewShift() const
{
	return bShiftView ? LastCorrection : FVector::ZeroVector;
}

bool UGSTN_CameraAnchorComponent::IsActiveForOwner() const
{
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!Owner || !World || !GetSettings().bEnabled || GSTNCameraAnchor::CVarEnable.GetValueOnGameThread() == 0)
	{
		return false;
	}
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (Controller && Controller->IsLocalController() && Controller->GetViewTarget() == Owner)
		{
			return true;
		}
	}
	return false;
}

void UGSTN_CameraAnchorComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer)
	{
		SetComponentTickEnabled(false);
	}
}

void UGSTN_CameraAnchorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RestoreCamera();
	Super::EndPlay(EndPlayReason);
}

void UGSTN_CameraAnchorComponent::ResolveComponents()
{
	bResolved = true;
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// The drawn body: Mover's primary visual component, else the first skeletal mesh
	USceneComponent* Visual = nullptr;
	if (const UMoverComponent* Mover = Owner->FindComponentByClass<UMoverComponent>())
	{
		Visual = Mover->GetPrimaryVisualComponent();
	}
	if (!Visual)
	{
		Visual = Owner->FindComponentByClass<USkeletalMeshComponent>();
	}
	VisualComponent = Visual;
	if (Visual)
	{
		VisualRelativeStanding = GSTNCameraAnchor::StandingRelative(*Visual);
	}

	// The camera component: tagged, else by class name in order
	USceneComponent* Camera = nullptr;
	TInlineComponentArray<USceneComponent*> Scene(Owner);
	for (USceneComponent* Component : Scene)
	{
		if (Component && Component->ComponentHasTag(CameraComponentTag))
		{
			Camera = Component;
			break;
		}
	}
	for (const FName& ClassName : GSTNCameraAnchor::CameraClassNames)
	{
		for (int32 Index = 0; !Camera && Index < Scene.Num(); ++Index)
		{
			if (GSTNCameraAnchor::IsOfClassNamed(Scene[Index], ClassName))
			{
				Camera = Scene[Index];
			}
		}
	}
	if (Camera == Owner->GetRootComponent())
	{
		Camera = nullptr;  // moving the root would move the character
	}
	CameraComponent = Camera;
	if (Camera)
	{
		CameraRelativeStanding = GSTNCameraAnchor::StandingRelative(*Camera);
		bCameraFollowsVisual = Visual && Camera->IsAttachedTo(Visual);
	}
}

void UGSTN_CameraAnchorComponent::RestoreCamera()
{
	USceneComponent* Camera = CameraComponent.Get();
	if (bCameraMoved && Camera)
	{
		Camera->SetRelativeLocation(CameraRelativeStanding.GetLocation());
	}
	bCameraMoved = false;
}

void UGSTN_CameraAnchorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Owner = GetOwner();
	if (!Owner || !IsActiveForOwner())
	{
		RestoreCamera();
		Damper.bValid = false;
		LastCorrection = FVector::ZeroVector;
		bShiftView = false;
		return;
	}
	if (!bResolved || (!VisualComponent.IsValid() && !CameraComponent.IsValid()))
	{
		ResolveComponents();
	}
	const FGSTN_CameraAnchorSettings& Tuning = GetSettings();
	const FVector Root = Owner->GetActorLocation();
	const USceneComponent* Visual = VisualComponent.Get();
	const FVector ParentScale = Owner->GetRootComponent() ? Owner->GetRootComponent()->GetComponentScale() : FVector::OneVector;
	const FVector Raw = Visual ? FGSTN_CameraAnchorDamper::StandingRootFromVisual(Visual->GetComponentTransform(), VisualRelativeStanding, ParentScale) : Root;
	const UMoverComponent* Mover = Owner->FindComponentByClass<UMoverComponent>();
	const FVector Up = Mover ? Mover->GetUpDirection() : Owner->GetActorUpVector();
	const FVector Damped = Damper.Step(Raw, Up, DeltaTime, Tuning);

	USceneComponent* Camera = CameraComponent.Get();
	if (Camera && Tuning.Mode == EGSTN_CameraAnchorMode::MoveCameraComponent)
	{
		// Where the component would be at its standing relative location this frame, then moved by the anchor's correction. A camera hung on
		// the mesh already follows the drawn body: only the damping is added
		const USceneComponent* Parent = Camera->GetAttachParent();
		const FTransform ParentWorld = Parent ? Parent->GetSocketTransform(Camera->GetAttachSocketName()) : FTransform::Identity;
		const FVector Unmoved = ParentWorld.TransformPosition(CameraRelativeStanding.GetLocation());
		LastCorrection = Damped - (bCameraFollowsVisual ? Raw : Root);
		Camera->SetWorldLocation(Unmoved + LastCorrection);
		bCameraMoved = true;
		bShiftView = false;
	}
	else
	{
		RestoreCamera();
		LastCorrection = Damped - Root;
		bShiftView = true;
	}

#if ENABLE_DRAW_DEBUG
	if (GSTNCameraAnchor::CVarDebug.GetValueOnGameThread() != 0)
	{
		DrawDebugSphere(GetWorld(), Raw, 8.f, 8, FColor::Red, false, -1.f, SDPG_Foreground);
		DrawDebugSphere(GetWorld(), Damped, 10.f, 8, FColor::Green, false, -1.f, SDPG_Foreground);
		DrawDebugSphere(GetWorld(), Root, 6.f, 8, FColor::Yellow, false, -1.f, SDPG_Foreground);
	}
#endif
}
