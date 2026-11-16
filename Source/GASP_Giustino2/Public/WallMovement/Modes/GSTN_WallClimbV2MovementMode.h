// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WallMovement/GSTN_WallControlTypes.h"
#include "WallMovement/Modes/GSTN_WallTraverseModeBase.h"
#include "GSTN_WallClimbV2MovementMode.generated.h"

/**
 * Wall climb v2 (wall-slide-v2.md 6.3): the other mode on the shared core. It has the same pipeline as the slide (upright capsule, proxy plane, one
 * move sweep, adhesion, corner and bump handling; see UGSTN_WallTraverseModeBase) and only supplies the control model: ComputeWallVelocity is
 * UGSTN_WallControlUtils::StepClimbVelocity, full 2D control of the stick on the proxy plane (FGSTN_WallClimbControlSettings: ClimbSpeed, ClimbAccel,
 * ClimbBrake). There is no gravity slide while holding: with the stick released the target is zero and the strong brake stops the character, who then
 * hangs still on the wall.
 *
 * Where it runs: only on surfaces that offer Surface.Behaviour.WallClimb. As a stand-alone mode that is its SurfaceFilter (default WallClimb, and
 * unmarked surfaces are NOT climb walls: climbing is something a level designer authors). Registered next to the slide with the surface mode list enabled
 * (the default), the filter is widened to every surface the listed modes take over and the surface decides: the entry transition
 * (UGSTN_WallSurfaceModeTransition) grabs a climb surface with this mode, and a character that crosses from a climb-only surface onto a slide-only one is
 * handed to the slide in place with its velocity (and back), see UGSTN_WallTraverseModeBase.
 *
 * Stamina is data, not code: an entry of the surface mode list can carry a BlockedWhen tag query (FGSTN_WallSurfaceModeEntry), matched against the Mover tags
 * of the tick's start state. A blocked climb is skipped by the choice, so an exhausted character on a wall that also offers sliding becomes a slide and on
 * a climb-only wall goes to SurfaceModes.BlockedModeName. The mode has no GAS dependency; an ability grants the tag.
 *
 * Declared but not implemented: bAllowCapsuleTilt (overhangs, off). The capsule never rotates on a wall in v2 (the rule that removes the snagging); a tilt
 * opt-in for climbing under overhangs is a later extension of the base, and this flag is where a project will turn it on. Today it changes nothing.
 *
 * Register it as "WallClimbingV2" on the Mover component (Tools/GAS/setup_wall_slide_modes.py --use v2climb).
 */
UCLASS(Blueprintable, BlueprintType)
class GASP_GIUSTINO2_API UGSTN_WallClimbV2MovementMode : public UGSTN_WallTraverseModeBase
{
	GENERATED_BODY()

	friend struct FGSTN_WallTraverseTestAccess;

public:
	UGSTN_WallClimbV2MovementMode(const FObjectInitializer& ObjectInitializer);

	/** Speed, acceleration and braking of the stick on the wall. See FGSTN_WallClimbControlSettings */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control")
	FGSTN_WallClimbControlSettings Control;

	/**
	 * Opt-in to tilt the capsule under overhangs. Declared for the climb extension of the base; NOT implemented, the capsule stays upright whatever
	 * the value (UGSTN_WallTraverseModeBase rule 1)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Control")
	bool bAllowCapsuleTilt = false;

	/** Arrival velocity turned onto the plane keeping its length times Control.EntrySpeedKeep, clamped to Control.MaxEntrySpeed */
	virtual FVector ComputeEntryVelocity(const FVector& InVelocity, const FVector& WallNormal) const override;

protected:
	/** The corner caps are fractions of Control.ClimbSpeed */
	virtual float GetEdgeReferenceSpeed() const override;

	/** StepClimbVelocity on the plane (H = horizontal along the wall, V = along the gravity tangent, up positive): the stick, with every direction its own */
	virtual FVector ComputeWallVelocity(const FMoverTickStartData& StartState, const FGSTN_WallMovementPlane& Plane, const FVector& WallIntent, float DeltaSeconds) const override;
};
