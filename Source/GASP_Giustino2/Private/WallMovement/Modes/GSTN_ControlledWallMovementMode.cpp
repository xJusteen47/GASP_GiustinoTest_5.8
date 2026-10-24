// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/Modes/GSTN_ControlledWallMovementMode.h"

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "WallMovement/GSTN_WallMovementDebug.h"
#include "WallMovement/MoveLibrary/GSTN_WallMovementUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_ControlledWallMovementMode)

UGSTN_ControlledWallMovementMode::UGSTN_ControlledWallMovementMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Detach.DetachModeName = DefaultModeNames::Falling;
	WallJump.JumpModeName = DefaultModeNames::Falling;
}

// ------------------------------------------------------------------------------------------------
// Input mapping
// ------------------------------------------------------------------------------------------------

FVector UGSTN_ControlledWallMovementMode::MapMoveIntentToWall(const FCharacterDefaultInputs& Inputs, const FVector& MoveIntent, const FGSTN_WallMovementPlane& Plane) const
{
	// Recover the stick input (camera space) from the world input. The world input is the stick rotated by the camera yaw:
	// interpreting it in world space would make the same key mean a different wall direction as the capsule turns around corners
	const FQuat ControlQuat = Inputs.ControlRotation.Quaternion();
	const FVector CamForwardFlat = FVector::VectorPlaneProject(ControlQuat.GetForwardVector(), Plane.GravityUp).GetSafeNormal();
	if (CamForwardFlat.IsNearlyZero())
	{
		// Degenerate camera: plain projection onto the wall plane
		return Super::MapMoveIntentToWall(Inputs, MoveIntent, Plane);
	}
	const FVector CamRightFlat = FVector::CrossProduct(Plane.GravityUp, CamForwardFlat);
	const float StickRight = FVector::DotProduct(MoveIntent, CamRightFlat);
	const float StickForward = FVector::DotProduct(MoveIntent, CamForwardFlat);

	FVector TangentUp, TangentRight;
	UGSTN_WallMovementUtils::ComputeWallTangentBasis(Plane.Orientation, Plane.Normal, TangentUp, TangentRight);

	if (InputMapping == EGSTN_WallInputMapping::CameraRelative)
	{
		// "Screen up" on the wall: Forward+Up of the camera projected on the plane. Walls in front -> camera up, ceilings -> camera forward
		FVector ScreenUp = FVector::VectorPlaneProject(ControlQuat.GetForwardVector() + ControlQuat.GetUpVector(), Plane.Normal).GetSafeNormal();
		if (ScreenUp.IsNearlyZero())
		{
			ScreenUp = TangentUp;
		}
		const FVector ScreenRight = FVector::CrossProduct(Plane.Normal, ScreenUp);

		return (ScreenRight * StickRight + ScreenUp * StickForward).GetClampedToMaxSize(1.f);
	}

	// SurfaceRelative: the stick drives the capsule's own frame on the wall (right = sideways, forward = up the wall).
	// Independent of where the capsule faces in the world, so the same key keeps the same meaning around every corner
	return (TangentRight * StickRight + TangentUp * StickForward).GetClampedToMaxSize(1.f);
}

// ------------------------------------------------------------------------------------------------
// Leaving on purpose
// ------------------------------------------------------------------------------------------------

void UGSTN_ControlledWallMovementMode::CheckForWallExit(const FSimulationTickParams& Params, FGSTN_WallTickState& InOutState) const
{
	const FCharacterDefaultInputs* Inputs = Params.StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;

	if (TryWallJump(Inputs, DeltaSeconds, InOutState) || TryDetachByButton(Params.StartState, DeltaSeconds, InOutState))
	{
		return;
	}

	if (Detach.bDetachByPullingAway && !Detach.DetachModeName.IsNone() && Inputs)
	{
		TryDetachByPullingAway(*Inputs, DeltaSeconds, InOutState);
		return;
	}

	InOutState.PullAwayTime = 0.f;
}

bool UGSTN_ControlledWallMovementMode::TryWallJump(const FCharacterDefaultInputs* Inputs, float DeltaSeconds, FGSTN_WallTickState& InOutState) const
{
	bool bJumpPressed = Inputs && Inputs->bIsJumpJustPressed;
#if GSTN_WALL_DEBUG
	bJumpPressed |= GSTNWallDebug::ConsumeJumpRequest();
#endif
	if (!WallJump.bEnableWallJump || WallJump.JumpModeName.IsNone() || !bJumpPressed)
	{
		return false;
	}

	const FGSTN_WallMovementPlane Plane = UGSTN_WallMovementUtils::MakeMovementPlane(InOutState.Orientation, InOutState.WallNormal, InOutState.GravityUp);
	InOutState.Velocity = UGSTN_WallMovementUtils::ComputeWallJumpVelocity(InOutState.Velocity, Plane, WallJump);
	InOutState.LeaveTo(WallJump.JumpModeName, DeltaSeconds);
	GSTN_WALL_LOG(1, TEXT("WALL JUMP vel=%s"), *GSTNWallDebug::Vec(InOutState.Velocity));
	return true;
}

bool UGSTN_ControlledWallMovementMode::TryDetachByButton(const FMoverTickStartData& StartState, float DeltaSeconds, FGSTN_WallTickState& InOutState) const
{
	if (!Detach.bDetachByButton || Detach.DetachModeName.IsNone() || !IsDetachButtonPressed(StartState))
	{
		return false;
	}

	InOutState.LeaveTo(Detach.DetachModeName, DeltaSeconds);
	GSTN_WALL_LOG(1, TEXT("DETACH (button)"));
	return true;
}

bool UGSTN_ControlledWallMovementMode::TryDetachByPullingAway(const FCharacterDefaultInputs& Inputs, float DeltaSeconds, FGSTN_WallTickState& InOutState) const
{
	FVector MoveInput = Inputs.GetMoveInput_WorldSpace();
#if GSTN_WALL_DEBUG
	GSTNWallDebug::OverrideMoveInput(Inputs.ControlRotation, InOutState.GravityUp, MoveInput);
#endif
	const bool bPullingAway = UGSTN_WallMovementUtils::IsPullingAwayFromWall(MoveInput, InOutState.WallNormal, InOutState.GravityUp, Detach.PullAwayInputDot);

	InOutState.PullAwayTime = bPullingAway ? InOutState.PullAwayTime + DeltaSeconds : 0.f;
	if (!bPullingAway || InOutState.PullAwayTime < Detach.PullAwayDelay)
	{
		return false;
	}

	InOutState.LeaveTo(Detach.DetachModeName, DeltaSeconds);
	GSTN_WALL_LOG(1, TEXT("DETACH (pulled away %.2fs)"), InOutState.PullAwayTime);
	return true;
}

bool UGSTN_ControlledWallMovementMode::IsDetachButtonPressed(const FMoverTickStartData& StartState) const
{
#if GSTN_WALL_DEBUG
	if (GSTNWallDebug::IsDetachHeld())
	{
		return true;
	}
#endif
	if (const FGSTN_WallInputs* WallInputs = StartState.InputCmd.InputCollection.FindDataByType<FGSTN_WallInputs>())
	{
		return WallInputs->bWantsToDetach;
	}

	// Fallback: the CharacterMover crouch intent. Read from the component, so it is not part of the predicted input
	if (Detach.bUseCrouchAsDetachButton)
	{
		if (const UCharacterMoverComponent* CharacterMover = Cast<UCharacterMoverComponent>(GetMoverComponent()))
		{
			return CharacterMover->GetCrouchIntent();
		}
	}
	return false;
}
