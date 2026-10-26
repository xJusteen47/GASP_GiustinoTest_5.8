// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Hits/GSTN_HitData.h"
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
 * once, so the model stays one pure function. Every default is the identity: unextended content is unchanged. The model input and
 * result are below.
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
 * A curve baked to integers so every machine reads the same number: 64 uint16 samples over [0, 1] in Q2.14 fixed point (16384
 * is 1.0, so a value up to about 4 fits), integer interpolation between neighbours. The input is quantised to 1/4096 first.
 * Empty = not authored: the next source applies, and evaluating it is the identity (1.0).
 */
USTRUCT()
struct GSTNCOMBATCORE_API FGSTN_BakedCurve
{
	GENERATED_BODY()

	static constexpr int32 SampleCount = 64;

	/** The fixed point value of 1.0 */
	static constexpr int32 One = 1 << 14;

	/** Not exposed to Blueprint (it has no 16 bit integer): the baking and the evaluation are native */
	UPROPERTY(EditAnywhere, Category = "Curve")
	TArray<uint16> Samples;

	bool IsAuthored() const { return Samples.Num() == SampleCount; }

	/** True when authored and every sample is equal (the curve changes nothing along the way) */
	bool IsFlat() const;

	/** Bakes a function of [0, 1] (the editor's or a test's), rounding to the fixed point; values are clamped to the representable range */
	void Bake(TFunctionRef<double(double)> Function);

	/** A constant curve */
	void BakeConstant(double Value);

	/** A straight line from the value at 0 to the value at 1 */
	void BakeLinear(double AtZero, double AtOne);

	/** The fixed point value (One = 1.0) at T in [0, 1], clamped; integer arithmetic after the quantisation of T. 'One' when not authored */
	int32 EvaluateFixed(double T) const;

	/** EvaluateFixed as a float */
	float Evaluate(double T) const { return static_cast<float>(EvaluateFixed(T)) / static_cast<float>(One); }
};

/** Bit flags of FGSTN_VictimReactionState::Flags */
namespace GSTNVictimReactionFlags
{
	constexpr uint8 Armor = 1 << 0;
	constexpr uint8 Intangible = 1 << 1;
	constexpr uint8 Invincible = 1 << 2;
	constexpr uint8 InHitstop = 1 << 3;
	constexpr uint8 Tumble = 1 << 4;
}

/**
 * What an attacker's machine needs to know about a victim to compute the same hitstop the authority computes (combat-m4-hits 3,
 * rule 3): a few bytes of replicated state, quantised. Plain old data, trivially copyable, never a pointer or a tag, so it can
 * sit in a snapshot ring and be rewound with the transforms.
 */
struct GSTNCOMBATCORE_API FGSTN_VictimReactionState
{
	/** Damage percent in tenths (0.1 resolution, up to 6553.5) */
	uint16 DamagePercentQ = 0;

	/** Weight scale in 1/64 (up to about 4) */
	uint8 WeightScaleQ = 64;

	/** GSTNVictimReactionFlags */
	uint8 Flags = 0;

	/** Bit mask the optional plugins define on the snapshot extension (broken parts...); the core never reads it */
	uint8 ExtMask = 0;

	/** The threshold currently in effect for the victim, in whole stagger force units (an Armor window raises it). Unset = no threshold */
	FGSTN_StaggerThreshold EffectiveThresholdQ;

	static FGSTN_VictimReactionState Make(double DamagePercent, double WeightScale, uint8 InFlags, const FGSTN_StaggerThreshold& Threshold, uint8 InExtMask = 0);

	float GetDamagePercent() const { return static_cast<float>(DamagePercentQ) * 0.1f; }
	float GetWeightScale() const { return static_cast<float>(WeightScaleQ) / 64.f; }
	bool HasFlag(uint8 Flag) const { return (Flags & Flag) != 0; }
};
static_assert(TIsTriviallyCopyConstructible<FGSTN_VictimReactionState>::Value, "The reaction state is copied byte for byte into snapshots");

/**
 * The constants every knockback model shares (combat-m4-hits 3): the stagger force formula, the hitstop formula, the reaction
 * tier thresholds, the hitstun length and the push back speed. Kept in one struct on the abstract model so a game tunes them
 * in one place. The defaults are neutral starting points (the standard rules asset sets the real ones); both tier thresholds
 * default to 0, which makes every staggered hit a Launch, the behaviour before the tiers existed.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ReactionTuning
{
	GENERATED_BODY()

	/** Stagger force per point of the hit's damage (Derived stagger force) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Stagger", meta = (ClampMin = "0.0"))
	float StaggerPerDamage = 1.f;

	/** Stagger force per cm/s of the hit's authored knockback (Derived stagger force) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Stagger", meta = (ClampMin = "0.0"))
	float StaggerPerKnockback = 0.f;

	/** Auto hitstop frames per point of damage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Hitstop", meta = (ClampMin = "0.0"))
	float HitstopPerDamage = 0.3f;

	/** Auto hitstop frames per cm/s of knockback; 0 keeps the damage-only hitstop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Hitstop", meta = (ClampMin = "0.0"))
	float HitstopPerKnockback = 0.f;

	/** Auto hitstop frames added to every hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Hitstop", meta = (ClampMin = "0.0", ForceUnits = "Frames"))
	float HitstopBase = 3.f;

	/** Upper limit of every hitstop, Auto and Override alike */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Hitstop", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 MaxHitstop = 20;

	/** Below this knockback a staggered hit stays in place; 0 (with LaunchThreshold 0) = every staggered hit launches */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Tiers", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float PushBackThreshold = 0.f;

	/** Below this knockback a staggered hit pushes back; at or above it launches */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Tiers", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float LaunchThreshold = 0.f;

	/** A launch at or above this knockback tumbles */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Tiers", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float TumbleThreshold = 1800.f;

	/** Hitstun frames per cm/s of knockback */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Hitstun", meta = (ClampMin = "0.0"))
	float HitstunFactor = 0.02f;

	/** Shortest hitstun of a staggered hit (keeps a stagger in place readable) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Hitstun", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 MinHitstunFrames = 6;

	/** Push back speed per cm/s of knockback */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Push back", meta = (ClampMin = "0.0"))
	float PushBackSpeedScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Push back", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MaxPushBackSpeed = 600.f;

	/** The weight at which the weight driven knockback equals the authored bracket */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Weight", meta = (ClampMin = "0.001"))
	float WeightReference = 100.f;

	/** Floor of the effective weight (a victim never becomes weightless) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reaction|Weight", meta = (ClampMin = "0.001"))
	float MinWeight = 1.f;
};

/**
 * Everything a knockback model reads (combat-m4-hits 3): the attack's data, the victim as the snapshot shows it and the
 * modifiers the steps wrote. Nothing else: no world, no actor, no component.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_KnockbackInput
{
	GENERATED_BODY()

	/** The hit after the relationship variant */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback")
	FGSTN_HitData Hit;

	/** Unit direction of the launch (H3 builds it); zero = no direction */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback")
	FVector LaunchDirection = FVector::ZeroVector;

	/** The victim's local up (minus its gravity direction of the current frame), never world Z. Used to keep a push back horizontal */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback")
	FVector Up = FVector::UpVector;

	/** The victim's weight before this hit's modifiers */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback", meta = (ClampMin = "0.0"))
	float Weight = 100.f;

	/** The damage taken before this hit, in percent (the percent models read it; this hit's damage is not in it) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback", meta = (ClampMin = "0.0"))
	float DamagePercent = 0.f;

	/** 1 = full health. Derived by the pipeline; the standard models do not read it, game models may */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HealthFraction = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback")
	FGSTN_HitReactionModifiers Reaction;
};

/** What a knockback model decides about one hit on one victim */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_KnockbackResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback")
	EGSTN_HitReaction Reaction = EGSTN_HitReaction::StaggerInPlace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback")
	bool bStaggered = false;

	/** The attack's stagger force, before the victim's StaggerScale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback")
	float StaggerForce = 0.f;

	/** The knockback magnitude after KnockbackScale (what the tiers compare); the velocity is derived from it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback")
	float Knockback = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback")
	FVector LaunchVelocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback")
	int32 HitstunFrames = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback")
	bool bTumble = false;

	/** The victim's hitstop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback")
	int32 HitstopFrames = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Knockback")
	int32 AttackerHitstopFrames = 0;
};
