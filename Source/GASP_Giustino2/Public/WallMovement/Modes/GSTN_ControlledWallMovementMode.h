// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WallMovement/Modes/GSTN_WallMovementModeBase.h"
#include "GSTN_ControlledWallMovementMode.generated.h"

/**
 * Player-controlled wall movement: adds to the essential wall movement
 *  - the stick mapping on the wall (InputMapping): the stick keeps its meaning around corners, see EGSTN_WallInputMapping
 *  - leaving the wall on purpose: wall jump, detach button, stick held away from the wall
 * Abstract: the vertical axis is still supplied by the subclasses.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class GASP_GIUSTINO2_API UGSTN_ControlledWallMovementMode : public UGSTN_WallMovementModeBase
{
	GENERATED_BODY()

public:
	UGSTN_ControlledWallMovementMode(const FObjectInitializer& ObjectInitializer);

	/** How the stick is turned into a direction on the wall. See EGSTN_WallInputMapping */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement")
	EGSTN_WallInputMapping InputMapping = EGSTN_WallInputMapping::SurfaceRelative;

	/** Ways to let go of the wall on purpose */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Entry and Exit")
	FGSTN_WallDetach Detach;

	/** Provisional wall jump */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Entry and Exit")
	FGSTN_WallJump WallJump;

protected:
	/** The stick (recovered from the camera-relative world input) drives the wall frame chosen by InputMapping */
	virtual FVector MapMoveIntentToWall(const FCharacterDefaultInputs& Inputs, const FVector& MoveIntent, const FGSTN_WallMovementPlane& Plane) const override;

	/** Wall jump, then detach by button, then detach by pulling away: the first that triggers wins */
	virtual void CheckForWallExit(const FSimulationTickParams& Params, FGSTN_WallTickState& InOutState) const override;

	/** Jump pressed: launch away from the wall into WallJump.JumpModeName */
	bool TryWallJump(const FCharacterDefaultInputs* Inputs, float DeltaSeconds, FGSTN_WallTickState& InOutState) const;

	/** Detach button held: let go into Detach.DetachModeName */
	bool TryDetachByButton(const FMoverTickStartData& StartState, float DeltaSeconds, FGSTN_WallTickState& InOutState) const;

	/** Times the stick held away from the wall, and lets go once it reaches Detach.PullAwayDelay */
	bool TryDetachByPullingAway(const FCharacterDefaultInputs& Inputs, float DeltaSeconds, FGSTN_WallTickState& InOutState) const;

	/** Whether the detach button is held (FGSTN_WallInputs, or the CharacterMover crouch intent as a fallback) */
	bool IsDetachButtonPressed(const FMoverTickStartData& StartState) const;
};
