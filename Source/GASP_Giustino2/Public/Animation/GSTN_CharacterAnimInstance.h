// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "GSTN_CharacterAnimInstance.generated.h"

/**
 * C++ parent for the character's main anim graph (reparent the GASP Mover ABP to it; it adds nothing else, so the GASP graph keeps
 * working as it is). It knows no feature: every frame it asks the linked anim layers that implement IGSTN_OverrideLayerSource whether
 * one wants to take over the pose, and exposes the answer for one "Blend Poses by bool" in front of a Linked Anim Layer node.
 * See docs/architecture/anim-layers.md.
 */
UCLASS(Blueprintable)
class GASP_GIUSTINO2_API UGSTN_CharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	/** A linked layer wants to replace the main pose this frame */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Override Layer")
	bool bOverrideLayerActive = false;

	/** Blend time of the winning layer, or of the one that just stopped (so the blend out uses the same time) */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Override Layer")
	float OverrideBlendTime = 0.15f;

	/** Class of the winning layer (debug and Blueprint branching); null when none is active */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Animation|Override Layer")
	TObjectPtr<UClass> ActiveOverrideLayerClass;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};
