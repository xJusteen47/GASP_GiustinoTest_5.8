// Fill out your copyright notice in the Description page of Project Settings.

#include "GSTN_WallSlideMovementMode.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallSlideMovementMode)

UGSTN_WallSlideMovementMode::UGSTN_WallSlideMovementMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Vertical motion is gravity only: no climbing input, no braking that would fight gravity
	Vertical.MaxUpSpeed = 0.f;
	Vertical.MaxDownSpeed = 0.f;
	Vertical.BrakingFriction = 0.f;
	Vertical.BrakingDeceleration = 0.f;
	Vertical.StopSpeed = 0.f;
	Vertical.TerminalDownSpeed = 400.f;

	Detach.bDetachByPullingAway = true;
}

float UGSTN_WallSlideMovementMode::GetVerticalForce(float VerticalSpeed, float GravityAlongWall) const
{
	// GravityAlongWall is negative (pulls down the wall). Moving up = decelerating, moving down = accelerating
	const float Scale = (VerticalSpeed > 0.f) ? AscendingGravityScale : DescendingGravityScale;
	return GravityAlongWall * Scale;
}
