// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WallMovement/Modes/GSTN_ControlledWallMovementMode.h"
#include "GSTN_SkiddingWallMovementMode.generated.h"

/**
 * Controlled wall movement with a skid: pushing the stick against a fast horizontal movement brakes hard
 * (Skid.Deceleration) instead of accelerating toward the input, then the character turns around.
 * The skid is decided from the start state of the move, so GenerateMove and SimulationTick agree on it; the result is
 * published in FGSTN_WallMovementSyncState::bIsSkidding for animation.
 * Abstract: the vertical axis is still supplied by the subclasses.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class GASP_GIUSTINO2_API UGSTN_SkiddingWallMovementMode : public UGSTN_ControlledWallMovementMode
{
	GENERATED_BODY()

public:
	virtual void SimulationTick_Implementation(const FSimulationTickParams& Params, FMoverTickEndData& OutputState) override;

	/** Hard brake when pushing against a fast horizontal movement */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement")
	FGSTN_WallSkid Skid;

protected:
	/** Skidding: constant deceleration toward zero. Otherwise the base horizontal model */
	virtual float ComputeHorizontalSpeed(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent,
		float HorizontalSpeed, float DeltaSeconds) const override;

	/** Whether the move starting from StartState is a skid (deterministic: used by both GenerateMove and SimulationTick) */
	bool IsSkidding(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent, float HorizontalSpeed) const;
};
