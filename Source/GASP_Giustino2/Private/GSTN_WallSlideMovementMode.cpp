// Fill out your copyright notice in the Description page of Project Settings.

#include "GSTN_WallSlideMovementMode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallSlideMovementMode)

UGSTN_WallSlideMovementMode::UGSTN_WallSlideMovementMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Defaults tuned on SandboxCharacter_Mover
	Horizontal.Friction = 3.f;
	Horizontal.BrakingDeceleration = 0.f;
	Skid.bEnableSkid = false;
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
	// No stick input on this axis: it only brakes (wall friction, never a stop snap that would block the slide), then gravity
	FGSTN_WallVerticalMovement Movement = Super::GetVerticalMovement();
	Movement.BrakingFriction = Slide.Friction;
	Movement.TerminalUpSpeed = Slide.TerminalUpSpeed;
	Movement.TerminalDownSpeed = Slide.TerminalDownSpeed;
	return Movement;
}

float UGSTN_WallSlideMovementMode::GetVerticalForce(float VerticalSpeed, float GravityAlongWall) const
{
	// GravityAlongWall is negative (pulls down the wall). Moving up = decelerating, moving down = accelerating
	const float Scale = (VerticalSpeed > 0.f) ? Slide.AscendingGravityScale : Slide.DescendingGravityScale;
	return GravityAlongWall * Scale;
}
