// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WallMovement/GSTN_WallSurfaceModeTypes.h"
#include "WallMovement/Transitions/GSTN_WallEntryTransition.h"
#include "GSTN_WallSurfaceModeTransition.generated.h"

/**
 * The wall entry transition that lets the surface choose the wall mode (wall-slide-v2.md 6.3 / 6.4 W4b): the same detection, conditions and re-entry rules
 * as UGSTN_WallEntryTransition (it IS one), but the mode it enters is not a fixed TargetModeName. The sweep's hit is resolved to its surface
 * (UGSTN_SurfaceUtils::Resolve) and SurfaceModes, a priority list, names the mode: the first entry whose behaviours the surface offers and whose mode is
 * registered on the Mover component and not blocked. By default a surface offering WallClimb is grabbed by WallClimbingV2, else one offering WallSlide by
 * WallSlidingV2; a surface nothing marked is grabbed by the slide (the plugin's catch-all default would otherwise make every unauthored wall a climb
 * wall), and a surface that offers neither is not a wall at all. If the climb mode is not registered the list falls through to the slide, so adding the
 * transition to a project that only has the slide changes nothing.
 *
 * TargetModeName and SurfaceFilter of the base class are not used to pick the mode (SurfaceFilter is ignored: the list is the filter). While a character is
 * on the wall the modes themselves keep the list (UGSTN_WallTraverseModeBase::SurfaceModes): keep the two lists equal.
 *
 * Add it to the Falling mode's Transitions in place of UGSTN_WallEntryTransition (Tools/GAS/setup_wall_slide_modes.py --use v2climb). Where the entry is a
 * gameplay ability (UGSTN_AbilityWallEntryTransition), use UGSTN_AbilityWallSurfaceModeTransition instead: the same choice behind the ability check.
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class GASP_GIUSTINO2_API UGSTN_WallSurfaceModeTransition : public UGSTN_WallEntryTransition
{
	GENERATED_BODY()

public:
	UGSTN_WallSurfaceModeTransition(const FObjectInitializer& ObjectInitializer);

	/** Which mode handles a surface, and what the entry refuses. bEnabled is ignored here (an entry transition always chooses by surface) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Surface Mode")
	FGSTN_WallSurfaceModeList SurfaceModes;

	/**
	 * The mode this transition would enter for a hit: SurfaceModes resolved against the hit's surface, the registered modes of the Mover component and, when
	 * SyncState is given, its tags (the stamina hook). None = the surface is not a wall for any listed mode (or only blocked ones). The part of Evaluate
	 * that does not need a simulation tick
	 */
	FName ChooseModeForHit(const FHitResult& Hit, const FMoverSyncState* SyncState) const;

protected:
	/** The surface's mode (ChooseModeForHit on the hit that found the wall); None refuses the wall */
	virtual FName ResolveEntryMode(const FEntryCandidate& Candidate, const FSimulationTickParams& Params) const override;

	/** Empty: the list is the filter, whatever SurfaceFilter holds */
	virtual const FGSTN_WallSurfaceFilter& GetEntrySurfaceFilter() const override;
};
