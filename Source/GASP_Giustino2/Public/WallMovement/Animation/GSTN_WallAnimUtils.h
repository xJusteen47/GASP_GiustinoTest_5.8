// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WallMovement/Animation/GSTN_WallAnimTypes.h"
#include "GSTN_WallAnimUtils.generated.h"

class UMoverComponent;

/**
 * Stateless maths of the wall anim state (wall-anim.md): splits a velocity into its parts on the wall, classifies the loop state with
 * hysteresis and steps the Enter / Active / Exit phases. The state is passed in and returned, so all of it is testable without a world
 * (GSTN.Wall.Anim.*). Presentation only: never call it from simulation code.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_WallAnimUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Splits Velocity on the wall: OutLateral along the wall horizontally (positive = right of a character facing the wall, i.e. facing
	 * -WallNormal), OutVertical along the gravity tangent of the wall (positive = up), OutPlaneSpeed the length on the plane.
	 * A wall normal (nearly) parallel to GravityUp (a floor or a ceiling) has no horizontal or vertical axis: both are 0, only OutPlaneSpeed is set
	 */
	UFUNCTION(BlueprintPure, Category = "Wall|Animation")
	static void SplitWallVelocity(const FVector& Velocity, const FVector& WallNormal, const FVector& GravityUp, float& OutLateral, float& OutVertical, float& OutPlaneSpeed);

	/**
	 * Loop state for this frame. Skid wins (PlaneSpeed >= SkidEnterSpeed, kept down to SkidExitSpeed; never while climbing unless
	 * bSkidWhileClimbing), then left / right (|Lateral| >= LateralEnterSpeed, kept down to LateralExitSpeed while the sign is the same),
	 * else Idle. Previous is last frame's state, for the hysteresis
	 */
	UFUNCTION(BlueprintPure, Category = "Wall|Animation")
	static EGSTN_WallSlideAnimState ClassifySlide(EGSTN_WallSlideAnimState Previous, float Lateral, float PlaneSpeed, bool bIsClimbing, const FGSTN_WallAnimSettings& Settings);

	/**
	 * One frame of the wall anim state. Phases: None -> Enter on grabbing a wall, Enter -> Active after EnterDuration, Active/Enter -> Exit on
	 * leaving it, Exit -> None after ExitDuration, Exit -> Enter on grabbing again. Off the wall the loop state and the speeds keep their last
	 * on-wall values, so the exit can read them
	 */
	UFUNCTION(BlueprintPure, Category = "Wall|Animation")
	static FGSTN_WallAnimState StepWallAnimState(const FGSTN_WallAnimState& Previous, const FGSTN_WallAnimInputs& Inputs, const FGSTN_WallAnimSettings& Settings, float DeltaSeconds);

	/**
	 * Gathers the inputs from a Mover component: active mode (class test against Settings.WallModeClass), presentation velocity, the wall
	 * sync state's filtered normal and gravity-up. Game thread only. A null component gives "not on a wall"
	 */
	static FGSTN_WallAnimInputs GatherInputs(const UMoverComponent* MoverComponent, const FGSTN_WallAnimSettings& Settings);
};
