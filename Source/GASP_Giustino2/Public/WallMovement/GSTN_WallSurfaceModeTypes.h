// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_WallSurfaceModeTypes.generated.h"

/**
 * One line of the surface-to-mode priority list (wall-slide-v2.md 6.3): "a surface offering these behaviours is handled by this mode".
 * Data only. The behaviours come from the traversal surface contract (Surface.Behaviour.*); the mode is a name registered on the Mover component.
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallSurfaceModeEntry
{
	GENERATED_BODY()

	/** The surface must match this to be handled by ModeName. Empty = every surface */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Mode")
	FGameplayTagQuery Behaviours;

	/** Movement mode (as registered on the Mover component) that handles the surface */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Mode")
	FName ModeName = NAME_None;

	/**
	 * The stamina hook, data only (the wall modes have no GAS dependency): while the Mover state carries tags that match this query (an ability or
	 * a gameplay effect that grants a tag, a Mover modifier...), the entry is skipped as if its mode were not registered. Empty = never blocked.
	 * A surface whose only matching entries are blocked sends a character that is on a wall to FGSTN_WallSurfaceModeList::BlockedModeName and
	 * refuses the entry transition. The state read is the tick's start state (Mover tags, rolled back with it), never a game thread object
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Mode")
	FGameplayTagQuery BlockedWhen;
};

/**
 * The surface-driven choice of the wall mode, shared by the entry transition (UGSTN_WallSurfaceModeTransition: which mode grabs a wall) and by the v2
 * wall modes (UGSTN_WallTraverseModeBase: which mode to hand over to while on a wall). The first entry, in order, whose Behaviours match the surface
 * and whose mode is registered and not blocked wins, so the list is a priority list (climb before slide by default).
 */
USTRUCT(BlueprintType)
struct GASP_GIUSTINO2_API FGSTN_WallSurfaceModeList
{
	GENERATED_BODY()

	/** Switch modes while on a wall when the contact surface asks for another one (the transition ignores this) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Mode")
	bool bEnabled = true;

	/** Priority list, first match wins. Filled with WallClimb -> WallClimbingV2, WallSlide -> WallSlidingV2 by the v2 modes and the transition */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Mode")
	TArray<FGSTN_WallSurfaceModeEntry> Entries;

	/**
	 * Mode for a surface nothing marked (no surface component, provider or material entry: it resolves to the plugin's default, which offers
	 * everything). It is the slide by default so that level geometry nobody authored keeps its old wall slide and is never a climb wall by accident.
	 * None = unmarked surfaces are not walls for the list
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Mode")
	FName UnknownSurfaceModeName = NAME_None;

	/**
	 * Where a character on a wall goes when the surface only matches blocked entries (out of stamina on a climb-only wall). Falling by default.
	 * None = it stays
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Mode")
	FName BlockedModeName = NAME_None;

	/**
	 * In-place switch only: consecutive ticks the contact must ask for the other mode before the switch happens. Mixed geometry (a seam, a
	 * striped wall) flips the contact surface from one tick to the next; a vote that has to hold for this long never flaps. 1 = switch at once
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall|Surface Mode", meta = (ClampMin = "1", ClampMax = "120"))
	int32 HysteresisTicks = 6;

	/** True when the list can decide anything */
	bool IsActive() const { return bEnabled && Entries.Num() > 0; }

	/** True if ModeName is the mode of one of the entries or the unknown-surface mode: the modes a character may hand over between in place */
	bool ContainsMode(FName ModeName) const
	{
		if (ModeName.IsNone())
		{
			return false;
		}
		if (ModeName == UnknownSurfaceModeName)
		{
			return true;
		}
		for (const FGSTN_WallSurfaceModeEntry& Entry : Entries)
		{
			if (Entry.ModeName == ModeName)
			{
				return true;
			}
		}
		return false;
	}
};
