// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_SimpleWallMovementMode.h"
#include "GSTN_WallSlideMovementMode.generated.h"

/**
 * Wall slide: wall movement where gravity keeps acting along the wall. The upward momentum the character arrives with
 * (e.g. grabbing the wall mid-jump) is eaten by AscendingGravityScale, then it slides down with DescendingGravityScale,
 * capped by Vertical.TerminalDownSpeed. Gravity is projected on the wall, so slanted walls slide slower and any gravity
 * direction works.
 *
 * Defaults: sideways movement on, no climbing input (vertical motion is gravity only), letting go by pulling the stick
 * away from the wall. Everything is inherited and can be changed per asset.
 */
UCLASS(Blueprintable, BlueprintType)
class GASP_GIUSTINO2_API UGSTN_WallSlideMovementMode : public UGSTN_SimpleWallMovementMode
{
	GENERATED_BODY()

public:
	UGSTN_WallSlideMovementMode(const FObjectInitializer& ObjectInitializer);

	/** Gravity multiplier while moving up the wall (1 = full gravity along the wall, higher = upward momentum dies faster) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Slide", meta = (ClampMin = "0"))
	float AscendingGravityScale = 1.f;

	/** Gravity multiplier while sliding down (low = slow slide). The slide speed is capped by Vertical.TerminalDownSpeed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Slide", meta = (ClampMin = "0"))
	float DescendingGravityScale = 0.3f;

protected:
	virtual float GetVerticalForce(float VerticalSpeed, float GravityAlongWall) const override;
};
