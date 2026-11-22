#include "GSTN_DepthSubjectAdapter_Mover.h"

#include "Components/CapsuleComponent.h"
#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "GameFramework/Actor.h"
#include "MoverComponent.h"

namespace GSTNDepthMarkerMoverPrivate
{
	static const UMoverComponent* FindMover(const AActor* Actor)
	{
		return Actor ? Actor->FindComponentByClass<UMoverComponent>() : nullptr;
	}
}

bool UGSTN_DepthSubjectAdapter_Mover::CanHandle(const AActor* Actor) const
{
	const UMoverComponent* Mover = GSTNDepthMarkerMoverPrivate::FindMover(Actor);
	return Mover && Mover->GetUpdatedComponent();
}

FTransform UGSTN_DepthSubjectAdapter_Mover::PresentedRootFromVisual(const FTransform& VisualWorld, const FTransform& BaseVisualRelative)
{
	return BaseVisualRelative.Inverse() * VisualWorld;
}

FTransform UGSTN_DepthSubjectAdapter_Mover::GetPresentedRoot(const UMoverComponent& Mover) const
{
	const USceneComponent* Root = Mover.GetUpdatedComponent();
	const FTransform Simulated = Root ? Root->GetComponentTransform() : FTransform::Identity;
	const USceneComponent* Visual = Mover.GetPrimaryVisualComponent();
	if (!bFollowPresentedMesh || !Root || !Visual || Visual == Root || Mover.SmoothingMode != EMoverSmoothingMode::VisualComponentOffset)
	{
		return Simulated;
	}
	FTransform Presented = PresentedRootFromVisual(Visual->GetComponentTransform(), Mover.GetBaseVisualComponentTransform());
	Presented.SetScale3D(Simulated.GetScale3D());
	return Presented;
}

bool UGSTN_DepthSubjectAdapter_Mover::ReadState(const AActor* Actor, FGSTN_DepthSubjectState& OutState) const
{
	const UMoverComponent* Mover = GSTNDepthMarkerMoverPrivate::FindMover(Actor);
	const USceneComponent* Root = Mover ? Mover->GetUpdatedComponent() : nullptr;
	if (!Root)
	{
		return false;
	}

	// The pawn's own up (local gravity), never world Z
	FVector Up = Mover->GetUpDirection();
	if (!Up.Normalize())
	{
		Up = FVector::UpVector;
	}

	float HalfHeight = 0.f;
	float Radius = 0.f;
	if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Root))
	{
		HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		Radius = Capsule->GetScaledCapsuleRadius();
	}

	const FTransform Presented = GetPresentedRoot(*Mover);

	OutState = FGSTN_DepthSubjectState();
	OutState.Up = Up;
	OutState.Feet = Presented.GetLocation() - Up * HalfHeight;
	if (HalfHeight > 0.f)
	{
		OutState.TopOffset = HalfHeight * 2.f;
	}
	if (Radius > 0.f)
	{
		OutState.FootRadius = Radius;
	}
	OutState.Velocity = Mover->GetVelocity();

	if (const UCharacterMoverComponent* Character = Cast<UCharacterMoverComponent>(Mover))
	{
		OutState.bAirborne = Character->IsAirborne();
		OutState.bGrounded = Character->IsOnGround();
	}
	else
	{
		OutState.bAirborne = FMath::Abs(FVector::DotProduct(OutState.Velocity, Up)) > AirborneSpeed;
		OutState.bGrounded = !OutState.bAirborne;
	}
	if (OutState.bGrounded)
	{
		UPrimitiveComponent* Base = Mover->GetMovementBase();
		OutState.BaseComponent = Base;
		OutState.bBaseMoves = Base && Base->Mobility == EComponentMobility::Movable;
	}
	return true;
}
