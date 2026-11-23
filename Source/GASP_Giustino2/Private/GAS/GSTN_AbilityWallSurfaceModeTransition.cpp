// Gameplay Ability System setup (GAS-NPP fork).

#include "GAS/GSTN_AbilityWallSurfaceModeTransition.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "WallMovement/GSTN_WallMovementDebug.h"
#include "WallMovement/MoveLibrary/GSTN_WallSurfaceModeUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AbilityWallSurfaceModeTransition)

UGSTN_AbilityWallSurfaceModeTransition::UGSTN_AbilityWallSurfaceModeTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SurfaceModes = UGSTN_WallSurfaceModeUtils::MakeDefaultList();
	TargetModeName = UGSTN_WallSurfaceModeUtils::SlideModeName;
	// Traversal T2: the re-entry rules that hold on moving walls and after a correction (UGSTN_WallReentryUtils)
	bInferModeChangeLostToRollback = true;
	bBaseRelativeHeightCap = true;
}

FName UGSTN_AbilityWallSurfaceModeTransition::ResolveEntryMode(const FEntryCandidate& Candidate, const FSimulationTickParams& Params) const
{
	const FName ModeName = UGSTN_WallSurfaceModeUtils::ChooseModeForHit(SurfaceModes, Candidate.Hit, GetMoverComponent(), &Params.StartState.SyncState);
	if (ModeName.IsNone())
	{
		GSTN_WALL_LOG(2, TEXT("ENTRY rejected: no registered, unblocked wall mode takes this surface"));
	}
	return ModeName;
}

const FGSTN_WallSurfaceFilter& UGSTN_AbilityWallSurfaceModeTransition::GetEntrySurfaceFilter() const
{
	static const FGSTN_WallSurfaceFilter NoFilter;
	return NoFilter;
}
