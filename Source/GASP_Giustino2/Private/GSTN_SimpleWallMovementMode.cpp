// Fill out your copyright notice in the Description page of Project Settings.

#include "GSTN_SimpleWallMovementMode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_SimpleWallMovementMode)

UGSTN_SimpleWallMovementMode::UGSTN_SimpleWallMovementMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UGSTN_SimpleWallMovementMode::PostLoad()
{
	Super::PostLoad();

	// Assets saved before the horizontal/vertical split carry MaxSpeed/Acceleration/Deceleration/*SpeedScale. If any differs
	// from its old default it was set on purpose: rebuild the new settings from them, then reset them so this runs once.
	const UGSTN_SimpleWallMovementMode* Defaults = GetDefault<UGSTN_SimpleWallMovementMode>();
	const bool bHasLegacyValues = !FMath::IsNearlyEqual(MaxSpeed, Defaults->MaxSpeed)
		|| !FMath::IsNearlyEqual(Acceleration, Defaults->Acceleration)
		|| !FMath::IsNearlyEqual(Deceleration, Defaults->Deceleration)
		|| !FMath::IsNearlyEqual(UpSpeedScale, Defaults->UpSpeedScale)
		|| !FMath::IsNearlyEqual(DownSpeedScale, Defaults->DownSpeedScale)
		|| !FMath::IsNearlyEqual(LateralSpeedScale, Defaults->LateralSpeedScale);

	if (bHasLegacyValues && this != Defaults)
	{
		Horizontal.MaxSpeed = MaxSpeed * LateralSpeedScale;
		Horizontal.Acceleration = Acceleration;
		Horizontal.BrakingDeceleration = Deceleration;

		Vertical.MaxUpSpeed = MaxSpeed * UpSpeedScale;
		Vertical.MaxDownSpeed = MaxSpeed * DownSpeedScale;
		Vertical.Acceleration = Acceleration;
		Vertical.BrakingDeceleration = Deceleration;

		MaxSpeed = Defaults->MaxSpeed;
		Acceleration = Defaults->Acceleration;
		Deceleration = Defaults->Deceleration;
		UpSpeedScale = Defaults->UpSpeedScale;
		DownSpeedScale = Defaults->DownSpeedScale;
		LateralSpeedScale = Defaults->LateralSpeedScale;
	}
}
