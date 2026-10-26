// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/Animation/GSTN_WallAnimInstance.h"

#include "GameFramework/Actor.h"
#include "MoverComponent.h"
#include "WallMovement/Animation/GSTN_WallAnimUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallAnimInstance)

void UGSTN_WallAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	WallAnim = FGSTN_WallAnimState();
	FindMoverComponent();
}

void UGSTN_WallAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!MoverComponent.IsValid())
	{
		FindMoverComponent();
	}

	// A missing Mover component (editor preview) reads as "not on a wall": the layer then fades out like a normal exit
	const FGSTN_WallAnimInputs Inputs = UGSTN_WallAnimUtils::GatherInputs(MoverComponent.Get(), Settings);
	WallAnim = UGSTN_WallAnimUtils::StepWallAnimState(WallAnim, Inputs, Settings, DeltaSeconds);
}

void UGSTN_WallAnimInstance::FindMoverComponent()
{
	const AActor* Owner = GetOwningActor();
	MoverComponent = Owner ? Owner->FindComponentByClass<UMoverComponent>() : nullptr;
}
