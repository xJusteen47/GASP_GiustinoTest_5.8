// Fill out your copyright notice in the Description page of Project Settings.

#include "WallMovement/Transitions/GSTN_WallSurfaceModeTransition.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "WallMovement/GSTN_WallMovementDebug.h"
#include "WallMovement/MoveLibrary/GSTN_WallSurfaceModeUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_WallSurfaceModeTransition)

UGSTN_WallSurfaceModeTransition::UGSTN_WallSurfaceModeTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SurfaceModes = UGSTN_WallSurfaceModeUtils::MakeDefaultList();
	// The base class refuses an empty target; the real one is chosen per surface
	TargetModeName = UGSTN_WallSurfaceModeUtils::SlideModeName;
	// Traversal T2: the re-entry rules that hold on moving walls and after a correction (UGSTN_WallReentryUtils)
	bInferModeChangeLostToRollback = true;
	bBaseRelativeHeightCap = true;
}

FName UGSTN_WallSurfaceModeTransition::ChooseModeForHit(const FHitResult& Hit, const FMoverSyncState* SyncState) const
{
	return UGSTN_WallSurfaceModeUtils::ChooseModeForHit(SurfaceModes, Hit, GetMoverComponent(), SyncState);
}

FName UGSTN_WallSurfaceModeTransition::ResolveEntryMode(const FEntryCandidate& Candidate, const FSimulationTickParams& Params) const
{
	const FName ModeName = ChooseModeForHit(Candidate.Hit, &Params.StartState.SyncState);
	if (ModeName.IsNone())
	{
		GSTN_WALL_LOG(2, TEXT("ENTRY rejected: no registered, unblocked wall mode takes this surface"));
	}
	return ModeName;
}

const FGSTN_WallSurfaceFilter& UGSTN_WallSurfaceModeTransition::GetEntrySurfaceFilter() const
{
	static const FGSTN_WallSurfaceFilter NoFilter;
	return NoFilter;
}
