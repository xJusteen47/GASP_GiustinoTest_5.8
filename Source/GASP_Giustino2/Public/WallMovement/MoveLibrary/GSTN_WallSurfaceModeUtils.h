// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WallMovement/GSTN_WallSurfaceModeTypes.h"
#include "GSTN_WallSurfaceModeUtils.generated.h"

class UMoverComponent;
struct FMoverSyncState;
struct FHitResult;

/**
 * GSTN_WallSurfaceModeUtils: the surface-driven choice of the wall mode as functions of data (wall-slide-v2.md 6.3), shared by the entry transition and
 * the v2 wall modes. The choice itself (PickMode) and the hysteresis (StepVote) are pure, so they are tested without a world; the two Mover helpers
 * read only what a simulation tick may read (the registered modes, the tags of the tick's start state), so a re-simulated frame decides the same.
 */
UCLASS()
class GASP_GIUSTINO2_API UGSTN_WallSurfaceModeUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Mode name of the climb v2 mode as registered on the Mover component by Tools/GAS/setup_wall_slide_modes.py */
	static const FName ClimbModeName;
	/** ... and of the slide v2 mode */
	static const FName SlideModeName;

	/** The default priority list: WallClimb -> WallClimbingV2, WallSlide -> WallSlidingV2; unmarked surfaces -> WallSlidingV2; blocked -> Falling; 6 ticks */
	static FGSTN_WallSurfaceModeList MakeDefaultList();

	/**
	 * The mode that handles a surface offering Behaviours: the first entry, in order, that matches them, whose mode IsRegistered says is available
	 * and whose BlockedWhen IsBlocked says is not blocked. A surface nothing marked (bUnknownSurface) gets UnknownSurfaceModeName if it is registered
	 * and no entry is evaluated (the unknown surface's behaviours are the plugin's catch-all default, which would make every wall a climb wall).
	 * Returns None when nothing handles it. bOutBlocked is set when entries matched but all of them were blocked (the caller decides what a
	 * blocked character does: BlockedModeName for a mode on a wall, nothing for the entry transition)
	 */
	static FName PickMode(const FGSTN_WallSurfaceModeList& List, const FGameplayTagContainer& Behaviours, bool bUnknownSurface,
		TFunctionRef<bool(FName)> IsRegistered, TFunctionRef<bool(const FGameplayTagQuery&)> IsBlocked, bool& bOutBlocked);

	/**
	 * The mode an entry transition enters for a wall hit: UGSTN_SurfaceUtils::Resolve on the hit, then PickMode with the Mover component's registered
	 * modes and, when SyncState is given, its tags (the stamina hook). None = no listed, registered, unblocked mode takes the surface. Shared by the
	 * surface-driven entry transitions (plain and ability gated), so they cannot diverge
	 */
	static FName ChooseModeForHit(const FGSTN_WallSurfaceModeList& List, const FHitResult& Hit, const UMoverComponent* MoverComp, const FMoverSyncState* SyncState);

	/**
	 * One tick of the hysteresis: the new vote count. bWantsOther = the contact asks for a mode other than the active one. The count rises by one for
	 * every consecutive such tick and is cleared by the first tick that does not ask; bOutSwitch becomes true on the tick the count reaches
	 * HysteresisTicks. A surface that flips every tick therefore never switches
	 */
	static uint8 StepVote(uint8 VoteTicks, bool bWantsOther, int32 HysteresisTicks, bool& bOutSwitch);

	/** True if the component has a mode of that name; true without a component (a headless test, nothing to check against) */
	static bool IsModeRegistered(const UMoverComponent* MoverComp, FName ModeName);

	/**
	 * Whether the Mover tags of SyncState match Query. Only the tags the query names are looked up (HasGameplayTagInState, parents included), so no
	 * tag container of the state has to be built. An empty query, or no component, is never blocked
	 */
	static bool IsBlockedByStateTags(const UMoverComponent* MoverComp, const FMoverSyncState& SyncState, const FGameplayTagQuery& Query);
};
