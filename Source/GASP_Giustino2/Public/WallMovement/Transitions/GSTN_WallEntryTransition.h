// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MovementModeTransition.h"
#include "GSTN_WallEntryTransition.generated.h"

struct FHitResult;
struct FMoverSyncState;

/** Whether the wall can be grabbed while still going up */
UENUM(BlueprintType)
enum class EGSTN_WallEntryVertical : uint8
{
	/** Any vertical speed */
	Any,
	/** Only while falling (vertical speed <= 0), like the Mario games that don't let you wall slide on the way up */
	DescendingOnly,
	/** While going up no faster than MaxAscendingSpeed */
	BelowSpeed,
};

/** What to do when the wall to grab is the same as the last one (horizontal normals within SameWallAngle) */
UENUM(BlueprintType)
enum class EGSTN_SameWallPolicy : uint8
{
	/** No restriction */
	Allow,
	/**
	 * Only at or below the height the last wall was left at (+ tolerance): no gaining height by wall jumping up the same
	 * wall (Super Mario 3D World). Walls facing a different way are not limited, so alternating walls still climbs
	 */
	HeightCap,
	/** Never: wall jumps must go between walls facing different ways */
	Deny,
};

/**
 * Falling -> wall mode (e.g. WallSliding) when the character moves/pushes into a wall in the air.
 * Add it to the Transitions of the Falling movement mode (or of any air mode).
 *
 * It only decides WHETHER to grab; how the incoming momentum is kept is set on the target mode (EntryMomentum).
 * The wall found here is handed to the mode, which starts already aligned to it.
 *
 * Re-entry rules read the wall memory in FGSTN_WallMovementSyncState (last wall normal, height and time), which every
 * wall mode keeps up to date. Their purpose: with strong air control the character could leave a wall and re-grab it
 * higher up again and again, climbing it; the height cap stops that on walls facing the same (or a similar) way.
 * The memory is forgotten when:
 *  - the character went through one of MemoryResetModes (e.g. it landed and walked) after leaving the wall
 *  - one of MemoryResetTags is present (e.g. granted by an ability; only while present), or
 *    UGSTN_WallMovementLibrary::ResetWallMemory is called (FGSTN_ResetWallMemoryEffect, stored in the sync state)
 *  - a different wall is grabbed (it overwrites the memory)
 */
UCLASS(Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced)
class GASP_GIUSTINO2_API UGSTN_WallEntryTransition : public UBaseMovementModeTransition
{
	GENERATED_BODY()

public:
	UGSTN_WallEntryTransition(const FObjectInitializer& ObjectInitializer);

	virtual FTransitionEvalResult Evaluate_Implementation(const FSimulationTickParams& Params) const override;
	virtual void Trigger_Implementation(const FSimulationTickParams& Params) override;

	/** Mode to enter (as registered on the Mover component) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry")
	FName TargetModeName = TEXT("WallSliding");

	// ---------------------------------------------------------------- Detection

	/** How far ahead (along the stick and the horizontal velocity) the capsule looks for a wall (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Detection", meta = (ClampMin = "0", ForceUnits = "cm"))
	float ProbeDistance = 30.f;

	/** Grabbable surfaces: angle between their normal and gravity-up, from MinSurfaceAngle... */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Detection", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MinSurfaceAngle = 60.f;

	/** ...to MaxSurfaceAngle (90 = vertical, above = overhangs) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Detection", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float MaxSurfaceAngle = 120.f;

	// ---------------------------------------------------------------- Conditions

	/** The stick must push toward the wall */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Conditions")
	bool bRequireInputTowardWall = true;

	/** dot(stick direction, into the wall) needed. 1 = straight at it, 0.5 = within ~60 degrees */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Conditions", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bRequireInputTowardWall"))
	float MinInputTowardWallDot = 0.5f;

	/** Grabbing while going up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Conditions")
	EGSTN_WallEntryVertical VerticalPolicy = EGSTN_WallEntryVertical::Any;

	/** Max upward speed for VerticalPolicy = BelowSpeed (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Conditions", meta = (ClampMin = "0", ForceUnits = "cm/s",
		EditCondition = "VerticalPolicy == EGSTN_WallEntryVertical::BelowSpeed"))
	float MaxAscendingSpeed = 200.f;

	/** Time in the current (air) mode before a wall can be grabbed: avoids grabbing the wall you just jumped off from the ground (s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Conditions", meta = (ClampMin = "0", ForceUnits = "s"))
	float MinAirTime = 0.1f;

	// ---------------------------------------------------------------- Re-entry (wall memory)

	/** Rule for grabbing again the same wall as last time */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Re-entry")
	EGSTN_SameWallPolicy SameWallPolicy = EGSTN_SameWallPolicy::HeightCap;

	/** Walls whose horizontal normals are within this angle count as "the same wall" (deg) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Re-entry", meta = (ClampMin = "0", ClampMax = "180", ForceUnits = "deg"))
	float SameWallAngle = 30.f;

	/** HeightCap: how far above the last exit height the same wall can still be grabbed (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Re-entry", meta = (ForceUnits = "cm",
		EditCondition = "SameWallPolicy == EGSTN_SameWallPolicy::HeightCap"))
	float HeightCapTolerance = 10.f;

	/** The same wall can't be grabbed again for this long after leaving it, e.g. right after a wall jump (s) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Re-entry", meta = (ClampMin = "0", ForceUnits = "s"))
	float ReentryCooldown = 0.25f;

	/**
	 * Going through one of these modes after leaving a wall resets the wall memory (no height cap, no same-wall rule).
	 * Typically the ground modes: landing and walking resets it
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Re-entry")
	TArray<FName> MemoryResetModes;

	/**
	 * While any of these tags is present the wall memory is ignored, e.g. an ability that grants the tag to allow climbing
	 * the same wall again. It is not latched: the entry check is read-only (it runs more than once per tick and again on
	 * re-simulation), so the reset ends with the tag. To forget the wall for good, queue FGSTN_ResetWallMemoryEffect
	 * (UGSTN_WallMovementLibrary::ResetWallMemory) from the ability. Looked up on the Mover component
	 * (modes, layered moves, modifiers) and on the owner's IGameplayTagAssetInterface (the actor or a component of it,
	 * such as an AbilitySystemComponent). Owner tags are game-thread state, not part of the predicted input
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wall Entry|Re-entry")
	FGameplayTagContainer MemoryResetTags;

protected:
	/**
	 * Whether any MemoryResetTags is present in the simulated state (Mover's own tags) or on the owner. Owner tags are
	 * game-thread state: predicted only if their source (e.g. the GAS-NPP ability system) ticks before Mover
	 */
	bool HasMemoryResetTag(const FMoverSyncState& SyncState) const;

	/** A grabbable wall found by FindEntryWall */
	struct FEntryCandidate
	{
		FVector Normal = FVector::ZeroVector;
		FVector Point = FVector::ZeroVector;
	};

	/** All the checks. Returns false (with the reason when logging) if the wall can't be grabbed now */
	bool FindEntryWall(const FSimulationTickParams& Params, FEntryCandidate& OutCandidate) const;

	/** Capsule sweep along a horizontal direction for a grabbable surface */
	bool SweepForWall(const FSimulationTickParams& Params, const FVector& Direction, const FVector& GravityUp, FHitResult& OutHit) const;
};
