// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "StructUtils/InstancedStruct.h"
#include "Hits/GSTN_ImpactTypes.h"
#include "GSTN_HitData.generated.h"

/** How the launch direction is turned to the victim on the movement plane */
UENUM(BlueprintType)
enum class EGSTN_YawSource : uint8
{
	/** The attacker's facing at the hit (the default) */
	AttackerFacing,
	/** From the hitbox centre to the victim */
	HitboxToVictim,
	/** Along the hitbox's velocity */
	HitboxVelocity
};

/** Special launch angles (resolved by the launch direction math) */
UENUM(BlueprintType)
enum class EGSTN_SpecialAngle : uint8
{
	None,
	/** Low angle on grounded victims below a knockback threshold, higher in the air */
	Sakurai3D,
	/** Towards the hitbox's velocity */
	AutoLink
};

/** Where the hitstop frames come from */
UENUM(BlueprintType)
enum class EGSTN_HitstopMode : uint8
{
	/** From the hit's damage and knockback by the rules' hitstop formula, then the multipliers */
	Auto,
	/** The frame counts below are final */
	Override
};

/** Where the stagger force of the attack comes from */
UENUM(BlueprintType)
enum class EGSTN_StaggerForceMode : uint8
{
	/** From the authored damage and knockback by the rules' formula */
	Derived,
	/** StaggerForce replaces the formula */
	Override
};

/**
 * The attack side of one hit (combat-m4-hits 3). Everything here is authored; nothing is derived from the victim. Plain data,
 * stored per hitbox and resolved per relationship by ApplyVariant before the pipeline reads it.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_HitData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Damage", meta = (ClampMin = "0.0"))
	float Damage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Knockback", meta = (ClampMin = "0.0"))
	float BaseKnockback = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Knockback", meta = (ClampMin = "0.0"))
	float KnockbackGrowth = 0.f;

	/** At or above 0 replaces the base + growth bracket; negative = unused */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Knockback")
	float FixedKnockback = -1.f;

	/** Elevation of the launch above the movement plane */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Knockback", meta = (ClampMin = "-90.0", ClampMax = "90.0", ForceUnits = "deg"))
	float AngleDeg = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Knockback")
	EGSTN_SpecialAngle SpecialAngle = EGSTN_SpecialAngle::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Knockback")
	EGSTN_YawSource YawSource = EGSTN_YawSource::AttackerFacing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Hitstun", meta = (ClampMin = "0.0"))
	float HitstunMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Hitstop")
	EGSTN_HitstopMode HitstopMode = EGSTN_HitstopMode::Auto;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Hitstop", meta = (EditCondition = "HitstopMode == EGSTN_HitstopMode::Auto", ClampMin = "0.0"))
	float HitstopMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Hitstop", meta = (EditCondition = "HitstopMode == EGSTN_HitstopMode::Auto", ClampMin = "0.0"))
	float AttackerHitstopMultiplier = 1.f;

	/** The victim's hitstop with HitstopMode Override */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Hitstop", meta = (EditCondition = "HitstopMode == EGSTN_HitstopMode::Override", ClampMin = "0", ForceUnits = "Frames"))
	int32 HitstopFramesOverride = 0;

	/** The attacker's hitstop with HitstopMode Override; -1 = the same as HitstopFramesOverride */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Hitstop", meta = (EditCondition = "HitstopMode == EGSTN_HitstopMode::Override", ClampMin = "-1", ForceUnits = "Frames"))
	int32 AttackerHitstopFramesOverride = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Shield", meta = (ClampMin = "0.0"))
	float ShieldDamage = 0.f;

	/** Among the hitboxes of one group that overlap the same victim in one frame, the highest wins (ties: lower array index) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	int32 Priority = 0;

	/** Invalid = Normal. Anything else is data only in M4 (no pipeline path yet); the timeline validator warns */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (Categories = "Combat.HitType"))
	FGameplayTag HitType;

	/**
	 * Gameplay effect classes applied to the victim, kept as soft UObject classes because Core has no GameplayAbilities
	 * dependency: the GAS layer resolves them and rejects anything that is not a gameplay effect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	TArray<TSoftClassPtr<UObject>> ExtraEffects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (Categories = "GameplayCue"))
	FGameplayTag CueTag;

	/** Game-specific payload the core carries and never reads */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit")
	FInstancedStruct CustomData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Stagger")
	EGSTN_StaggerForceMode StaggerForceMode = EGSTN_StaggerForceMode::Derived;

	/** The stagger force with StaggerForceMode Override */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Stagger", meta = (EditCondition = "StaggerForceMode == EGSTN_StaggerForceMode::Override", ClampMin = "0.0"))
	float StaggerForce = 0.f;

	/** Multiplies the stagger force either way (a relationship variant can halve it) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Stagger", meta = (ClampMin = "0.0"))
	float StaggerForceScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit|Impact")
	FGSTN_ImpactProfile Impact;

	/** Data of optional plugins, each entry a struct derived from FGSTN_HitExtension. Empty by default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hit", meta = (BaseStruct = "/Script/GSTNCombatCore.GSTN_HitExtension", ExcludeBaseStruct))
	TArray<FInstancedStruct> ExtensionData;

	/** True for an unset type or Combat.HitType.Normal */
	bool IsNormalHitType() const;

	/** The attacker's hitstop override once -1 is resolved */
	int32 GetAttackerHitstopFramesOverride() const { return AttackerHitstopFramesOverride >= 0 ? AttackerHitstopFramesOverride : HitstopFramesOverride; }
};

/** The fields of FGSTN_HitData a variant can replace (bit mask of FGSTN_HitDataOverride::Fields) */
UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EGSTN_HitDataField : uint32
{
	None = 0,
	Damage = 1 << 0,
	BaseKnockback = 1 << 1,
	KnockbackGrowth = 1 << 2,
	FixedKnockback = 1 << 3,
	AngleDeg = 1 << 4,
	SpecialAngle = 1 << 5,
	YawSource = 1 << 6,
	HitstunMultiplier = 1 << 7,
	HitstopMode = 1 << 8,
	HitstopMultiplier = 1 << 9,
	AttackerHitstopMultiplier = 1 << 10,
	HitstopFramesOverride = 1 << 11,
	AttackerHitstopFramesOverride = 1 << 12,
	ShieldDamage = 1 << 13,
	Priority = 1 << 14,
	HitType = 1 << 15,
	ExtraEffects = 1 << 16,
	CueTag = 1 << 17,
	StaggerForceMode = 1 << 18,
	StaggerForce = 1 << 19,
	StaggerForceScale = 1 << 20,
	Impact = 1 << 21
};
ENUM_CLASS_FLAGS(EGSTN_HitDataField);

/**
 * What a relationship variant changes in a hit: the fields named by Fields take their value from Values, then the multipliers
 * scale the result, then ExtensionOverrides replace the extension entries of the same struct type. Everything else keeps the
 * hitbox's data.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_HitDataOverride
{
	GENERATED_BODY()

	/** Which fields of Values replace the hit's (EGSTN_HitDataField) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override", meta = (Bitmask, BitmaskEnum = "/Script/GSTNCombatCore.EGSTN_HitDataField"))
	int32 Fields = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
	FGSTN_HitData Values;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override|Multipliers", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.f;

	/** Scales BaseKnockback, KnockbackGrowth and FixedKnockback (when used) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override|Multipliers", meta = (ClampMin = "0.0"))
	float KnockbackMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override|Multipliers", meta = (ClampMin = "0.0"))
	float HitstunMultiplier = 1.f;

	/** Scales HitstopMultiplier and AttackerHitstopMultiplier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override|Multipliers", meta = (ClampMin = "0.0"))
	float HitstopMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override|Multipliers", meta = (ClampMin = "0.0"))
	float ShieldDamageMultiplier = 1.f;

	/** Scales StaggerForceScale */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override|Multipliers", meta = (ClampMin = "0.0"))
	float StaggerForceMultiplier = 1.f;

	/**
	 * Replace the extension entry of the same struct type, or append when the hit has none. Per struct type, not per field:
	 * the core does not know an extension's fields.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override", meta = (BaseStruct = "/Script/GSTNCombatCore.GSTN_HitExtension", ExcludeBaseStruct))
	TArray<FInstancedStruct> ExtensionOverrides;

	bool Overrides(EGSTN_HitDataField Field) const { return (Fields & static_cast<int32>(Field)) != 0; }
};

/** How a hitbox treats a victim of one relationship (Self, Ally, Enemy, Neutral) */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_HitVariant
{
	GENERATED_BODY()

	/** The hitbox does not hit victims of this relationship */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variant")
	bool bIgnore = false;

	/** Scales the hitbox's shape for victims of this relationship (a bigger net for allies of a healing move) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variant", meta = (ClampMin = "0.0"))
	float ShapeScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Variant")
	FGSTN_HitDataOverride Override;
};

/** Stateless helpers over hit data */
UCLASS()
class GSTNCOMBATCORE_API UGSTN_HitDataUtils : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * The hit data after a variant: override fields, then multipliers (a negative FixedKnockback stays unused), then extension
	 * entries replaced by struct type. Pure and deterministic: the same inputs give the same bits.
	 */
	UFUNCTION(BlueprintPure, Category = "GSTN|Hits")
	static FGSTN_HitData ApplyVariant(const FGSTN_HitData& Hit, const FGSTN_HitVariant& Variant);
};
