// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "GSTN_PartDefinition.generated.h"

/** What a hit on an already broken part does (combat-parts.md §3.1) */
UENUM(BlueprintType)
enum class EGSTN_BrokenHurtboxPolicy : uint8
{
	/** The stump stays hittable and takes the part's BrokenMainTransfer */
	Keep,
	/** The part's hurtboxes are hidden from the snapshot, so hits pass to the body */
	Disable,
	/**
	 * The stump has a smaller shape. Damage is computed as for Keep; the shape itself is a hit shape of the M4 hit data and is
	 * not declared yet, so validation reports a part that chooses this until the hit shape type exists
	 */
	Replace
};

/**
 * The most severe reaction tier a part (or the body) can undergo, from least to most severe. The order is part of the rule:
 * the cap of a hit is the lesser of the part's and the body's. It mirrors the hit reaction tiers of the hit pipeline
 * (stagger in place, push back, launch) value for value and name for name, so assets keep their meaning when the part resolve
 * step maps it onto them.
 */
UENUM(BlueprintType)
enum class EGSTN_PartReactionTier : uint8
{
	/** The entity flinches where it stands */
	StaggerInPlace,
	/** The entity slides back along the ground */
	PushBack,
	/** The entity flies (the default, no cap) */
	Launch
};

/**
 * How a part (or the whole body) answers a hit, as data (combat-parts.md §3.1, §4.4). Hitstun is the hit's stagger force
 * against StaggerThreshold, weight feeds the reaction tier, and these fields are the "where" of a hit. The defaults are the
 * identity, so a part that sets nothing reacts as the body does.
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartReaction
{
	GENERATED_BODY()

	/**
	 * Stagger force a hit must reach to put this part into hitstun. Negative = inherit: a part inherits the body's, the body
	 * inherits the combat rules' global value. A weak point is low, armour and boss plates high
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	float StaggerThreshold = -1.f;

	/** The most severe tier a hit on this part can cause; Launch = no cap. A heavy limb or a boss never flies */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	EGSTN_PartReactionTier MaxReaction = EGSTN_PartReactionTier::Launch;

	/** Multiplier of the effective weight that decides the knockback: a head hit may launch more than a leg hit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = 0))
	float JuggleFactor = 1.f;

	/** Multiplier of the automatic hitstop of a hit on this part; a weak point freezes longer */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = 0))
	float HitstopScale = 1.f;

	/** Multiplier of the knockback of a hit on this part: 0.3 for a pauldron, 1 for the torso */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = 0))
	float KnockbackScale = 1.f;
};

/**
 * Which broken parts make a large entity grabbable (combat-parts.md §7). The projected Combat.Part.GrabLocked tag is present
 * while a non-empty gate is unmet. An empty gate never locks. Both conditions apply when both are set.
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_GrabGate
{
	GENERATED_BODY()

	/** At least this many parts must be broken. 0 = no count condition */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab", meta = (ClampMin = 0, ClampMax = 16))
	int32 MinBrokenParts = 0;

	/** Part indices that must be broken (all of them, or any one with bAnyOf). Empty = no named-part condition */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab")
	TArray<int32> RequiredParts;

	/** One broken required part is enough instead of all of them */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grab")
	bool bAnyOf = false;

	/** True when the gate sets no condition, so it never locks */
	bool IsEmpty() const { return MinBrokenParts <= 0 && RequiredParts.IsEmpty(); }
};

/**
 * One destructible part of an entity (combat-parts.md §3.1). It maps hurtboxes to a health of its own and a share of each hit
 * to the entity's main pool. Fixed bounds are in GSTN_PartsLimits.h. Identity (Name) is unique and stable: other plugins key
 * their own data by it (a platform shape that disappears with the part), so renaming a part is a data migration.
 *
 * All the numbers are authored here and nowhere in code. Hardness is an integer tier on purpose: the rule that picks the
 * penetration band compares integers only.
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartDefinition
{
	GENERATED_BODY()

	/** Unique within the part set. Identity for designers, debug and bridges */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	FName Name = NAME_None;

	/** Classification (Part.Type.* leaves): head, arm, armour... Read by rules, cues and AI */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part", meta = (Categories = "Part.Type"))
	FGameplayTagContainer Types;

	/**
	 * Names in the entity's hurtbox set that belong to this part. A hurtbox belongs to at most one part; hurtboxes no part
	 * names belong to the implicit body (the full hit goes to the main pool). The hurtbox set itself is unchanged
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	TArray<FName> Hurtboxes;

	/** Bones hidden or detached when the part breaks. Presentation only; may be empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	TArray<FName> Bones;

	/** Index of the parent part, INDEX_NONE for a root. Breaking a part breaks its children in the same hit */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	int32 ParentIndex = INDEX_NONE;

	/** Armour tier 0..15: a hit's penetration against it picks the damage band. Integer, so no float picks the band */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = 0, ClampMax = 15))
	int32 Hardness = 0;

	/**
	 * How deep the part is, 0..1. The damage the part takes is Damage * (1 - Thickness) + DeepDamage * Thickness, where DeepDamage is the hit's own
	 * (it defaults to Damage). Independent of Hardness: hardness decides whether a hit gets through, thickness what it does once through
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = 0, ClampMax = 1))
	float Thickness = 0.f;

	/** Part damage at which the part breaks. 0 or less = unbreakable (an armour plate that only reduces) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = 0))
	float PartCap = 100.f;

	/** Multiplier of the damage that goes to the part (before the band) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = 0))
	float PartDamageScale = 1.f;

	/**
	 * Share of the band-scaled damage that goes to the main pool. A weak point has low hardness, a small PartCap and a value
	 * above 1 (2); an armoured limb 0.2 to 0.5; the torso 1
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = 0))
	float MainTransfer = 1.f;

	/** MainTransfer once the part is broken (the stump). Negative = the default, MainTransfer times 0.5 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	float BrokenMainTransfer = -1.f;

	/** What a hit on the broken part does */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	EGSTN_BrokenHurtboxPolicy BrokenPolicy = EGSTN_BrokenHurtboxPolicy::Keep;

	/**
	 * Cue, UI and event flag (Combat.Event.Part.WeakPointHit, hit marker). It has no damage effect of its own: the numbers
	 * above already encode a weak point, so it cannot double count
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part")
	bool bWeakPoint = false;

	/** Hitstun condition, reaction cap, juggle, hitstop and knockback scale of a hit on this part */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction")
	FGSTN_PartReaction Reaction;

	/** Fraction of the entity's base weight lost when this part breaks (a lighter, mutilated enemy flies further) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reaction", meta = (ClampMin = 0, ClampMax = 1))
	float LostMassFraction = 0.f;

	/** What breaking the part does: FGSTN_PartBreakEffect structs (tags, disabled hit groups, stats, bonus damage...) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break", meta = (BaseStruct = "/Script/GSTNPartsCore.GSTN_PartBreakEffect", ExcludeBaseStruct))
	TArray<FInstancedStruct> OnBreak;

	/** Whether the part can break at all */
	bool IsBreakable() const { return PartCap > 0.f; }

	/** MainTransfer of a hit on the part while it is broken */
	float GetBrokenMainTransfer() const { return BrokenMainTransfer >= 0.f ? BrokenMainTransfer : MainTransfer * 0.5f; }

	/** First break effect of type EffectT, or null */
	template <typename EffectT>
	const EffectT* FindBreakEffect() const
	{
		for (const FInstancedStruct& Entry : OnBreak)
		{
			if (const EffectT* Found = Entry.GetPtr<EffectT>())
			{
				return Found;
			}
		}
		return nullptr;
	}
};
