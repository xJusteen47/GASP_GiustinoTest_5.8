// Minion, pure part (no world, no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "LOD/GSTN_MinionLOD.h"
#include "GSTN_MinionDefinition.generated.h"

class AActor;
class UAnimInstance;
class USkeletalMesh;
class UGSTN_CombatTimeline;
class UGSTN_HurtboxSetDefinition;

namespace GSTNMinion
{
	/** Most entries in a definition's move list: the net item's MoveId has 6 bits and 0 means none (8.2) */
	constexpr int32 MaxMoves = 31;

	/** Longest timeline a minion may play, in frames: the move start stamp has 10 bits (8.2) */
	constexpr int32 MaxTimelineFrames = 1023;

	/** Primary asset type of UGSTN_MinionDefinition */
	inline FPrimaryAssetType DefinitionAssetType() { return FPrimaryAssetType(TEXT("GSTN_MinionDefinition")); }
}

/** How a minion type moves (combat-m8-minions.md 6): decided per type by spike 3 */
UENUM(BlueprintType)
enum class EGSTN_MinionMovementBackend : uint8
{
	/** The batch's own lightweight movement (steering, ground snap, knockback integration) */
	Lightweight = 0,
	/**
	 * A Mover component on the minion (optional module, only for types that need it). G4: not built, spike 3 decision B; such
	 * types run the lightweight backend (the subsystem warns once)
	 */
	Mover
};

/**
 * Numbers of a minion type that are not part of a timeline. Converted to attributes only when a minion is promoted to a fighter,
 * so a minion never carries an attribute set.
 */
USTRUCT(BlueprintType)
struct GSTNMINIONCORE_API FGSTN_MinionStats
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Stats", meta = (ClampMin = "1.0"))
	float MaxHealth = 40.f;

	/** Mass for the weight-driven knockback model (higher = flies less) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Stats", meta = (ClampMin = "1.0"))
	float Weight = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Stats", meta = (ClampMin = "0.0"))
	float KnockbackResistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Stats", meta = (ClampMin = "0.0"))
	float HitstunResistance = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float WalkSpeed = 200.f;

	/** Also the speed the net item's 6-bit speed field maps to 63 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float RunSpeed = 450.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement", meta = (ClampMin = "0.0"))
	float TurnRateDegPerSec = 540.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement", meta = (ClampMin = "0.0"))
	float Acceleration = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement", meta = (ClampMin = "0.0"))
	float Braking = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement", meta = (ClampMin = "0.0"))
	float GravityScale = 1.f;

	/** Highest obstacle the minion steps onto while walking, and how far down the ground snap reaches */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Movement", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float StepHeight = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Capsule", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float CapsuleRadius = 34.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Capsule", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float CapsuleHalfHeight = 88.f;
};

/**
 * One move a brain can request: a Command.Minion.* tag mapped to a timeline, with the range and cooldown the batch checks
 * before it starts it. Index in the list + 1 is the move's MoveId on the wire (0 = none).
 */
USTRUCT(BlueprintType)
struct GSTNMINIONCORE_API FGSTN_MinionMove
{
	GENERATED_BODY()

	/** The command a brain requests to start this move */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Move", meta = (Categories = "Command.Minion"))
	FGameplayTag Command;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Move")
	TSoftObjectPtr<UGSTN_CombatTimeline> Timeline;

	/** Frames after the move ends before it may start again */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Move", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 CooldownFrames = 60;

	/** Closest distance to the target at which the move may start */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MinRange = 0.f;

	/** Farthest distance to the target at which the move may start */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Move", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxRange = 200.f;

	/** Higher wins when several moves are possible */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Move")
	int32 Priority = 0;
};

/**
 * Timelines a minion plays in reaction to the world rather than on request. Hitstun and Death are required; PushBack is optional
 * and falls back to Hitstun. Timelines are the same assets the fighters use (UGSTN_CombatTimeline), played by the batch.
 */
USTRUCT(BlueprintType)
struct GSTNMINIONCORE_API FGSTN_MinionReactionTimelines
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Reactions")
	TSoftObjectPtr<UGSTN_CombatTimeline> Spawn;

	/** Required: the stagger in place */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Reactions")
	TSoftObjectPtr<UGSTN_CombatTimeline> Hitstun;

	/** Optional: the push back tier; empty falls back to Hitstun */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Reactions")
	TSoftObjectPtr<UGSTN_CombatTimeline> PushBack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Reactions")
	TSoftObjectPtr<UGSTN_CombatTimeline> Launched;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Reactions")
	TSoftObjectPtr<UGSTN_CombatTimeline> Knockdown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Reactions")
	TSoftObjectPtr<UGSTN_CombatTimeline> GetUp;

	/** Required: played when health reaches zero */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minion|Reactions")
	TSoftObjectPtr<UGSTN_CombatTimeline> Death;
};

/**
 * One minion type (combat-m8-minions.md 3): everything the batch and the presentation need to know about it, as data. One asset
 * per type, listed in the project's minion registry (same order on both sides of the wire, so the net item sends an index).
 *
 * G2 holds the parts that need no runtime class: tag, stats, hurtbox set, moves, reaction timelines, LOD override, movement
 * backend, team and the soft class references. The knockback model override and the StateTree reference join in the checkpoints
 * that add their classes. Referenced softly everywhere so loading a definition does not load the world of animations.
 *
 * IsDataValid reports (errors unless noted): a type tag that is not under Minion.Type, non-positive health, weight or capsule,
 * a missing hurtbox set, more than MaxMoves moves, a move with no command or no timeline or a negative range or a duplicate
 * command, a missing Hitstun or Death timeline, a timeline above MaxTimelineFrames frames, an inconsistent LOD override, and
 * (warning) a run speed below the walk speed. "Only supported tracks" is checked by the editor module, which knows the track list.
 */
UCLASS(BlueprintType)
class GSTNMINIONCORE_API UGSTN_MinionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** The type: a leaf under Minion.Type (declared in Config/Tags/GSTNMinion.ini or the game's own tag ini) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion", meta = (Categories = "Minion.Type"))
	FGameplayTag TypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion")
	FGSTN_MinionStats Stats;

	/** The hurtboxes of the type (a set, so a shield or a part cluster is added by data) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion|Combat")
	TSoftObjectPtr<UGSTN_HurtboxSetDefinition> HurtboxSet;

	/** The moves a brain can request, at most 31 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion|Combat")
	TArray<FGSTN_MinionMove> Moves;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion|Combat")
	FGSTN_MinionReactionTimelines Reactions;

	/** Team id (FGenericTeamId value) of the type. A plain byte so this module needs no AI module */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion|Combat")
	uint8 TeamId = 1;

	/** Most hits one attack of this type may land on one victim in one frame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion|Combat", meta = (ClampMin = "1"))
	int32 MaxHitsPerFrame = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion|Movement")
	EGSTN_MinionMovementBackend MovementBackend = EGSTN_MinionMovementBackend::Lightweight;

	/** True to use LODOverride instead of the project's UGSTN_MinionLODSettings for this type */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion|LOD")
	bool bOverrideLOD = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion|LOD", meta = (EditCondition = "bOverrideLOD"))
	FGSTN_MinionLODParams LODOverride;

	/** Actor class that presents the minion on a client (a skeletal mesh puppet). Empty = the runtime default */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion|Presentation")
	TSoftClassPtr<AActor> PuppetClass;

	/**
	 * Mesh of the pooled puppet (G7). Soft: a client loads it when the first puppet of the type is acquired, the server never does.
	 * Empty is valid (a headless or -nullrhi run, a test): the puppet then has no mesh and costs only its transform
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion|Presentation")
	TSoftObjectPtr<USkeletalMesh> PuppetMesh;

	/** Animation Blueprint of the puppet's mesh. Empty = the mesh plays nothing (the reference pose) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion|Presentation")
	TSoftClassPtr<UAnimInstance> PuppetAnimClass;

	/**
	 * The brain of the type: a StateTree asset using UGSTN_MinionStateTreeSchema (G8). Kept as a soft UObject reference so Core has no
	 * StateTree dependency; the runtime casts it. Empty = the type has no brain (a script or the bench writes its intents).
	 * Server only: a client never loads it
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion|Brain", meta = (AllowedClasses = "/Script/StateTreeModule.StateTree"))
	TSoftObjectPtr<UObject> Brain;

	/** Fighter class the minion becomes when a script promotes it. Empty = the type cannot be promoted */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Minion|Promotion")
	TSoftClassPtr<AActor> PromotionFighterClass;

	/** The LOD parameters this type uses: its override when set, otherwise Defaults */
	const FGSTN_MinionLODParams& ResolveLODParams(const FGSTN_MinionLODParams& Defaults) const
	{
		return bOverrideLOD ? LODOverride : Defaults;
	}

	/** The index in Moves of the move for a command, INDEX_NONE if none. MoveId on the wire is this + 1 */
	int32 FindMoveIndex(const FGameplayTag& Command) const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
