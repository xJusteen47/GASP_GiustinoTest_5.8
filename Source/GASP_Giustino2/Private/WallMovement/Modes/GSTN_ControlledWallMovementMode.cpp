// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/Modes/GSTN_ControlledWallMovementMode.h"
#include "Movement/GSTN_MoverButtonInputs.h"

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

	if (TryWallJump(Params.StartState, DeltaSeconds, InOutState) || TryDetachByButton(Params.StartState, DeltaSeconds, InOutState))
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

bool UGSTN_ControlledWallMovementMode::TryWallJump(const FMoverTickStartData& StartState, float DeltaSeconds, FGSTN_WallTickState& InOutState) const
{
	// Two carriers of the jump press, both in the predicted, replayed Mover command: the combat model's logical Jump button
	// (FGSTN_MoverButtonInputs, layout function "Wall.Jump", also gstn.Wall.Jump) and GASP's own jump intent. Either one jumps: reading
	// only the first lost the wall jump of pawns whose combat input config does not bind GASP's jump action (desktop check 2026-11-27)
	bool bJumpPressed = false;
	if (const FGSTN_MoverButtonInputs* Buttons = StartState.InputCmd.InputCollection.FindDataByType<FGSTN_MoverButtonInputs>())
	{
		bJumpPressed = Buttons->Has(FGSTN_MoverButtonInputs::JumpPressed);
	}
	if (const FCharacterDefaultInputs* Inputs = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>())
	{
		bJumpPressed |= Inputs->bIsJumpJustPressed;
	}
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
	if (const FGSTN_WallInputs* WallInputs = StartState.InputCmd.InputCollection.FindDataByType<FGSTN_WallInputs>(); WallInputs && WallInputs->bWantsToDetach)
	{
		return true;
	}

	// The crouch as detach button, read from the command (FGSTN_MoverButtonInputs, layout function "Wall.Detach"). The CharacterMover crouch
	// intent read from the component was not part of the predicted input (a replay or the server could disagree): kept only for a pawn
	// without the combat model, which has no command entry to read
	const FGSTN_MoverButtonInputs* Buttons = StartState.InputCmd.InputCollection.FindDataByType<FGSTN_MoverButtonInputs>();
	if (Buttons && Buttons->Has(FGSTN_MoverButtonInputs::DetachForced))
	{
		// gstn.Wall.Detach: detaches regardless of the crouch setting, as the old debug global did
		return true;
	}
	if (Detach.bUseCrouchAsDetachButton)
	{
		if (Buttons)
		{
			return Buttons->Has(FGSTN_MoverButtonInputs::DetachHeld);
		}
		if (const UCharacterMoverComponent* CharacterMover = Cast<UCharacterMoverComponent>(GetMoverComponent()))
		{
			return CharacterMover->GetCrouchIntent();
		}
	}
	return false;
}
