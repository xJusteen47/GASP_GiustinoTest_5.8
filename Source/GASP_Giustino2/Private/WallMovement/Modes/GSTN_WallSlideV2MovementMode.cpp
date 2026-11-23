// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/Modes/GSTN_WallSlideV2MovementMode.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "WallMovement/GSTN_WallMovementDebug.h"
#include "WallMovement/MoveLibrary/GSTN_WallControlUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallSlideV2MovementMode)

UGSTN_WallSlideV2MovementMode::UGSTN_WallSlideV2MovementMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Same stick mapping, wall jump and surface limits as the legacy slide, which were tuned on SandboxCharacter_Mover
	InputMapping = EGSTN_WallInputMapping::CameraRelative;

	WallJump.bEnableWallJump = true;
	WallJump.AwaySpeed = 400.f;
	WallJump.UpSpeed = 300.f;

	// Accepted surfaces, as the angle between their normal and gravity-up: 45 (anything flatter is a floor) to 120. That covers every inclined wall
	// (a normal at 60, 70, 80: the wall leans back and the character slides down it), the vertical wall (90) and overhangs up to 30 degrees past it.
	// The entry transition's defaults are the same range
	MinSurfaceAngle = 45.f;
	MaxSurfaceAngle = 120.f;
	WallProbeDistance = 30.f;
	MaxNormalChangeRate = 600.f;

	// The look-ahead sees a recess as deep as the contact sweep reaches (wall-slide-v2.md 2.3: RecessDepth = WallProbeDistance), so a recess
	// is read as a recess and not as the end of the wall
	Edges.RecessDepth = WallProbeDistance;
}

float UGSTN_WallSlideV2MovementMode::GetEdgeReferenceSpeed() const
{
	return Control.TerminalDownSpeed;
}

FVector UGSTN_WallSlideV2MovementMode::ComputeEntryVelocity(const FVector& InVelocity, const FVector& WallNormal) const
{
	return UGSTN_WallControlUtils::ProjectEntryVelocity(Control, InVelocity, WallNormal);
}

FVector UGSTN_WallSlideV2MovementMode::MapMoveIntentToWall(const FCharacterDefaultInputs& Inputs, const FVector& MoveIntent, const FGSTN_WallMovementPlane& Plane) const
{
	const FVector Mapped = Super::MapMoveIntentToWall(Inputs, MoveIntent, Plane);
	FVector HAxis, VAxis;
	UGSTN_WallControlUtils::MakePlaneAxes(Plane.Normal, Plane.GravityUp, HAxis, VAxis);
	const FVector2D OnPlane = UGSTN_WallControlUtils::ToPlane(Mapped, HAxis, VAxis);
	const FVector2D Kept = UGSTN_WallControlUtils::SuppressVerticalIntoWall(OnPlane, UGSTN_WallControlUtils::IntoWallAmount(MoveIntent, Plane.Normal, Plane.GravityUp), MoveIntent.Size());
	return UGSTN_WallControlUtils::FromPlane(Kept, HAxis, VAxis);
}

FVector UGSTN_WallSlideV2MovementMode::ComputeWallVelocity(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent, float DeltaSeconds) const
{
	FVector HAxis, VAxis;
	UGSTN_WallControlUtils::MakePlaneAxes(Plane.Normal, Plane.GravityUp, HAxis, VAxis);

	const FVector2D PriorOnPlane = UGSTN_WallControlUtils::ToPlane(GetPriorVelocityOnPlane(StartState, Plane), HAxis, VAxis);
	const FVector2D InputOnPlane = UGSTN_WallControlUtils::ToPlane(WallIntent, HAxis, VAxis);

	FGSTN_WallSlideControlSettings Settings = Control;
	bool bBrakeTag = false;
	if (const UMoverComponent* MoverComp = GetMoverComponent())
	{
		// Gravity along the wall (V is up on the plane): an inclined wall pulls less than a vertical one, in proportion, and its terminal speed
		// follows (SlopeTerminalScaling). Down the wall is the gravity tangent of the proxy plane, never world Z
		Settings = UGSTN_WallControlUtils::AdaptToSlope(Control, MoverComp->GetGravityAcceleration(), VAxis);
		// Mover tags of the simulated state, not the game thread's: a re-simulated frame reads its own
		bBrakeTag = Brake.BrakeInputTag.IsValid() && MoverComp->HasGameplayTagInState(StartState.SyncState, Brake.BrakeInputTag, false);
	}

	const bool bBrake = UGSTN_WallControlUtils::ResolveBrake(Brake, PriorOnPlane, InputOnPlane, bBrakeTag);
	// The option to slow the slide while pushing into the wall; the stick is only read for it when it is on
	const float IntoWall = Settings.HoldTowardWallTerminalScale < 1.f
		? UGSTN_WallControlUtils::IntoWallAmount(GetWorldMoveIntent(StartState, Plane.GravityUp), Plane.Normal, Plane.GravityUp) : 0.f;
	const FVector2D NewOnPlane = UGSTN_WallControlUtils::StepSlideVelocity(Settings, PriorOnPlane, InputOnPlane, bBrake, DeltaSeconds, IntoWall);

	GSTN_WALL_LOG(2, TEXT("GEN slide v2 prior=(h %.1f v %.1f) in=(%.2f %.2f) brake=%d out=(h %.1f v %.1f)"),
		PriorOnPlane.X, PriorOnPlane.Y, InputOnPlane.X, InputOnPlane.Y, bBrake ? 1 : 0, NewOnPlane.X, NewOnPlane.Y);
	return UGSTN_WallControlUtils::FromPlane(NewOnPlane, HAxis, VAxis);
}
