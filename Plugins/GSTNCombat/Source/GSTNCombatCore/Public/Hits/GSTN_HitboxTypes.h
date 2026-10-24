// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Hits/GSTN_HitData.h"
#include "Hits/GSTN_HitShape.h"
#include "GSTN_HitboxTypes.generated.h"

/** The space a hitbox's shape and keys live in */
UENUM(BlueprintType)
enum class EGSTN_HitboxSpace : uint8
{
	/** Relative to the attacker's root, at the frame being evaluated */
	Root,
	/** Relative to a socket whose root-relative transform was baked for every timeline frame (FGSTN_CombatTimelineData::BakedSockets) */
	BakedSocket,
	/** Relative to the attacker's transform at the frame the move started: the hitbox stays where it was thrown */
	WorldAtMoveStart,
	/** Owned by a projectile that moves by itself (M10); the timeline only spawns it */
	Projectile
};

/** The axis of a hitbox's space along which a weapon-length mod stretches it. None = the hitbox ignores reach mods */
UENUM(BlueprintType)
enum class EGSTN_ReachAxis : uint8
{
	None,
	X,
	Y,
	Z
};

/** Label of a hitbox inside its group. It changes no rule: it drives editor colours and lets cues pick a sweetspot sound */
UENUM(BlueprintType)
enum class EGSTN_HitboxZone : uint8
{
	Normal,
	Sweet,
	Sour
};

/**
 * One key of a hitbox's path. Frame counts from the first frame of the track (StartFrame), so a track can be moved without
 * re-keying. Between two keys offset and scale interpolate linearly and the rotation by quaternion slerp; before the first
 * key and after the last the nearest key holds.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_HitboxKey
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 Frame = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key", meta = (ForceUnits = "cm"))
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Key")
	FVector Scale = FVector::OneVector;
};

/**
 * A hitbox of a timeline (combat-m4-hits 3): its shape, where it lives and moves, which group it counts for, and the hit it
 * delivers. Sweetspots and sourspots are several hitboxes of the same HitGroup with different Data: when more than one of
 * them overlaps the same victim in the same frame the highest Data.Priority wins, ties by the lower array index (the rule
 * itself runs in the attacker tick, H8).
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_HitboxSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitbox")
	FGSTN_HitShape Shape;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitbox")
	EGSTN_HitboxSpace Space = EGSTN_HitboxSpace::Root;

	/** The baked socket with Space BakedSocket */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitbox", meta = (EditCondition = "Space == EGSTN_HitboxSpace::BakedSocket"))
	FName Socket = NAME_None;

	/** The path, sorted by Frame. Empty = the shape stays where its space puts it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitbox")
	TArray<FGSTN_HitboxKey> Keys;

	/** A victim is hit at most once per group and attack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitbox")
	uint8 HitGroup = 0;

	/** Tests the sub-steps between the previous and the current frame, so a fast hitbox does not tunnel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitbox")
	bool bSweepBetweenFrames = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitbox")
	FGSTN_HitData Data;

	/** Per victim relationship (keys are Combat.Relationship.*): ignore it, scale the shape, or change the hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitbox", meta = (ForceInlineRow, Categories = "Combat.Relationship"))
	TMap<FGameplayTag, FGSTN_HitVariant> RelationshipVariants;

	/** Data for kit mods that lengthen a weapon (data only, read by the loadout layer): position and extent along this axis scale with the mod's ReachScale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitbox|Reach")
	EGSTN_ReachAxis ReachAxis = EGSTN_ReachAxis::None;

	/** False keeps the extent when the position scales: a tip sweetspot that moves to the new tip but keeps its size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitbox|Reach", meta = (EditCondition = "ReachAxis != EGSTN_ReachAxis::None"))
	bool bScaleExtentWithReach = true;

	/** Label only (data, no reader in M4) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitbox")
	EGSTN_HitboxZone Zone = EGSTN_HitboxZone::Normal;
};

/**
 * One socket's root-relative transform for every frame of a timeline, written by the baking tool (a Python helper in M4, the
 * timeline editor in M5). The nested map is not reflectable, so the frames sit in a struct.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_BakedSocketFrames
{
	GENERATED_BODY()

	/** Index = timeline frame. After the last frame the last transform holds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Baked")
	TArray<FTransform> Frames;
};
