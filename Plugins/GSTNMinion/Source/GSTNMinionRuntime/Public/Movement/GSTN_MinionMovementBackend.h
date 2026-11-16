// Minion runtime: movement backends (authority batch stage 4, M8 G4).

#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Engine/EngineTypes.h"
#include "Templates/Function.h"
#include "GSTN_MinionMovementBackend.generated.h"

class UGSTN_MinionDefinition;
class UWorld;
struct FGSTN_MinionStore;

/** What drives a minion's velocity besides its intent. A runtime column, never on the wire (the action state carries the tier) */
enum class EGSTN_MinionKnockbackMode : uint8
{
	/** Locomotion: intent, gravity when off the ground */
	None = 0,
	/** Flying from a launch: gravity only, no control, ends on a walkable landing */
	Launched,
	/** Sliding on the ground from a push back: decays, snaps to the ground, stops at walls and ledges, never airborne */
	PushedBack
};

/**
 * Tuning of the lightweight backend, shared by every type (the per-type numbers are in FGSTN_MinionStats). Held by the minion
 * subsystem; a project-settings home comes with the LOD settings exposure (G11).
 *
 * Collision is an object-type query (WorldStatic + WorldDynamic by default), not a trace channel: a dedicated GSTN_MinionBlock
 * channel needs a DefaultEngine.ini entry, and an optional plugin must not require the project to edit its collision profile.
 * Levels that need minion-only blockers can add a custom object type and list it here.
 */
USTRUCT(BlueprintType)
struct GSTNMINIONRUNTIME_API FGSTN_MinionMovementSettings
{
	GENERATED_BODY()

	/** Object types the capsule sweeps block on */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement|Collision")
	TArray<TEnumAsByte<ECollisionChannel>> BlockingObjectTypes = { ECC_WorldStatic, ECC_WorldDynamic };

	/** Steepest walkable floor, against the gravity up. 40 degrees walks, 60 blocks */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement|Collision", meta = (ClampMin = "0.0", ClampMax = "89.0", ForceUnits = "deg"))
	float WalkableSlopeDegrees = 45.f;

	/** Gap kept between the capsule and the floor, so a ground move does not start every sweep touching the floor */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement|Collision", meta = (ClampMin = "0.1", ClampMax = "5.0", ForceUnits = "cm"))
	float FloorSkin = 1.f;

	/** Fall speed cap along gravity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement|Air", meta = (ClampMin = "100.0", ForceUnits = "cm/s"))
	float MaxFallSpeed = 4000.f;

	/** Deceleration of a push back while it slides on the ground */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement|Knockback", meta = (ClampMin = "0.0", ForceUnits = "cm/s^2"))
	float PushBackDeceleration = 2000.f;

	/** A push back below this speed ends (the minion goes back to Idle) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement|Knockback", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float PushBackStopSpeed = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement|Separation")
	bool bSeparation = true;

	/** Separation speed per cm of overlap between two capsules (on the ground plane) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement|Separation", meta = (ClampMin = "0.0"))
	float SeparationStiffness = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement|Separation", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MaxSeparationSpeed = 300.f;
};

/**
 * Movement columns the Core store does not carry (Capacity entries each, reset when a slot is activated or freed). Kept out of
 * the store because they are authority-only: the net item sends position, yaw, speed and the action state, never these.
 */
struct GSTNMINIONRUNTIME_API FGSTN_MinionMovementColumns
{
	void Reset(int32 Capacity);
	void ResetSlot(int32 Slot);

	int32 Capacity() const { return Velocity.Num(); }

	TArray<FVector3f> Velocity;
	/** Velocity a hit (or a script) asked for; consumed by the next step of the slot. Zero = none */
	TArray<FVector3f> PendingLaunch;
	/** 1 when the capsule stands on a walkable floor */
	TArray<uint8> bGrounded;
	/** EGSTN_MinionKnockbackMode */
	TArray<uint8> KnockbackMode;
};

/**
 * Everything a backend needs for one batch frame. Built by the minion subsystem; a backend reads the store and the columns
 * and writes Position, YawDegrees, Speed, ActionState (locomotion and the end of a knockback only) and the movement columns.
 *
 * Gravity is a function of the position, so a gravity field (G-gravity plugin) can plug in without touching the backend; the
 * default is the world's gravity Z. Up is always minus the gravity direction at the capsule, and the capsule is aligned to it.
 */
struct GSTNMINIONRUNTIME_API FGSTN_MinionMoveContext
{
	UWorld* World = nullptr;
	FGSTN_MinionStore* Store = nullptr;
	FGSTN_MinionMovementColumns* Columns = nullptr;
	/** By store TypeIndex */
	TConstArrayView<TObjectPtr<UGSTN_MinionDefinition>> Definitions;
	/** By slot: last frame the slot ran a batch. dt of a slot = FixedStepSeconds x (Frame - LastRunFrame), at least one step */
	TConstArrayView<int32> LastRunFrame;
	int32 Frame = 0;
	float FixedStepSeconds = 1.f / 62.5f;

	/** Gravity acceleration (cm/s^2) at a world position. Unset = (0, 0, World->GetGravityZ()) */
	TFunction<FVector(const FVector&)> GravityAt;

	FGSTN_MinionMovementSettings Settings;
	FCollisionObjectQueryParams ObjectQuery;
	FCollisionQueryParams QueryParams;
	/** Feet below this height (world Z) = fell out of the world */
	float KillZ = -UE_BIG_NUMBER;

	// Outputs, appended by the backend in slot order

	/** Slots that fell below KillZ: the subsystem counts and frees them */
	TArray<int32> FellOut;
	/** Slots stepped this frame (the bench's per-minion-frame mean) */
	int32 MinionFrames = 0;

	float DeltaSecondsOf(int32 Slot) const
	{
		const int32 Last = LastRunFrame.IsValidIndex(Slot) ? LastRunFrame[Slot] : INDEX_NONE;
		const int32 Frames = Last == INDEX_NONE ? 1 : FMath::Max(1, Frame - Last);
		return FixedStepSeconds * static_cast<float>(Frames);
	}

	FVector GetGravityAt(const FVector& Position) const;
};

/**
 * A minion movement implementation (combat-m8-minions.md 6). StepBatch moves every slot of Slots (ascending slot order, alive,
 * activated, not dormant) by its own dt and leaves the end-of-frame position, velocity and ground flag in the store and the
 * columns. Deterministic given the store, the columns, the intents and the world's collision: no wall clock, no random, every
 * per-slot loop in the order of Slots.
 */
class GSTNMINIONRUNTIME_API IGSTN_MinionMovementBackend
{
public:
	virtual ~IGSTN_MinionMovementBackend() = default;

	virtual void StepBatch(FGSTN_MinionMoveContext& Ctx, TArrayView<const int32> Slots) = 0;
};
