// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Rules/GSTN_KitExtension.h"
#include "GSTN_ImpactTypes.generated.h"

/** What a launched victim undergoes on a surface (combat-m4-hits 7b) */
UENUM(BlueprintType)
enum class EGSTN_ImpactOutcome : uint8
{
	/** Slide or land as without impacts */
	None,
	/** Pinned to the surface for a number of frames */
	Splat,
	/** Reflected about the contact normal */
	Bounce,
	/** The surface object is destroyed, the victim continues */
	BreakSurface
};

/**
 * The attack's say in a surface impact: a finisher that forces a slam, a jab that never splats. The victim and surface layers
 * are FGSTN_ImpactResponse and FGSTN_SurfaceImpactRow; UGSTN_ImpactMath::Resolve combines the three.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ImpactProfile
{
	GENERATED_BODY()

	/** Outcome the attack asks for; None = no preference */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	EGSTN_ImpactOutcome Force = EGSTN_ImpactOutcome::None;

	/** With a Force: it applies even to a surface that allows no outcome at all (a finisher that breaks the unbreakable) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	bool bForceIgnoresSurface = false;

	/** Outcomes this attack never causes (an enum, since outcomes are not tags) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	TArray<EGSTN_ImpactOutcome> Forbid;

	/** Scales the impact speed the contact is judged with */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact", meta = (ClampMin = "0.0"))
	float SpeedScale = 1.f;

	/** Reserved for M6 (tech): the impact cannot be teched. Nothing reads it in M4 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	bool bNoTech = false;

	/**
	 * Ground landing of a victim this hit launched (owner 2026-11-16): no small "tech bounce" on the landing, the victim drops flat and lies
	 * down. The hard bounce has its own per-move switches: Force = Bounce always bounces (a spike), Forbid Bounce never does. The tech bounce's
	 * height follows the judged impact speed, so SpeedScale also scales it per move
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	bool bNoTechBounce = false;

	/** Bits of the packed form that carry the speed scale */
	static constexpr uint16 SpeedScaleUnitsPerOne = 32;

	/**
	 * The profile as the 16 bits the victim keeps in its sync state while it tumbles (combat-m4-hits 11, the profile of the hit that
	 * launched it): Force (2 bits), Forbid mask (3), bForceIgnoresSurface, bNoTech, bNoTechBounce, and the speed scale in 1/32 (8 bits, up to 7.97)
	 * stored against the default, so the default profile packs to 0 and an idle combatant pays nothing on the wire. Lossy by design:
	 * a scale is rounded to 1/32, a duplicate in Forbid is one bit; Unpack(Pack(P)) is the profile every machine decides with
	 */
	uint16 Pack() const;
	static FGSTN_ImpactProfile Unpack(uint16 Packed);
	bool IsDefault() const { return Pack() == 0; }
};

/**
 * The HARD ground bounce of a tumbling victim, the rules' part (owner 2026-11-16; UGSTN_CombatRules::GroundImpact). A victim that hits a floor
 * fast enough while its hitstun still runs bounces off it like a wall bounce (Brawlhalla / a Smash spike): still tumbling, still in hitstun.
 * Slower than that it LANDS, and the Defence plugin's landing gives the small "tech bounce" before it lies down (FGSTN_GroundBounceRules).
 * The speed is the contact's speed into the floor along its normal, times the hit's FGSTN_ImpactProfile::SpeedScale.
 *
 * Per move (the hit that launched the victim, FGSTN_ImpactProfile): Force = Bounce bounces on the first floor whatever the speed (spent
 * there); Forbid Bounce never hard-bounces (always a landing); bNoTechBounce lands flat. A surface that forces its own outcome (a trampoline)
 * is the surface's business and is not limited here.
 *
 * The chain limit stops a victim from bouncing on forever (down a slope, off ledges, from one step to the next): after MaxBounces hard
 * bounces, or BounceWindowFrames after the first one, every floor is a landing. Both count per launch: a new staggering hit starts again
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_GroundImpactRules
{
	GENERATED_BODY()

	/** A fast enough floor contact bounces by itself, without a Force on the hit. Off = only forced bounces (the behaviour before 2026-11-16) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Impact")
	bool bBounceBySpeed = true;

	/** The hard bounce needs more than this speed into the floor (cm/s, after the hit's SpeedScale); slower is a landing (tech bounce) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Impact", meta = (ClampMin = "0.0", ForceUnits = "cm/s", EditCondition = "bBounceBySpeed"))
	float BounceMinSpeed = 1200.f;

	/**
	 * Only while the hitstun still runs (Smash: a victim bounces while its knockback is live). Off: a long fall after the hitstun can bounce
	 * too. Kept on, a launch upward that comes down after its hitstun lands (tech bounce) however fast it falls
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Impact", meta = (EditCondition = "bBounceBySpeed"))
	bool bOnlyDuringHitstun = true;

	/** The fraction of the speed the bounce keeps (reflected about the floor's normal). The victim's ImpactResponse restitution wins when set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Impact", meta = (ClampMin = "0.0"))
	float Restitution = 0.6f;

	/** Hard ground bounces per launch; the next floor is a landing. 0 = no limit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Impact|Limit", meta = (ClampMin = "0", ClampMax = "15"))
	int32 MaxBounces = 2;

	/** Frames after the first hard ground bounce of a launch during which another may happen; later floors are landings. 0 = no limit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ground Impact|Limit", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 BounceWindowFrames = 90;

	/** The speed alone asks for a hard bounce (bBounceBySpeed, the threshold, the hitstun condition). Pure */
	bool WantsBounceAtSpeed(double JudgedSpeed, bool bInHitstun) const
	{
		return bBounceBySpeed && JudgedSpeed > static_cast<double>(BounceMinSpeed) && (bInHitstun || !bOnlyDuringHitstun);
	}

	/** The chain limit lets one more hard bounce happen: Count bounces so far, the first at FirstFrame (INDEX_NONE for none). Pure */
	bool AllowsAnotherBounce(int32 Count, int32 FirstFrame, int32 Frame) const
	{
		if (MaxBounces > 0 && Count >= MaxBounces)
		{
			return false;
		}
		return BounceWindowFrames <= 0 || Count == 0 || FirstFrame == INDEX_NONE || Frame - FirstFrame <= BounceWindowFrames;
	}
};

/** What decides a hard wall or ceiling bounce against the small bump (FGSTN_SurfaceBounceRules::Judge) */
UENUM(BlueprintType)
enum class EGSTN_SurfaceBounceJudge : uint8
{
	/** The speed into the face, the same test as the floor (FGSTN_GroundImpactRules::WantsBounceAtSpeed). Owner 2026-11-17: "same rules as the ground" */
	Speed,
	/** The hitstun still to run when the victim touches the face: at least BounceMinHitstunFrames = hard bounce, less = bump */
	RemainingHitstun,
};

/**
 * The DEFAULT bounce of a tumbling victim off a wall or a ceiling, the rules' part (owner 2026-11-17; UGSTN_CombatRules::WallImpact and
 * CeilingImpact). Every wall and ceiling bounces a launched victim, with or without a surface table:
 *
 *  - HARD bounce: the same rules as the floor's hard bounce (Smash): above the speed threshold, while the hitstun runs, the victim is reflected
 *    about the face at the restitution, still tumbling, still in hitstun. bSameAsGround reads threshold, hitstun condition, restitution and chain
 *    limit from GroundImpact; off, from Bounce below. The chain (walls and ceilings together) is counted apart from the floor's.
 *  - BUMP: slower than that (little knockback left) the victim does NOT get the floor's tech bounce: it is nudged off the face a little
 *    (BumpMinSpeed..BumpMaxSpeed, scaled by the impact speed) and plays a short clip. The clip is presentation: the combat tick projects
 *    Combat.State.Bump (+ .Wall / .Ceiling) for BumpFrames, and a reaction anim rule on that tag plays it.
 *
 * What a surface or the attack asks first is not touched: a surface row that splats, breaks or forces a bounce, a hit that forces an outcome
 * (Force) and the Defence plugin's wall tech all decide before this default. A hit that forbids Bounce gets the bump, never the hard bounce
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_SurfaceBounceRules
{
	GENERATED_BODY()

	/** Off: these faces do nothing by default (the victim slides along them, the behaviour before 2026-11-17); surface rows still apply */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Bounce")
	bool bEnabled = true;

	/** What tells the hard bounce from the bump */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Bounce", meta = (EditCondition = "bEnabled"))
	EGSTN_SurfaceBounceJudge Judge = EGSTN_SurfaceBounceJudge::Speed;

	/** Judge RemainingHitstun: the hard bounce needs at least this much hitstun left */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Bounce", meta = (ClampMin = "1", ForceUnits = "Frames",
		EditCondition = "bEnabled && Judge == EGSTN_SurfaceBounceJudge::RemainingHitstun"))
	int32 BounceMinHitstunFrames = 20;

	/** The hard bounce follows the floor's rules (GroundImpact: threshold, hitstun condition, restitution, chain limit). Off: Bounce below */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Bounce", meta = (EditCondition = "bEnabled"))
	bool bSameAsGround = true;

	/** The hard bounce's own rules when bSameAsGround is off (bBounceBySpeed off = never hard, always the bump) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Bounce", meta = (EditCondition = "bEnabled && !bSameAsGround"))
	FGSTN_GroundImpactRules Bounce;

	/** Too slow for the hard bounce: the small bump with its clip. Off: the victim slides along the face as before */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Bounce|Bump", meta = (EditCondition = "bEnabled"))
	bool bBump = true;

	/** Contacts at or under this speed into the face (grazing it) do nothing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Bounce|Bump", meta = (ClampMin = "0.0", ForceUnits = "cm/s", EditCondition = "bEnabled && bBump"))
	float NoBumpBelowSpeed = 50.f;

	/** The bump only while the hitstun runs. Off (default): a tumble after its hitstun bumps too */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Bounce|Bump", meta = (EditCondition = "bEnabled && bBump"))
	bool bBumpOnlyDuringHitstun = false;

	/** Impact speeds mapped onto BumpMinSpeed..BumpMaxSpeed: at or under ScaleMinSpeed the smallest bump, at or over ScaleMaxSpeed the biggest */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Bounce|Bump", meta = (ClampMin = "0.0", ForceUnits = "cm/s", EditCondition = "bEnabled && bBump"))
	float BumpScaleMinSpeed = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Bounce|Bump", meta = (ClampMin = "0.0", ForceUnits = "cm/s", EditCondition = "bEnabled && bBump"))
	float BumpScaleMaxSpeed = 1200.f;

	/** Speed off the face of the smallest and of the biggest bump (cm/s along the face's normal; 150 cm/s is a few cm before gravity wins) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Bounce|Bump", meta = (ClampMin = "0.0", ForceUnits = "cm/s", EditCondition = "bEnabled && bBump"))
	float BumpMinSpeed = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Bounce|Bump", meta = (ClampMin = "0.0", ForceUnits = "cm/s", EditCondition = "bEnabled && bBump"))
	float BumpMaxSpeed = 350.f;

	/** Shape of the scale: 1 linear, above 1 soft impacts stay small longer */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Bounce|Bump", meta = (ClampMin = "0.1", EditCondition = "bEnabled && bBump"))
	float BumpExponent = 1.f;

	/** The fraction of the speed along the face the bump keeps (0 = stops sliding, 1 = keeps all of it) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Bounce|Bump", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "bEnabled && bBump"))
	float BumpTangentKeep = 0.5f;

	/**
	 * Length of the bump: Combat.State.Bump lives this long (the clip's window) and the hitstun lasts at least as long. Another bump cannot
	 * start before it ends (a hard bounce can)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface Bounce|Bump", meta = (ClampMin = "1", ForceUnits = "Frames", EditCondition = "bEnabled && bBump"))
	int32 BumpFrames = 14;

	/** The hard bounce's rules in force: GroundImpact or Bounce. Pure */
	const FGSTN_GroundImpactRules& HardRules(const FGSTN_GroundImpactRules& Ground) const { return bSameAsGround ? Ground : Bounce; }

	/** The judgement asks for a hard bounce (before the chain limit, the surface and the victim). Pure */
	bool WantsHardBounce(const FGSTN_GroundImpactRules& Ground, double JudgedSpeed, int32 HitstunFramesLeft) const
	{
		const FGSTN_GroundImpactRules& Hard = HardRules(Ground);
		if (Judge == EGSTN_SurfaceBounceJudge::RemainingHitstun)
		{
			return Hard.bBounceBySpeed && JudgedSpeed > 0.0 && HitstunFramesLeft >= BounceMinHitstunFrames;
		}
		return Hard.WantsBounceAtSpeed(JudgedSpeed, HitstunFramesLeft > 0);
	}

	/** The bump's speed off the face for an impact speed, 0 for none (bump off, or grazing). Pure */
	double BumpSpeed(double JudgedSpeed) const
	{
		if (!bBump || JudgedSpeed <= static_cast<double>(NoBumpBelowSpeed))
		{
			return 0.0;
		}
		const double Range = FMath::Max(static_cast<double>(BumpScaleMaxSpeed) - static_cast<double>(BumpScaleMinSpeed), 1.0);
		const double Alpha = FMath::Clamp((JudgedSpeed - static_cast<double>(BumpScaleMinSpeed)) / Range, 0.0, 1.0);
		const double Shaped = FMath::Pow(Alpha, static_cast<double>(FMath::Max(BumpExponent, 0.1f)));
		return FMath::Lerp(static_cast<double>(BumpMinSpeed), static_cast<double>(FMath::Max(BumpMaxSpeed, BumpMinSpeed)), Shaped);
	}
};

/** Which outcomes a surface or a victim lets through (None is always allowed) */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ImpactAllowed
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	bool bSplat = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	bool bBounce = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	bool bBreakSurface = true;

	bool Allows(EGSTN_ImpactOutcome Outcome) const
	{
		switch (Outcome)
		{
		case EGSTN_ImpactOutcome::Splat: return bSplat;
		case EGSTN_ImpactOutcome::Bounce: return bBounce;
		case EGSTN_ImpactOutcome::BreakSurface: return bBreakSurface;
		default: return true;
		}
	}

	/** True when at least one real outcome is allowed */
	bool AllowsAnything() const { return bSplat || bBounce || bBreakSurface; }
};

/** Layer 3: what one surface type does to whoever hits it (combat-m4-hits 7b) */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_SurfaceImpactRow
{
	GENERATED_BODY()

	/** The surface type; a contact on a child tag (Surface.Type.Wall.Brick) uses the nearest parent row */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (Categories = "Surface.Type"))
	FGameplayTag Surface;

	/** What happens when nothing above asks for something else */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	EGSTN_ImpactOutcome DefaultOutcome = EGSTN_ImpactOutcome::None;

	/** What the surface can do at all: an outcome it does not allow can neither be a default nor be asked by a victim */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	FGSTN_ImpactAllowed Allowed;

	/**
	 * The physical truth of the surface: DefaultOutcome happens whatever the attack or the victim prefer (a trampoline always
	 * bounces). The impact speed threshold still applies.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	bool bForceDefault = false;

	/** The impact speed along the normal must exceed this, else None */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0.0", ForceUnits = "cm/s"))
	float MinImpactSpeed = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0.0"))
	float Restitution = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0", ForceUnits = "Frames"))
	int32 SplatFrames = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0.0"))
	float ImpactDamageScale = 1.f;

	/** For BreakSurface: the fraction of its speed the victim keeps when it goes through */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "0.0"))
	float ResidualSpeedScale = 0.7f;

	/**
	 * For BreakSurface: the frames the victim stops while the surface breaks. The surface is broken outside the simulation, at the end of the
	 * frame the stop opens on (the authority; a client's copy goes when the surface replicates), so the stop has to outlast the frames until it
	 * is gone, or the victim goes on into a surface that is still there
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (ClampMin = "2", ForceUnits = "Frames"))
	int32 BreakStopFrames = 6;
};

/**
 * The surface layer of the impact rules: rows by surface type tag. A contact carries the GSTNSurface type index of its surface
 * (UGSTN_SurfaceUtils::GetSurfaceTypeIndex, 0 = no type) and no tag is ever stored in a sync state or snapshot. The table compiles
 * a lookup from every type index to its row (exact tag, else the nearest parent tag) at load, and again when the surface type
 * list changes, so the order of Rows does not matter.
 */
UCLASS(BlueprintType)
class GSTNCOMBATCORE_API UGSTN_SurfaceImpactTable : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Rows by surface type tag, in any order. A tag with two rows uses the first */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surfaces")
	TArray<FGSTN_SurfaceImpactRow> Rows;

	/** Builds the tag and surface type index lookups. Called at load and after an edit; call it after filling Rows from code */
	void Compile();

	/** The row index of a surface type (exact tag, else the nearest parent), or INDEX_NONE */
	int32 FindRowIndex(const FGameplayTag& Surface) const;

	/** The row of a contact's surface type index (GSTNSurface), or null for no type, an unknown index or a type without a row */
	const FGSTN_SurfaceImpactRow* GetRow(int32 SurfaceTypeIndex) const;

	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	TMap<FGameplayTag, int32> IndexByTag;

	/** Row index by surface type index, INDEX_NONE for none; sized to the surface type list when compiled */
	mutable TArray<int32> RowBySurfaceIndex;

	/** Checksum of the surface type list RowBySurfaceIndex was built for; a different one means the indices moved */
	mutable uint32 CompiledTypeChecksum = 0;
	mutable bool bCompiledOnce = false;

	void BuildSurfaceIndexLookup() const;
};

/** Layer 2 for one surface type: what this victim can undergo there. Negative numbers mean "the surface's value" */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ImpactResponseEntry
{
	GENERATED_BODY()

	/** Empty in FGSTN_ImpactResponse::Default */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response", meta = (Categories = "Surface.Type"))
	FGameplayTag Surface;

	/** Restricts the surface's default (a heavy enemy never splats) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response")
	FGSTN_ImpactAllowed Allowed;

	/** At or above 0: the larger of this and the surface's threshold applies */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response", meta = (ClampMin = "-1.0", ForceUnits = "cm/s"))
	float MinImpactSpeed = -1.f;

	/** At or above 0: replaces the surface's */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response", meta = (ClampMin = "-1.0"))
	float Restitution = -1.f;

	/** At or above 0: replaces the surface's */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response", meta = (ClampMin = "-1", ForceUnits = "Frames"))
	int32 SplatFrames = -1;

	/** Multiplies the surface's */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response", meta = (ClampMin = "0.0"))
	float ImpactDamageScale = 1.f;
};

/** The victim's say in a surface impact (from its kit or definition), by surface type with a default for the rest */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ImpactResponse
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response")
	FGSTN_ImpactResponseEntry Default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Response")
	TArray<FGSTN_ImpactResponseEntry> PerSurface;

	/** The entry for a surface type: the first whose tag the surface matches (hierarchically), else Default */
	const FGSTN_ImpactResponseEntry& Find(const FGameplayTag& Surface) const;
};

/**
 * One contact as the decision reads it: quantised so every machine feeds the same integers. Plain data (a 16 bit field is not a
 * Blueprint type).
 */
struct GSTNCOMBATCORE_API FGSTN_ImpactInput
{
	/** GSTNSurface type index of the surface (UGSTN_SurfaceUtils::GetSurfaceTypeIndex); 0 = no surface type */
	uint8 SurfaceTypeIndex = 0;

	/** Impact speed along the contact normal in whole cm/s */
	uint16 ImpactSpeedQ = 0;

	static FGSTN_ImpactInput Make(uint8 InSurfaceTypeIndex, double ImpactSpeed)
	{
		FGSTN_ImpactInput Input;
		Input.SurfaceTypeIndex = InSurfaceTypeIndex;
		Input.ImpactSpeedQ = static_cast<uint16>(FMath::Clamp(FMath::RoundToInt(ImpactSpeed), 0, 65535));
		return Input;
	}
};

/** What a contact resolves to */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ImpactResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	EGSTN_ImpactOutcome Outcome = EGSTN_ImpactOutcome::None;

	/** The speed the contact was judged with (after the attack's SpeedScale) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	float JudgedSpeed = 0.f;

	/** For Bounce */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	float Restitution = 0.f;

	/** For Splat */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	int32 SplatFrames = 0;

	/** Surface scale times the victim's */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	float ImpactDamageScale = 1.f;

	/** For BreakSurface: the surface's fraction of the speed that is kept */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	float ResidualSpeedScale = 0.7f;

	/** For BreakSurface: the surface's stop while it breaks */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	int32 BreakStopFrames = 6;
};

/**
 * The victim layer of the impact rules as kit data (combat-m4-hits 7b): what this kit's fighter can undergo on each surface type. A kit
 * without it uses the rules' ImpactResponse. Data only.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_ImpactResponseExtension : public FGSTN_KitExtension
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Impact")
	FGSTN_ImpactResponse Response;
};
