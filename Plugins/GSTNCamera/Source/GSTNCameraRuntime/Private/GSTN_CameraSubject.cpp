#include "GSTN_CameraSubjectAdapters.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY(LogGSTNCamera);

TArray<TWeakObjectPtr<UClass>>& FGSTN_CameraAdapterRegistry::Classes()
{
	static TArray<TWeakObjectPtr<UClass>> Registered;
	return Registered;
}

void FGSTN_CameraAdapterRegistry::Register(UClass* AdapterClass)
{
	if (AdapterClass && AdapterClass->IsChildOf(UGSTN_CameraSubjectAdapter::StaticClass()))
	{
		Classes().AddUnique(AdapterClass);
	}
}

void FGSTN_CameraAdapterRegistry::Unregister(UClass* AdapterClass)
{
	Classes().Remove(AdapterClass);
}

const UGSTN_CameraSubjectAdapter* FGSTN_CameraAdapterRegistry::Find(const AActor* Actor)
{
	const UGSTN_CameraSubjectAdapter* Best = nullptr;
	for (const TWeakObjectPtr<UClass>& Class : Classes())
	{
		const UGSTN_CameraSubjectAdapter* Candidate = Class.IsValid() ? Class->GetDefaultObject<UGSTN_CameraSubjectAdapter>() : nullptr;
		if (Candidate && Candidate->CanRead(Actor) && (!Best || Candidate->GetPriority() >= Best->GetPriority()))
		{
			Best = Candidate;
		}
	}
	return Best;
}

bool FGSTN_CameraAdapterRegistry::Read(const AActor* Actor, FGSTN_CameraSubjectState& OutState)
{
	if (!Actor)
	{
		return false;
	}
	if (Actor->Implements<UGSTN_CameraSubject>())
	{
		FGSTN_CameraSubjectState State;
		if (IGSTN_CameraSubject::Execute_GetCameraSubjectState(Actor, State))
		{
			OutState = State;
			OutState.bValid = true;
			return true;
		}
	}
	if (const UGSTN_CameraSubjectAdapter* Adapter = Find(Actor))
	{
		FGSTN_CameraSubjectState State;
		if (Adapter->Read(Actor, State))
		{
			OutState = State;
			OutState.bValid = true;
			return true;
		}
	}
	return false;
}

bool UGSTN_CameraSubjectAdapter_Actor::Read(const AActor* Actor, FGSTN_CameraSubjectState& OutState) const
{
	if (!Actor)
	{
		return false;
	}
	OutState.Pivot = Actor->GetActorLocation();
	OutState.Up = FVector::UpVector;
	OutState.Velocity = Actor->GetVelocity();
	OutState.Facing = Actor->GetActorForwardVector();
	OutState.bGrounded = true;
	OutState.bSuppressed = false;
	return true;
}

bool UGSTN_CameraSubjectAdapter_Character::CanRead(const AActor* Actor) const
{
	const ACharacter* Character = Cast<ACharacter>(Actor);
	return Character && Character->GetCharacterMovement();
}

bool UGSTN_CameraSubjectAdapter_Character::Read(const AActor* Actor, FGSTN_CameraSubjectState& OutState) const
{
	const ACharacter* Character = Cast<ACharacter>(Actor);
	const UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return false;
	}
	OutState.Pivot = Character->GetActorLocation();
	const FVector Gravity = Movement->GetGravityDirection();
	OutState.Up = Gravity.IsNearlyZero() ? FVector::UpVector : -Gravity.GetSafeNormal();
	OutState.Velocity = Character->GetVelocity();
	OutState.Facing = Character->GetActorForwardVector();
	OutState.bGrounded = Movement->IsMovingOnGround();
	OutState.bSuppressed = false;
	return true;
}
