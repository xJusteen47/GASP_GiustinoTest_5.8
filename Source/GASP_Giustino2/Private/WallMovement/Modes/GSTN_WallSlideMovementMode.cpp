// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/Modes/GSTN_WallSlideMovementMode.h"

#include "MoverComponent.h"
#include "WallMovement/GSTN_WallMovementDebug.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallSlideMovementMode)

UGSTN_WallSlideMovementMode::UGSTN_WallSlideMovementMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Defaults tuned on SandboxCharacter_Mover
	Horizontal.Friction = 3.f;
	Horizontal.BrakingDeceleration = 0.f;
	InputMapping = EGSTN_WallInputMapping::CameraRelative;

	WallJump.bEnableWallJump = true;
	WallJump.AwaySpeed = 400.f;
	WallJump.UpSpeed = 300.f;

	MinSurfaceAngle = 45.f;
	MaxSurfaceAngle = 120.f;
	WallProbeDistance = 30.f;
	MaxNormalChangeRate = 600.f;
}

FGSTN_WallVerticalMovement UGSTN_WallSlideMovementMode::GetVerticalMovement() const
{
	// Never a stop snap here: it would block the slide
	FGSTN_WallVerticalMovement Movement = Super::GetVerticalMovement();
	Movement.BrakingFriction = Slide.Friction;
	Movement.TerminalUpSpeed = Slide.TerminalUpSpeed;
	Movement.TerminalDownSpeed = Slide.TerminalDownSpeed;
	return Movement;
}

float UGSTN_WallSlideMovementMode::ComputeVerticalSpeed(const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent, float VerticalSpeed, float DeltaSeconds) const
{
	VerticalSpeed = Super::ComputeVerticalSpeed(Plane, WallIntent, VerticalSpeed, DeltaSeconds);

	// Gravity projected on the vertical axis is negative (pulls down the wall). Moving up = decelerating, moving down = accelerating
	const float GravityAlongWall = FVector::DotProduct(GetMoverComponent()->GetGravityAcceleration(), Plane.VerticalAxis);
	const float GravityScale = (VerticalSpeed > 0.f) ? Slide.AscendingGravityScale : Slide.DescendingGravityScale;
	GSTN_WALL_LOG(2, TEXT("GEN slide gAlong=%.0f scale=%.2f"), GravityAlongWall, GravityScale);

	return VerticalSpeed + GravityAlongWall * GravityScale * DeltaSeconds;
}
