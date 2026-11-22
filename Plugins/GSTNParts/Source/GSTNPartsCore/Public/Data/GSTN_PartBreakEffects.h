// Destructible parts, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GSTN_PartBreakEffects.generated.h"

struct FGSTN_PartsProjection;
struct FGSTN_PartsState;

/**
 * Base of what happens to an entity when one of its parts is broken (combat-parts.md §3.3). A derived struct holds the tuning
 * of one effect; the entries live in FGSTN_PartDefinition::OnBreak as FInstancedStruct, so a part set gains an effect by data
 * and a plugin adds a kind by deriving, with no change to this module. An entry whose struct is not loaded (its plugin is
 * disabled) is an empty instanced struct: it is kept in the asset, ignored when read, and reported by validation as a warning.
 *
 * Two kinds, by rollback behaviour. Derived effects (Tags, DisableHurtbox, DisableHitGroups) are functions of the broken mask,
 * recomputed every frame, so nothing is replayed. One-shot effects (Stats, MainDamage, Launch, Stagger) are requests applied
 * once at the break frame by the outcome applier, with the rollback guarantees of the damage effect. An effect never reads a
 * live component: the simulation passes it the part state and it writes into a projection or a request.
 */
USTRUCT(BlueprintType, meta = (Hidden))
struct GSTNPARTSCORE_API FGSTN_PartBreakEffect
{
	GENERATED_BODY()

	virtual ~FGSTN_PartBreakEffect() = default;

	/**
	 * Derived effects only: adds this effect's projection for the broken part PartIndex. Called every frame for every broken
	 * part, in part index order, so it must be a pure function of its own data and the state: no component, world or random
	 * reads, no memory kept between calls. One-shot effects leave it empty; the outcome applier handles them
	 */
	virtual void Project(const FGSTN_PartsState& State, int32 PartIndex, FGSTN_PartsProjection& Out) const {}
};

/**
 * Derived. Tags projected on the entity while the part is broken (Combat.Part.Effect.* leaves, designer lock tags of a lost
 * arm). Removed suppresses a tag of the definition's base tags (Combat.Immunity.Grab) while broken. A projection, not an
 * effect: no counted-tag bookkeeping, so a rollback has nothing to undo.
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartBreakEffect_Tags : public FGSTN_PartBreakEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Effect")
	FGameplayTagContainer Granted;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Effect")
	FGameplayTagContainer Removed;

	virtual void Project(const FGSTN_PartsState& State, int32 PartIndex, FGSTN_PartsProjection& Out) const override;
};

/** Derived. Names of hurtboxes the snapshot hides while the part is broken; overrides the part's own broken-hurtbox policy */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartBreakEffect_DisableHurtbox : public FGSTN_PartBreakEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Effect")
	TArray<FName> Hurtboxes;

	virtual void Project(const FGSTN_PartsState& State, int32 PartIndex, FGSTN_PartsProjection& Out) const override;
};

/** Derived. The entity's own hitboxes in these hit groups stop being generated while the part is broken: a lost arm cannot punch */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartBreakEffect_DisableHitGroups : public FGSTN_PartBreakEffect
{
	GENERATED_BODY()

	/** Bit n disables hit group n of the entity's moves */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Effect", meta = (Bitmask))
	uint8 GroupMask = 0;

	virtual void Project(const FGSTN_PartsState& State, int32 PartIndex, FGSTN_PartsProjection& Out) const override;
};

/** One magnitude of a Stats break effect: the SetByCaller key and its value */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartStatMagnitude
{
	GENERATED_BODY()

	/** The SetByCaller stat key the effect is applied with (walk speed, weight, a resistance) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Effect", meta = (Categories = "Combat.SetByCaller.Stat"))
	FGameplayTag Stat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Effect")
	float Magnitude = 0.f;
};

/**
 * One-shot. Applied by the GAS module as an infinite effect whose magnitudes are the SetByCaller values below (the way weapon
 * stats are applied), so the lost leg slows the entity until the effect is removed. Declared here as data; applying it needs the
 * ability system, so it lives in GSTNPartsGAS.
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartBreakEffect_Stats : public FGSTN_PartBreakEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Effect")
	TArray<FGSTN_PartStatMagnitude> Magnitudes;
};

/**
 * One-shot. Bonus main damage at the break (a head break adds 30 %, or kills). Counted once per newly broken part, in part
 * index order, in the same hit's main damage: the applier must not add it a second time.
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartBreakEffect_MainDamage : public FGSTN_PartBreakEffect
{
	GENERATED_BODY()

	/** Percent points added to the main pool, before the entity cap clamps it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Effect", meta = (ClampMin = 0))
	float AddPercent = 0.f;

	/** Breaking the part destroys the entity regardless of the main pool */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Effect")
	bool bKill = false;
};

/**
 * One-shot. An optional impulse on the entity, the recoil of the break, applied through the same knockback layered move as a
 * hit. The spec leaves its fields open: speed and how much of it points along the entity's up (gravity-relative, never world
 * Z); the horizontal part follows the hit that broke the part.
 */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartBreakEffect_Launch : public FGSTN_PartBreakEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Effect", meta = (ClampMin = 0, ForceUnits = "cm/s"))
	float Speed = 0.f;

	/** 0 = purely horizontal, 1 = purely along up */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Effect", meta = (ClampMin = 0, ClampMax = 1))
	float UpFraction = 0.5f;
};

/** One-shot. Forces at least this much hitstun at the break (a lost leg buckles) */
USTRUCT(BlueprintType)
struct GSTNPARTSCORE_API FGSTN_PartBreakEffect_Stagger : public FGSTN_PartBreakEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break Effect", meta = (ClampMin = 0, ForceUnits = "Frames"))
	int32 Frames = 0;
};
