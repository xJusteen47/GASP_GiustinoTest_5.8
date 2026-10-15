// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GSTN_WallMovementModeBase.h"
#include "GSTN_WallSlideMovementMode.generated.h"

/** Vertical motion of a wall slide: no stick input, only gravity along the wall, wall friction and speed limits */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallSlideVertical
{
	GENERATED_BODY()

	/** Gravity multiplier while moving up the wall (1 = full gravity along the wall, higher = upward momentum dies faster) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0"))
	float AscendingGravityScale = 1.f;

	/** Gravity multiplier while sliding down (low = slow slide) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0"))
	float DescendingGravityScale = 0.3f;

	/** Wall friction, proportional to the speed (1/s): drag against both the slide and the upward momentum. 0 = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0"))
	float Friction = 0.f;

	/** Absolute upward speed limit, whatever the source (entry momentum, bases). 0 = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float TerminalUpSpeed = 0.f;

	/** Max slide speed. 0 = none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall", meta = (ClampMin = "0", ForceUnits = "cm/s"))
	float TerminalDownSpeed = 400.f;
};

/**
 * Wall slide: wall movement where gravity keeps acting along the wall. The upward momentum the character arrives with
 * (e.g. grabbing the wall mid-jump) is eaten by Slide.AscendingGravityScale, then it slides down with
 * Slide.DescendingGravityScale, capped by Slide.TerminalDownSpeed. Gravity is projected on the wall, so slanted walls
 * slide slower and any gravity direction works. The stick only moves the character sideways.
 */
UCLASS(Blueprintable, BlueprintType)
class GASP_GIUSTINO2_API UGSTN_WallSlideMovementMode : public UGSTN_WallMovementModeBase
{
	GENERATED_BODY()

public:
	UGSTN_WallSlideMovementMode(const FObjectInitializer& ObjectInitializer);

	/** Up/down along the wall: gravity, wall friction and speed limits */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Movement")
	FGSTN_WallSlideVertical Slide;

protected:
	virtual FGSTN_WallVerticalMovement GetVerticalMovement() const override;
	virtual float GetVerticalForce(float VerticalSpeed, float GravityAlongWall) const override;
};
