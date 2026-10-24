// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GSTN_ReactionTypes.generated.h"

/**
 * How far a hit moves the victim (combat-m4-hits 3). Ordered: a MaxReaction caps the tier a victim can be pushed to, so a heavy
 * enemy or an armoured part that can never fly is `PushBack`.
 */
UENUM(BlueprintType)
enum class EGSTN_HitReaction : uint8
{
	/** Hitstun with no displacement */
	StaggerInPlace,
	/** A horizontal slide on the ground, no airborne state */
	PushBack,
	/** Launched, tumble above the tumble threshold */
	Launch
};

/**
 * A stagger resistance: a number, or "never staggers" (super armor). Stored as a flag and not as a float sentinel so tuning
 * UIs and validators can show it plainly. A negative Value means "unset" (the next source in the chain decides); a context no
 * step touched therefore staggers on every hit, which is the identity.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_StaggerThreshold
{
	GENERATED_BODY()

	/** In stagger force units. Negative = unset */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stagger", meta = (EditCondition = "!bInfinite", ClampMin = "-1.0"))
	float Value = -1.f;

	/** Infinite stagger resistance: no hit ever staggers, damage and hitstop still apply */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stagger")
	bool bInfinite = false;

	bool IsSet() const { return bInfinite || Value >= 0.f; }

	/** The threshold a stagger test compares against: unset is 0 (every hit staggers), infinite is no finite number (check bInfinite first) */
	float GetFiniteOrZero() const { return bInfinite ? 0.f : FMath::Max(Value, 0.f); }

	/** The stronger of two resistances: infinite beats a number, an unset one loses to a set one */
	static FGSTN_StaggerThreshold Max(const FGSTN_StaggerThreshold& A, const FGSTN_StaggerThreshold& B)
	{
		if (A.bInfinite || B.bInfinite)
		{
			FGSTN_StaggerThreshold Result;
			Result.bInfinite = true;
			return Result;
		}
		FGSTN_StaggerThreshold Result;
		Result.Value = FMath::Max(A.Value, B.Value);
		return Result;
	}
};

/**
 * What a victim-side step may change about how one hit is resolved. Written by steps, read by the knockback model exactly
 * once, so the model stays one pure function. Every default is the identity: unextended content is unchanged. H2 adds the
 * model input and result around it.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_HitReactionModifiers
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction", meta = (ClampMin = "0.0"))
	float WeightScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction", meta = (ClampMin = "0.0"))
	float KnockbackScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction", meta = (ClampMin = "0.0"))
	float HitstunScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction", meta = (ClampMin = "0.0"))
	float HitstopScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction", meta = (ClampMin = "0.0"))
	float StaggerScale = 1.f;

	/** Unset = the next source decides (hurtbox, then the entity default, then the rules) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	FGSTN_StaggerThreshold StaggerThreshold;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	EGSTN_HitReaction MaxReaction = EGSTN_HitReaction::Launch;

	/** Damage and hitstop apply, no hitstun, launch or push (the Armor semantics) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction")
	bool bNoFlinch = false;
};

/**
 * A curve baked to integers so every machine reads the same number: 64 uint16 samples over [0, 1], integer interpolation
 * (H2 adds the evaluation and the baking). Empty = not authored, the next source applies.
 */
USTRUCT()
struct GSTNCOMBATCORE_API FGSTN_BakedCurve
{
	GENERATED_BODY()

	static constexpr int32 SampleCount = 64;

	/** Not exposed to Blueprint (it has no 16 bit integer): the baking and the evaluation are native */
	UPROPERTY(EditAnywhere, Category = "Curve")
	TArray<uint16> Samples;

	bool IsAuthored() const { return Samples.Num() == SampleCount; }
};
