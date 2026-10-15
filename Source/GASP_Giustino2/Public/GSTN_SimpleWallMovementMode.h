// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_WallMovementModeBase.h"
#include "GSTN_SimpleWallMovementMode.generated.h"

/**
 * Free wall movement (climbing): the stick drives both the horizontal and the vertical axis of the wall, and the
 * character clings to it (no gravity along the wall). Works on overhangs and ceilings.
 */
UCLASS(Blueprintable, BlueprintType)
class GASP_GIUSTINO2_API UGSTN_SimpleWallMovementMode : public UGSTN_WallMovementModeBase
{
	GENERATED_BODY()

public:
	UGSTN_SimpleWallMovementMode(const FObjectInitializer& ObjectInitializer);

	virtual void PostLoad() override;

	/** Up/down along the wall (gravity projected on the wall), driven by the stick */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement")
	FGSTN_WallVerticalMovement Vertical;

	/** [Rotation] On ceilings (where "up" is undefined) turn the capsule up toward the movement direction, like a walking heading */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation")
	bool bAlignHeadingToMovementOnCeilings = true;

	/** [Rotation] How fast the heading follows the movement direction on ceilings (1/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Orientation", meta = (ClampMin = "0", EditCondition = "bAlignHeadingToMovementOnCeilings"))
	float CeilingHeadingSharpness = 5.f;

protected:
	virtual FGSTN_WallVerticalMovement GetVerticalMovement() const override { return Vertical; }
	virtual float GetCeilingHeadingSharpness() const override { return bAlignHeadingToMovementOnCeilings ? CeilingHeadingSharpness : 0.f; }

private:
	// ---- Legacy settings (before the horizontal/vertical split). Not editable: only loaded from old assets and migrated in PostLoad
	UPROPERTY()
	float MaxSpeed = 160.f;
	UPROPERTY()
	float Acceleration = 900.f;
	UPROPERTY()
	float Deceleration = 1400.f;
	UPROPERTY()
	float UpSpeedScale = 1.f;
	UPROPERTY()
	float DownSpeedScale = 1.f;
	UPROPERTY()
	float LateralSpeedScale = 1.f;
};
