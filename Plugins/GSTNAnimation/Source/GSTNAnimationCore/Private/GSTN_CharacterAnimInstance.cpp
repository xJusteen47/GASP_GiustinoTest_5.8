// Feature animation decoupled from the main graph (no GAS, no Network Prediction, no Mover).

#include "GSTN_CharacterAnimInstance.h"

#include "GSTN_OverrideLayerSource.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_CharacterAnimInstance)

void UGSTN_CharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	const IGSTN_OverrideLayerSource* Best = nullptr;
	const UAnimInstance* BestInstance = nullptr;
	if (const USkeletalMeshComponent* Mesh = GetOwningComponent())
	{
		// Linked instances are updated before the main one, so their answers are this frame's
		for (const UAnimInstance* Linked : Mesh->GetLinkedAnimInstances())
		{
			const IGSTN_OverrideLayerSource* Source = Cast<IGSTN_OverrideLayerSource>(Linked);
			if (Source && Source->IsOverrideLayerActive() && (!Best || Source->GetOverridePriority() > Best->GetOverridePriority()))
			{
				Best = Source;
				BestInstance = Linked;
			}
		}
	}

	bOverrideLayerActive = Best != nullptr;
	ActiveOverrideLayerClass = BestInstance ? BestInstance->GetClass() : nullptr;
	if (Best)
	{
		OverrideBlendTime = Best->GetOverrideBlendTime();
	}

	UpdateFootLockRequest(DeltaSeconds);
}

void UGSTN_CharacterAnimInstance::UpdateFootLockRequest(float DeltaSeconds)
{
	// A montage "drives" the pose while it is not stopping (playing or blending in); a stopping one only adds its fading weight
	bool bMontageDriving = false;
	float MaxWeight = 0.f;
	for (const FAnimMontageInstance* Instance : MontageInstances)
	{
		if (Instance && Instance->IsValid())
		{
			MaxWeight = FMath::Max(MaxWeight, Instance->GetWeight());
			bMontageDriving |= !Instance->IsStopped();
		}
	}
	FootLockMontageWeight = MaxWeight;

	// Capsule speed from the actor's own motion (no movement component knowledge: this module depends on no Mover)
	float Speed = 0.f;
	if (const AActor* Owner = GetOwningActor())
	{
		const FVector Location = Owner->GetActorLocation();
		if (bFootLockHasLastLocation && DeltaSeconds > KINDA_SMALL_NUMBER)
		{
			Speed = FVector::Dist(Location, FootLockLastActorLocation) / DeltaSeconds;
		}
		FootLockLastActorLocation = Location;
		bFootLockHasLastLocation = true;
	}
	bFootLockMoving = Speed > FootLockStillSpeed;

	if (bMontageDriving)
	{
		// Arms the window for when this montage stops; the montage's own frames are never touched
		bFootLockWindowOpen = true;
		FootLockWindowTime = 0.f;
		bFootLockInstantRelease = true;
		bFootLockRequest = false;
		return;
	}
	bFootLockInstantRelease = false;
	if (bFootLockWindowOpen)
	{
		FootLockWindowTime += DeltaSeconds;
		// Moving (walk away, knockback, falling) or the time cap closes the window for good: no re-lock when the capsule stops again
		if (bFootLockMoving || FootLockWindowTime > FootLockMaxHold)
		{
			bFootLockWindowOpen = false;
		}
	}
	bFootLockRequest = bFootLockWindowOpen;
}
