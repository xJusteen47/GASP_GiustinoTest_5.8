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

/** When a part held by other parts (FGSTN_PartDefinition::HeldBy) comes off (combat-parts.md 3.4) */
UENUM(BlueprintType)
enum class EGSTN_HeldByRule : uint8
{
	/** The part comes off when every holder is broken (a plate on four screws) */
	AllBroken,
	/** The part comes off when any one holder is broken (a cover on a latch or a hinge) */
	AnyBroken
};

/**
 * A screw or bolt (combat-parts.md 3.4): a part that hits turn out instead of damaging. The progress is the part's own damage byte
 * (0 in, 255 out), so the rule adds no state. A hit whose TurnSense equals LoosenSense moves the byte up by StepBytes(TurnSteps), the
 * opposite sense moves it down (never below 0) when bOppositeTightens; a hit without a sense, or any other damage, leaves the byte
 * alone. Turning ignores hardness (the screw's Hardness still decides ricochets), the PartCap and the part's damage scale: the byte at
 * 255 breaks the part like any part, with its own OnBreak. Minions can carry screws too (their row has the same byte).
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartScrewRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Screw")
	bool bEnabled = false;

	/** The turn sense that loosens the screw: -1 counter-clockwise, +1 clockwise (seen from above, like FGSTN_HitData::TurnSense). 0 is not a sense: validation rejects it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Screw", meta = (EditCondition = "bEnabled", ClampMin = "-1", ClampMax = "1"))
	int32 LoosenSense = -1;

	/** Steps (hit TurnSteps) that take the screw from fully in to out, 1..255. A minion screw is a one-hit screw with 1 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Screw", meta = (EditCondition = "bEnabled", ClampMin = "1", ClampMax = "255"))
	int32 TurnsToRelease = 8;

	/** A hit turning the other way screws it back in (never below fully in) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Screw", meta = (EditCondition = "bEnabled"))
	bool bOppositeTightens = true;

	/** The damage bytes of Steps steps: ceil(255 x Steps / TurnsToRelease), integers only. At least 1 for a positive step count, so a turn always moves the byte */
	int32 StepBytes(int32 Steps) const
	{
		const int32 Turns = FMath::Max(TurnsToRelease, 1);
		const int32 Clamped = FMath::Clamp(Steps, 0, 255);
		return (255 * Clamped + Turns - 1) / Turns;
	}
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
	 * Per-hurtbox hardness inside this one part (combat-parts.md §3.4, the charger's abdomen: medium on top, light underneath), keyed
	 * by a name in Hurtboxes, 0..15. It wins over Hardness for a hit the attacker landed on that hurtbox; the part keeps one damage
	 * pool, one PartCap and one break, so breaking it from either side breaks the whole part. Static data, so the attacker's ricochet
	 * prediction stays exact. Empty (the default) = Hardness for every hurtbox, at the cost of one emptiness test per hit
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TMap<FName, int32> HurtboxHardnessOverride;

	/** The hardness a hit on HurtboxName meets: its override if any, else Hardness. No allocation (a map lookup only when overrides exist) */
	int32 GetHardnessFor(FName HurtboxName) const
	{
		if (HurtboxHardnessOverride.IsEmpty())
		{
			return Hardness;
		}
		const int32* Found = HurtboxHardnessOverride.Find(HurtboxName);
		return Found ? *Found : Hardness;
	}

	/**
	 * Wounds (combat-parts.md 3.4): the hardness this part has once a hit that carries a wound (FGSTN_PartBreakEffect_Wound) has struck it,
	 * 0..15, replacing Hardness and the hurtbox overrides while the part's bit of the wounded mask is on. Negative (the default) = the part
	 * cannot be wounded and a wound hit leaves no mark on it. Static data, so the attacker's ricochet prediction stays exact. Not for minions
	 * (their row has no wounded mask): validation rejects it on a minion set
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = -1, ClampMax = 15))
	int32 WoundedHardness = -1;

	/** Whether a wound hit can mark this part */
	bool CanBeWounded() const { return WoundedHardness >= 0; }

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

	/** Screw or bolt behaviour: hits with the right TurnSense unscrew this part (combat-parts.md 3.4). Off by default */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Screw")
	FGSTN_PartScrewRule Screw;

	/**
	 * Names of the parts that hold this one (screws, a latch). The part comes off when HeldByRule is met OR when its own damage breaks it,
	 * whichever comes first (owner, 2026-11-08: make an indestructible plate with its data, Hardness / PartCap <= 0). When the rule is met, in
	 * the same pass that breaks children, the part breaks whatever its Hardness and even with a PartCap of 0 or less, running its own OnBreak.
	 * Empty = held by nothing. ParentIndex stays the part's own parent
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Screw")
	TArray<FName> HeldBy;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Screw", meta = (EditCondition = "!HeldBy.IsEmpty()"))
	EGSTN_HeldByRule HeldByRule = EGSTN_HeldByRule::AllBroken;

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
