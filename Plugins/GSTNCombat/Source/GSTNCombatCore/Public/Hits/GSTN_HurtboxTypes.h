// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Hits/GSTN_HitShape.h"
#include "Hits/GSTN_ReactionTypes.h"
#include "GSTN_HurtboxTypes.generated.h"

class USkeletalMesh;

/** How a hurtbox takes hits at one frame, after the hurtbox modifier tracks of the running timeline are applied */
UENUM(BlueprintType)
enum class EGSTN_HurtState : uint8
{
	Normal,
	/** Not hit at all: the hit passes through */
	Intangible,
	/** The hit registers (attacker hitstop, confirm) but there is no damage, knockback or hitstun */
	Invincible,
	/** No knockback or hitstun while the hit's stagger force is below the armor threshold; damage applies */
	Armor
};

/**
 * The way into a hurtbox that sits inside another one (combat-m4-hits 9c B, combat-parts 3.4): a hollow medium-hardness cavity inside
 * a heavy-armour leg. Hit shapes are pure maths with no world query, so a bulky swing that overlaps the leg from outside also overlaps
 * the cavity inside it and would pick it by Priority, hitting soft material through the armour. With an opening, the cavity counts
 * as overlapped only when the hitbox also overlaps the Mouth AND its pass radius fits MaxPassRadiusCm; otherwise it is left out of
 * the overlap list and the pick falls to the next candidate by Priority (the armour around it).
 *
 * The hitbox's pass radius is derived from its shape as the world occlusion rule derives it (9c A): a sphere's radius, a capsule's
 * radius, a box's smallest half-size. Static data on a static set, so the attacker's prediction and the authority agree.
 * Optional and off by default: a hurtbox without one is tested exactly as before, at the cost of one bool.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_HurtboxOpening
{
	GENERATED_BODY()

	/** Off = the hurtbox has no opening and is reached by plain overlap (every set authored before openings) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox|Opening")
	bool bEnabled = false;

	/** The hole, root-relative like the hurtbox's own shape; authored at the surface of the armour, where a hitbox enters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox|Opening", meta = (EditCondition = "bEnabled"))
	FGSTN_HitShape Mouth;

	/** The largest hitbox pass radius that fits through the hole: a 2 cm stab or a 1 cm bullet passes a 5 cm hole, a 30 cm hammer does not */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox|Opening", meta = (EditCondition = "bEnabled", ClampMin = "0.0", ForceUnits = "cm"))
	float MaxPassRadiusCm = 5.f;
};

/**
 * One hurtbox of a combatant: where it is and how it reacts. Shape is root-relative, baked from the reference pose, so a
 * hurtbox costs no bone transform at runtime.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_Hurtbox
{
	GENERATED_BODY()

	/** Identity inside the set (what a modifier track addresses by name) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox")
	FName Name = NAME_None;

	/** The bone the shape was baked from (tooling only) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox")
	FName Bone = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox")
	FGSTN_HitShape Shape;

	/** Classification (Combat.Hurtbox.Head, .Feet...). What a modifier track addresses by tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox", meta = (Categories = "Combat.Hurtbox"))
	FGameplayTagContainer Tags;

	/** Among the overlapped hurtboxes of one hit the highest wins; ties go to the lower index in the set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox")
	uint8 Priority = 0;

	/** The "where" of a hit: scales, stagger threshold and reaction cap of this hurtbox */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox")
	FGSTN_HitReactionModifiers ReactionScale;

	/** Optional way in for a hurtbox enclosed by another (a cavity behind armour). Off by default; see FGSTN_HurtboxOpening */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox|Opening")
	FGSTN_HurtboxOpening Opening;

	/**
	 * A deep weak point (plan precisione-weakpoint 3): only a Stab timeline whose hitbox travels within StabMaxAngleDeg of the way in (against
	 * the face normal at the entry point) touches it; anything else passes as if it were not there. Author it a few cm outside the hard shell
	 * so a precise strike meets it before the shell
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox|Stab")
	bool bStabOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox|Stab", meta = (EditCondition = "bStabOnly", ClampMin = "0.0", ClampMax = "90.0", ForceUnits = "deg"))
	float StabMaxAngleDeg = 30.f;
};

/**
 * A hurtbox a plugin contributes to an entity for one snapshot, on top of its set (a shield bubble, M6 5.3 "Contact"). Not part of
 * the set asset: it is derived every frame from replicated state, so it follows the entity through the lag compensation history and
 * is the same on every machine that reads the same state. The shape is root-relative like a set hurtbox's.
 *
 * An extra shares the priority scale of the body: the highest Priority among the overlapped hurtboxes wins and a tie goes to the extra
 * (so a bubble at priority 0 still sits above the body it covers). Its index in a hit is ExtraIndexBase + position in the list.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ExtraHurtbox
{
	GENERATED_BODY()

	/** Root-relative, like FGSTN_Hurtbox::Shape */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox")
	FGSTN_HitShape Shape;

	/** Same scale as FGSTN_Hurtbox::Priority; on a tie the extra wins (the lower position among extras) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox")
	uint8 Priority = 0;

	/** Hit types this extra lets through (the hit passes it as if it were not there): matched hierarchically, an unset hit type is never ignored */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox", meta = (Categories = "Combat.HitType"))
	FGameplayTagContainer IgnoreHitTypes;

	/** Who contributed it: the contributor's feature id (GSTNFeatureIds), so a step can tell a bubble from another extra without a tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox")
	uint8 Source = 0;

	/** As FGSTN_Hurtbox::bStabOnly */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox|Stab")
	bool bStabOnly = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox|Stab", meta = (EditCondition = "bStabOnly", ClampMin = "0.0", ClampMax = "90.0", ForceUnits = "deg"))
	float StabMaxAngleDeg = 30.f;

	/**
	 * A shell hit from outside only (M11 L7c, the backpack dome; sphere only): a melee attacker whose root, or a projectile whose previous
	 * position, is inside the sphere never touches it, so one hit counts on one side of the shell only. Every consumer of the extras (attacker
	 * tick, minion attacker, projectile) asks IsSkippedFrom with the point of the same snapshot, so the attacker and the victim agree
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hurtbox")
	bool bOutsideOnly = false;

	/** The extra is left out for a hit that comes from FromWorld: bOutsideOnly and FromWorld inside the sphere placed on EntityTransform. Other shapes never are */
	bool IsSkippedFrom(const FVector& FromWorld, const FTransform& EntityTransform) const;
};

/** The extras of one entity in a snapshot or a history record: inline, because there are at most MaxExtraHurtboxes and most frames none */
using FGSTN_ExtraHurtboxList = TArray<FGSTN_ExtraHurtbox, TInlineAllocator<2>>;

/** Hurtbox indices of a hit: the set takes [0, ExtraIndexBase), the extras of the snapshot take ExtraIndexBase + position */
namespace GSTNHurtboxIndex
{
	constexpr int32 ExtraIndexBase = 0x80;

	/** Extras one entity carries; a contributor that appends more is truncated, identically on every machine */
	constexpr int32 MaxExtraHurtboxes = 2;

	inline bool IsExtra(int32 Index) { return Index >= ExtraIndexBase && Index < ExtraIndexBase + MaxExtraHurtboxes; }
	inline int32 ToExtraPosition(int32 Index) { return Index - ExtraIndexBase; }
	inline int32 FromExtraPosition(int32 Position) { return ExtraIndexBase + Position; }
}

/**
 * The hurtboxes of one kind of combatant (combat-m4-hits 3), referenced by soft reference from the character data. The
 * threshold and the reaction cap here are the entity defaults, so a whole enemy type gets them without touching each hurtbox.
 */
UCLASS(BlueprintType)
class GSTNCOMBATCORE_API UGSTN_HurtboxSetDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Index in this array is the HurtboxIndex a hit carries (at most 128 entries: the indices from GSTNHurtboxIndex::ExtraIndexBase are the extras') */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hurtboxes")
	TArray<FGSTN_Hurtbox> Hurtboxes;

	/** Entity default of the hitstun condition. Unset = the rules' value */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	FGSTN_StaggerThreshold StaggerThreshold;

	/** Entity default cap of the reaction tier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	EGSTN_HitReaction MaxReaction = EGSTN_HitReaction::Launch;

	/** Weight against progress of damage taken. Empty = the rules' curve */
	UPROPERTY(EditAnywhere, Category = "Reaction")
	FGSTN_BakedCurve WeightCurve;

#if WITH_EDITORONLY_DATA
	/**
	 * The mesh the hurtboxes were authored on: what the hurtbox set editor shows them over, and what the validator checks the bones against.
	 * Authoring data only (stripped from cooked builds, never read by a simulation). Empty: the editor falls back to its preview subject
	 */
	UPROPERTY(EditAnywhere, Category = "Preview")
	TSoftObjectPtr<USkeletalMesh> PreviewMesh;
#endif

#if WITH_EDITOR
	/** FGSTN_CombatRulesValidation::ValidateHurtboxSet, then the set's reaction matrix against the settings' rules as an info message */
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
