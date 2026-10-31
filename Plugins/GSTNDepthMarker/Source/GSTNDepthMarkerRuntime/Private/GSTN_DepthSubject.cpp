#include "GSTN_DepthSubject.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Components/MeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace GSTNDepthMarkerSubjectPrivate
{
	static TArray<TSubclassOf<UGSTN_DepthSubjectAdapter>>& Classes()
	{
		static TArray<TSubclassOf<UGSTN_DepthSubjectAdapter>> Registered;
		return Registered;
	}
}

void FGSTN_DepthAdapterRegistry::Register(TSubclassOf<UGSTN_DepthSubjectAdapter> AdapterClass)
{
	if (AdapterClass)
	{
		GSTNDepthMarkerSubjectPrivate::Classes().AddUnique(AdapterClass);
	}
}

void FGSTN_DepthAdapterRegistry::Unregister(TSubclassOf<UGSTN_DepthSubjectAdapter> AdapterClass)
{
	GSTNDepthMarkerSubjectPrivate::Classes().Remove(AdapterClass);
}

int32 FGSTN_DepthAdapterRegistry::Num()
{
	return GSTNDepthMarkerSubjectPrivate::Classes().Num();
}

const UGSTN_DepthSubjectAdapter* FGSTN_DepthAdapterRegistry::Resolve(const AActor* Actor)
{
	const UGSTN_DepthSubjectAdapter* Best = nullptr;
	for (const TSubclassOf<UGSTN_DepthSubjectAdapter>& Class : GSTNDepthMarkerSubjectPrivate::Classes())
	{
		const UGSTN_DepthSubjectAdapter* Candidate = Class ? Class->GetDefaultObject<UGSTN_DepthSubjectAdapter>() : nullptr;
		// Later registrations win ties, so a project adapter at the same priority replaces a built-in one
		if (Candidate && Candidate->CanHandle(Actor) && (!Best || Candidate->Priority >= Best->Priority))
		{
			Best = Candidate;
		}
	}
	return Best;
}

FVector UGSTN_DepthSubjectAdapter::GetPresentedOffset(const AActor* Actor)
{
	if (!Actor)
	{
		return FVector::ZeroVector;
	}
	const USceneComponent* Visual = nullptr;
	if (const ACharacter* Character = Cast<ACharacter>(Actor))
	{
		Visual = Character->GetMesh();
	}
	if (!Visual)
	{
		TInlineComponentArray<const UMeshComponent*> Meshes(Actor);
		for (const UMeshComponent* Mesh : Meshes)
		{
			if (Mesh != Actor->GetRootComponent() && Mesh->GetAttachParent())
			{
				Visual = Mesh;
				break;
			}
		}
	}
	const USceneComponent* Parent = Visual ? Visual->GetAttachParent() : nullptr;
	const USceneComponent* Archetype = Visual ? Cast<USceneComponent>(Visual->GetArchetype()) : nullptr;
	if (!Parent || !Archetype || Archetype == Visual)
	{
		return FVector::ZeroVector;
	}
	return Visual->GetComponentLocation() - Parent->GetComponentTransform().TransformPosition(Archetype->GetRelativeLocation());
}

bool UGSTN_DepthSubjectAdapter_CharacterMovement::CanHandle(const AActor* Actor) const
{
	const ACharacter* Character = Cast<ACharacter>(Actor);
	return Character && Character->GetCharacterMovement() && Character->GetCapsuleComponent();
}

bool UGSTN_DepthSubjectAdapter_CharacterMovement::ReadState(const AActor* Actor, FGSTN_DepthSubjectState& OutState) const
{
	const ACharacter* Character = Cast<ACharacter>(Actor);
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	if (!Movement || !Capsule)
	{
		return false;
	}

	// The character's own local gravity, never world Z
	FVector Up = -Movement->GetGravityDirection();
	if (!Up.Normalize())
	{
		Up = FVector::UpVector;
	}
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	OutState = FGSTN_DepthSubjectState();
	OutState.Up = Up;
	OutState.Feet = Capsule->GetComponentLocation() - Up * HalfHeight + (bFollowPresentedMesh ? GetPresentedOffset(Actor) : FVector::ZeroVector);
	OutState.bAirborne = Movement->IsFalling();
	OutState.bGrounded = Movement->IsMovingOnGround();
	OutState.FootRadius = Capsule->GetScaledCapsuleRadius();
	OutState.TopOffset = HalfHeight * 2.f;
	OutState.Velocity = Movement->Velocity;
	if (OutState.bGrounded)
	{
		const UPrimitiveComponent* Base = Movement->CurrentFloor.HitResult.GetComponent();
		OutState.BaseComponent = const_cast<UPrimitiveComponent*>(Base);
		OutState.bBaseMoves = Base && Base->Mobility == EComponentMobility::Movable;
	}
	return true;
}

bool UGSTN_DepthSubjectAdapter_Actor::ReadState(const AActor* Actor, FGSTN_DepthSubjectState& OutState) const
{
	if (!Actor)
	{
		return false;
	}
	const UWorld* World = Actor->GetWorld();
	const float GravityZ = World ? World->GetGravityZ() : -980.f;

	OutState = FGSTN_DepthSubjectState();
	OutState.Up = GravityZ > 0.f ? FVector::DownVector : FVector::UpVector;
	OutState.Feet = Actor->GetActorLocation() + (bFollowPresentedMesh ? GetPresentedOffset(Actor) : FVector::ZeroVector);
	OutState.Velocity = Actor->GetVelocity();
	OutState.bAirborne = FMath::Abs(FVector::DotProduct(OutState.Velocity, OutState.Up)) > AirborneSpeed;
	OutState.bGrounded = !OutState.bAirborne;
	return true;
}
