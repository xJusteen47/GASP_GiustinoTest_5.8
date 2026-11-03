// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/Modes/GSTN_SimpleWallMovementMode.h"

#include "WallMovement/MoveLibrary/GSTN_WallMovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_SimpleWallMovementMode)

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

FVector UGSTN_SimpleWallMovementMode::ComputeTargetUp(const FQuat& CurrentQuat, const FVector& WallNormal, const FVector& GravityUp, const FVector& Velocity, float DeltaSeconds) const
{
	FVector Up = Super::ComputeTargetUp(CurrentQuat, WallNormal, GravityUp, Velocity, DeltaSeconds);
	if (!bAlignHeadingToMovementOnCeilings || CeilingHeadingSharpness <= 0.f || FVector::DotProduct(WallNormal, GravityUp) >= 0.f)
	{
		return Up;
	}

	// Fades in as the upright bias fades out (1 - verticality), and with the speed
	const float Verticality = FVector::VectorPlaneProject(GravityUp, WallNormal).Size();
	const FVector MoveDir = FVector::VectorPlaneProject(Velocity, WallNormal).GetSafeNormal();
	const float SpeedFactor = FMath::Clamp(Velocity.Size() / FMath::Max(Horizontal.MaxSpeed * 0.5f, 1.f), 0.f, 1.f);
	const float CeilingFactor = 1.f - Verticality;
	if (!MoveDir.IsNearlyZero() && SpeedFactor > 0.f)
	{
		const float HeadingAlpha = UGSTN_WallMovementUtils::ExpAlpha(CeilingHeadingSharpness * CeilingFactor * SpeedFactor, DeltaSeconds);
		Up = UGSTN_WallMovementUtils::RotateTowardsInPlane(Up, MoveDir, WallNormal, HeadingAlpha);
	}
	return Up;
}
