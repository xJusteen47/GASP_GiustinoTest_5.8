// Gameplay Ability System setup (GAS-NPP fork).

#include "GAS/GSTN_AbilityWallSurfaceModeTransition.h"

#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "WallMovement/MoveLibrary/GSTN_WallSurfaceModeUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GSTN_AbilityWallSurfaceModeTransition)

UGSTN_AbilityWallSurfaceModeTransition::UGSTN_AbilityWallSurfaceModeTransition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SurfaceModes = UGSTN_WallSurfaceModeUtils::MakeDefaultList();
	TargetModeName = UGSTN_WallSurfaceModeUtils::SlideModeName;
}

FName UGSTN_AbilityWallSurfaceModeTransition::ResolveEntryMode(const FEntryCandidate& Candidate, const FSimulationTickParams& Params) const
{
	return UGSTN_WallSurfaceModeUtils::ChooseModeForHit(SurfaceModes, Candidate.Hit, GetMoverComponent(), &Params.StartState.SyncState);
}
