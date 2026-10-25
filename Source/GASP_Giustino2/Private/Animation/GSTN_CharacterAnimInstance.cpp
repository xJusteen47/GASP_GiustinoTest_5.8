// Fill out your copyright notice in the Description page of Project Settings.

#include "Animation/GSTN_CharacterAnimInstance.h"

#include "Animation/GSTN_OverrideLayerSource.h"
#include "Components/SkeletalMeshComponent.h"

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
}
