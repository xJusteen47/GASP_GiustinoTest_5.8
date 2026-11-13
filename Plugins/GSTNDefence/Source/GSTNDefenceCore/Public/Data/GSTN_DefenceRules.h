// Defence, pure part (no GAS, no Network Prediction, no Mover).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Data/GSTN_Curve1D.h"
#include "Hits/GSTN_HitMathUtils.h"
#include "GSTN_DefenceRules.generated.h"

/** Limits the wire form of the defence state relies on */
namespace GSTNDefenceLimits
{
	/** Parry profiles one rules asset may hold: the profile index travels as a small integer in the snapshot, never as a tag or a name */
	constexpr int32 MaxParryProfiles = 16;
}

/**
 * Where the shield's perfect-shield window is anchored (m6 spec 5.4). Release is Ultimate's: the window opens when the shield is
 * dropped. Press is the older powershield: the window opens when the shield is raised.
 */
UENUM(BlueprintType)
enum class EGSTN_ParryAnchor : uint8
{
	Release,
	Press
};

/** The tech kinds a tech set may offer; EvaluateTech picks one from the stick and the contact normal */
UENUM(BlueprintType)
enum class EGSTN_TechType : uint8
{
	None,
	InPlace,
	RollLeft,
	RollRight,
	WallTech,
	WallJumpTech
};

/**
 * The shield bubble: a sphere of FIXED size around the body, as in Rivals of Aether 2 (owner decision 2026-10-26). There is no
 * stick tilt, no shrink with the shield's health, no arc and no poke: the shield's health only decides when it breaks and how
 * hard a blocked hit stuns and pushes. Frames are fixed-tick frames (62.5 Hz).
 */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_ShieldRules
{
	GENERATED_BODY()

	// ---- Bubble ----

	/** Radius of the bubble at any shield value; the default covers the capsule with a margin */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Bubble", meta = (ClampMin = "1.0", ForceUnits = "cm"))
	float RadiusCm = 80.f;

	/** Height of the bubble's centre above the entity root, along the entity's local up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Bubble", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float CentreHeightCm = 90.f;

	// ---- Timing ----

	/** Frames from the press until the bubble blocks (the bubble exists from the press frame) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Timing", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 StartupFrames = 1;

	/** Frames the bubble outlasts the release */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Timing", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ReleaseFrames = 6;

	// ---- Health ----

	/** Shield health at full; the default of the attribute set's maximum */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Health", meta = (ClampMin = "1.0"))
	float MaxShield = 50.f;

	/** Health lost per frame while the shield is held (about 5 s for a full hold at the default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Health", meta = (ClampMin = "0.0"))
	float DrainPerFrame = 0.16f;

	/** Health regained per frame after the regeneration delay (about half the drain) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Health", meta = (ClampMin = "0.0"))
	float RegenPerFrame = 0.08f;

	/** Frames after the release before the health regenerates */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Health", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 RegenDelayFrames = 30;

	/** Shield damage a blocked hit deals per point of its damage, when the hit data says none */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Health", meta = (ClampMin = "0.0"))
	float ShieldDamagePerDamage = 1.f;

	/** Fraction of the maximum the health returns to when the break ends */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Break", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RecoverFractionAfterBreak = 0.6f;

	/** Frames of the long stun after a break */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Break", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 BreakStunFrames = 150;

	/** Speed of the upward launch of a break, along the entity's local up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Break", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float BreakLaunch = 900.f;

	// ---- Reaction to a blocked hit ----

	/** Shield stun of a hit with no damage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Reaction", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 ShieldStunBase = 3;

	/** Shield stun frames added per point of the hit's damage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Reaction", meta = (ClampMin = "0.0"))
	float ShieldStunPerDamage = 0.5f;

	/** Upper limit of the shield stun */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Reaction", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 MaxShieldStun = 30;

	/** Scale of the hitstop of a blocked hit against the knockback model's hitstop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Reaction", meta = (ClampMin = "0.0"))
	float ShieldHitstopMultiplier = 1.f;

	/** Pushback speed of a blocked hit with no damage, away from the attacker, no tumble */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Reaction", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float PushbackSpeed = 150.f;

	/** Pushback speed added per point of the hit's damage */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Reaction", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float PushbackSpeedPerDamage = 15.f;

	/** Frames the pushback lasts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Reaction", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 PushbackFrames = 8;

	/** Scale of the shield damage by the relationship of the attacker (for modes that also field minions or hits of the environment). Missing = 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shield|Reaction", meta = (Categories = "Combat.Relationship"))
	TMap<FGameplayTag, float> IncomingShieldDamageScaleByRelationship;
};

/** The shield's perfect shield: when the parry window of the shield opens (m6 spec 5.4, source Shield) */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_ParryRules
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parry")
	EGSTN_ParryAnchor Anchor = EGSTN_ParryAnchor::Release;

	/** Length of the window from the anchor frame */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parry", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 WindowFrames = 5;

	/** With the Release anchor, the shield must have been held at least this long for the release to parry (anti-tap knob) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parry", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 MinHeldFramesBeforeRelease = 0;

	/** The profile the shield's parry gives, an index into UGSTN_DefenceRules::ParryProfiles (0 by convention) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parry", meta = (ClampMin = "0"))
	int32 ProfileIndex = 0;
};

/** What a parry gives, shared by both parry sources (the shield's and a move's window) */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_ParryProfile
{
	GENERATED_BODY()

	/** What a ParryWindow track names; empty is allowed for profile 0 only (the shield's), and names must be unique */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parry")
	FName Name = NAME_None;

	/** Hitstop of both sides */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parry", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 HitstopFrames = 8;

	/** Recovery of the defender after a shield-source parry (a move keeps playing); the defender may cancel into any command after it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parry", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 DefenderRecoveryFrames = 4;

	/** Stun of the punished attacker when no hit class entry matches; 0 is Ultimate's defender advantage only */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parry", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 AttackerStunFrames = 20;

	/** Stun of the punished attacker by hit class (the hit's Combat.HitType.* tag, matched hierarchically) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parry", meta = (Categories = "Combat.HitType"))
	TMap<FGameplayTag, int32> AttackerStunFramesByHitClass;

	/** Whether projectiles can be parried at all */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parry")
	bool bParryableProjectiles = true;

	/** Reserved: the projectile is sent back. Not implemented in this version */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parry")
	bool bReflectProjectiles = false;

	/** Meter the defender gains */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parry", meta = (ClampMin = "0.0"))
	float MeterGain = 5.f;
};

/** Rules of the optional tech (m6 spec 6). Tech is a pluggable kit ability: these values matter only for a kit whose tech set is not empty */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_TechRules
{
	GENERATED_BODY()

	/** A press up to this many frames before the contact counts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 WindowFrames = 20;

	/** After a press further presses are ignored for this long (anti-mash); a press during it does not extend it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 LockoutFrames = 40;

	/** A press up to this many frames after the contact still counts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 LateFrames = 0;

	/** Whether presses during hitstun that is not a tumble count too (false: they are ignored and start no lockout) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech")
	bool bTechInHitstunNotTumble = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech|Moves", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 InPlaceFrames = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech|Moves", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 RollFrames = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech|Moves", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float RollDistance = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech|Moves", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 WallTechFrames = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech|Moves")
	bool bAllowWallTech = true;

	/** Frames downed after a failed tech on the ground */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech|Knockdown", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 KnockdownFrames = 40;

	/** Frames added to the downed state after a failed tech, the price of not teching */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech|Knockdown", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 NoTechKnockdownBonusFrames = 10;

	/** Fallback intangibility of a tech; the tech moves' own hurtbox modifier windows win */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tech|Moves", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 IntangibleFrames = 12;
};

/** Directional influence of a launch (m6 spec 5.5): the M4 parameters plus the strength curve and the victim scales */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_DIRules
{
	GENERATED_BODY()

	/** The M4 parameters, overriding the combat rules' default per game mode */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DI")
	FGSTN_DIParams Params;

	/** Stick magnitude (0..1) to a factor on the DI strength. Empty = 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DI")
	FGSTN_Curve1D StrengthCurve;

	/** Optional: |launch elevation| (0..1) to a factor on MaxDIElevationDot, for less influence near vertical. Empty = 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DI")
	FGSTN_Curve1D ElevationDeltaByElevation;

	/** Launches below this knockback get no DI */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DI", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MinKnockbackForDI = 0.f;

	/** Scale of the DI by a tag of the victim (bosses 0): the product of every entry whose tag the victim carries */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "DI")
	TMap<FGameplayTag, float> ScaleByVictimTag;
};

/** Held-direction drift of the victim during hitstop (m6 spec 5.5): holding a direction drifts, there is no mashing */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_SDIRules
{
	GENERATED_BODY()

	/** Drift speed at the full stick */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SDI", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float DriftSpeedCmPerSec = 120.f;

	/** Stick magnitude (0..1) to a factor on the drift speed. Empty = 1 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SDI")
	FGSTN_Curve1D StrengthCurve;

	/** The total drift of one hit stops at this distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SDI", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxDistanceCmPerHit = 12.f;

	/** Below this stick magnitude (a tilt) there is no drift */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SDI", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinZone = 0.3f;

	/** Scale the drift by the hitstop length; false: the total grows with the hitstop length by itself */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SDI")
	bool bScaleByHitstopFrames = false;

	/** Scale of the drift by a tag of the victim (bosses 0): the product of every victim tag that matches */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SDI")
	TMap<FGameplayTag, float> ScaleByVictimTag;

	/** Whether an armored victim may drift */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SDI")
	bool bSDIAllowedWhenArmored = false;

	/** Whether a victim in the stagger-in-place tier may drift */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SDI")
	bool bSDIInStaggerInPlace = true;
};

/** Landing from a tumble without a tech (m6 spec 6, owner revision): a stick-steered mini bounce, then the downed state */
USTRUCT(BlueprintType)
struct GSTNDEFENCECORE_API FGSTN_GroundBounceRules
{
	GENERATED_BODY()

	/** Speed of the mini bounce along the entity's local up */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float GroundBounceSpeed = 300.f;

	/** The stick at the contact frame steers the bounce's direction by up to this angle off the vertical */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0.0", ClampMax = "90.0", ForceUnits = "deg"))
	float GroundBounceSteerDeg = 30.f;

	/** Frames after which a downed entity gets up by itself; 0 = never (the default) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Landing", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 MaxDownedFrames = 0;
};

/**
 * The defence data of a game mode: one of each rule struct and the parry profiles. Referenced by the defence extension of the
 * combat rules asset (FGSTN_DefenceRulesExtension) and, per kit, by FGSTN_DefenceKitExtension, so two fighters with different
 * shields resolve differently in the same pipeline (m6 spec 1d). Soft references only; the simulation resolves it before the
 * pawn's first tick and every machine reads the same asset.
 *
 * bShieldEnabled is the PvP/PvE switch: false means no shield exists in the mode (no binding, no bubble, no state), while the
 * parry of a move's window works in every mode.
 */
UCLASS(BlueprintType)
class GSTNDEFENCECORE_API UGSTN_DefenceRules : public UDataAsset
{
	GENERATED_BODY()

public:
	UGSTN_DefenceRules();

	/** The mode enables the shield (PvP yes, PvE no). A kit also needs a shield of its own: see UGSTN_DefenceMath::IsShieldAvailable */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence")
	bool bShieldEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence")
	FGSTN_ShieldRules Shield;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence")
	FGSTN_ParryRules Parry;

	/** Profile 0 is the shield's; the parry window track of a move names another by FGSTN_ParryProfile::Name. Never empty */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence")
	TArray<FGSTN_ParryProfile> ParryProfiles;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence")
	FGSTN_TechRules Tech;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence")
	FGSTN_DIRules DI;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence")
	FGSTN_SDIRules SDI;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defence")
	FGSTN_GroundBounceRules GroundBounce;

	/**
	 * The index of the profile with this name. An empty name is profile 0 (the shield's), an unknown name is INDEX_NONE: a track
	 * that names a profile that does not exist is reported by validation, never silently mapped to 0.
	 */
	int32 FindProfileIndex(FName ProfileName) const;

	/** Appends one message per problem in the rules; empty when the asset is valid. Pure, so the editor and the tests share it */
	void Validate(TArray<FString>& OutErrors) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
