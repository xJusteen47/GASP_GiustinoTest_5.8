// Gameplay Ability System setup (GAS-NPP fork).

#pragma once

#include "CoreMinimal.h"
#include "GAS/GSTN_AbilityWallEntryTransition.h"
#include "WallMovement/GSTN_WallSurfaceModeTypes.h"
#include "GSTN_AbilityWallSurfaceModeTransition.generated.h"

/**
 * UGSTN_AbilityWallEntryTransition (the wall grab is a gameplay ability) whose wall mode is chosen by the surface, as UGSTN_WallSurfaceModeTransition does for
 * the plain transition (wall-slide-v2.md 6.3): a climb surface is grabbed by WallClimbingV2, a slide or unmarked one by WallSlidingV2. The ability check, the
 * wall detection and every rule are the parent's; only the mode differs. The one ability (UGSTN_GA_WallSlide) covers both modes (its WallModeNames list them).
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced, DisplayName = "GSTN Wall Entry (Gameplay Ability, by Surface)")
class GASP_GIUSTINO2_API UGSTN_AbilityWallSurfaceModeTransition : public UGSTN_AbilityWallEntryTransition
{
	GENERATED_BODY()

public:
	UGSTN_AbilityWallSurfaceModeTransition(const FObjectInitializer& ObjectInitializer);

	/** Which mode handles a surface, and what the entry refuses (see UGSTN_WallSurfaceModeTransition). Keep it equal to the modes' own list */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Surface Mode")
	FGSTN_WallSurfaceModeList SurfaceModes;

protected:
	virtual FName ResolveEntryMode(const FEntryCandidate& Candidate, const FSimulationTickParams& Params) const override;
};
