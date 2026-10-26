// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/Modes/GSTN_WallClimbV2MovementMode.h"

#include "GSTN_SurfaceTags.h"
#include "WallMovement/GSTN_WallMovementDebug.h"
#include "WallMovement/MoveLibrary/GSTN_WallControlUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallClimbV2MovementMode)

UGSTN_WallClimbV2MovementMode::UGSTN_WallClimbV2MovementMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Same stick mapping, wall jump and surface limits as the slide v2, so the two feel like one character on the wall. Unlike the slide, "stick forward" is
	// meant to go up the wall here (camera relative: the camera's forward and up projected on the plane), so the slide's into-wall suppression is not applied
	InputMapping = EGSTN_WallInputMapping::CameraRelative;

	WallJump.bEnableWallJump = true;
	WallJump.AwaySpeed = 400.f;
	WallJump.UpSpeed = 300.f;

	MinSurfaceAngle = 45.f;
	MaxSurfaceAngle = 120.f;
	WallProbeDistance = 30.f;
	MaxNormalChangeRate = 600.f;
	Edges.RecessDepth = WallProbeDistance;

	// Climbing is authored: only surfaces offering WallClimb, and unmarked level geometry is not one (the slide keeps that role). This is the stand-alone
	// filter; with the surface mode list enabled (default) the probes accept what the listed modes take over, see UGSTN_WallTraverseModeBase
	if (GSTNSurfaceTags::Behaviour_WallClimb.GetTag().IsValid())
	{
		SurfaceFilter.RequiredBehaviours = FGameplayTagQuery::MakeQuery_MatchTag(GSTNSurfaceTags::Behaviour_WallClimb);
	}
	SurfaceFilter.bUnknownSurfacesPass = false;
}

float UGSTN_WallClimbV2MovementMode::GetEdgeReferenceSpeed() const
{
	return Control.ClimbSpeed;
}

FVector UGSTN_WallClimbV2MovementMode::ComputeEntryVelocity(const FVector& InVelocity, const FVector& WallNormal) const
{
	FGSTN_WallSlideControlSettings EntrySettings;
	EntrySettings.EntrySpeedKeep = Control.EntrySpeedKeep;
	EntrySettings.MaxEntrySpeed = Control.MaxEntrySpeed;
	return UGSTN_WallControlUtils::ProjectEntryVelocity(EntrySettings, InVelocity, WallNormal);
}

FVector UGSTN_WallClimbV2MovementMode::ComputeWallVelocity(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent, float DeltaSeconds) const
{
	FVector HAxis, VAxis;
	UGSTN_WallControlUtils::MakePlaneAxes(Plane.Normal, Plane.GravityUp, HAxis, VAxis);

	const FVector2D PriorOnPlane = UGSTN_WallControlUtils::ToPlane(GetPriorVelocityOnPlane(StartState, Plane), HAxis, VAxis);
	const FVector2D InputOnPlane = UGSTN_WallControlUtils::ToPlane(WallIntent, HAxis, VAxis);
	const FVector2D NewOnPlane = UGSTN_WallControlUtils::StepClimbVelocity(Control, PriorOnPlane, InputOnPlane, DeltaSeconds);

	GSTN_WALL_LOG(2, TEXT("GEN climb v2 prior=(h %.1f v %.1f) in=(%.2f %.2f) out=(h %.1f v %.1f)"),
		PriorOnPlane.X, PriorOnPlane.Y, InputOnPlane.X, InputOnPlane.Y, NewOnPlane.X, NewOnPlane.Y);
	return UGSTN_WallControlUtils::FromPlane(NewOnPlane, HAxis, VAxis);
}
