// Pure combat logic (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "Timeline/GSTN_BodyCapsule.h"
#include "Timeline/GSTN_TimelineTypes.h"
#include "GSTN_Pushbox.generated.h"

/**
 * Pushbox (owner 2026-11-15, plan docs/notes/plans/pushbox-plan.md): what keeps two fighters apart, separate from the movement capsule (the
 * ECB, which only meets the world: fighters ignore each other's capsule, UGSTN_CombatWorldSubsystem::RegisterEntity). A vertical cylinder:
 * the push is horizontal only, so nobody slides up a rounded cap. Overlapping pushboxes are pulled apart softly, a capped distance per frame
 * (FGSTN_PushboxSettings), never rigidly: a body-fitted air capsule landing on a victim does not get thrown back by Mover's depenetration.
 *
 * Per frame, first match wins: a Pushbox track of the running move holding the frame > the movement capsule of the frame (the Body Capsule
 * track's or the pawn's own) with its radius scaled by FGSTN_PushboxSettings::CapsuleRadiusScale. So a move with a Body Capsule track gets
 * a fitting pushbox with no data of its own.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Pushbox"))
struct GSTNCOMBATCORE_API FGSTN_TimelineTrack_Pushbox : public FGSTN_TimelineTrack
{
	GENERATED_BODY()

	/** False: no pushbox on these frames (a cross-up, a dash through the opponent): this fighter neither pushes nor is pushed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox")
	bool bEnabled = true;

	/** Cylinder radius, 0 = the movement capsule's radius scaled by the rules' CapsuleRadiusScale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox", meta = (ClampMin = "0.0", ForceUnits = "cm", EditCondition = "bEnabled"))
	float Radius = 0.f;

	/** Cylinder half height, 0 = the movement capsule's */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox", meta = (ClampMin = "0.0", ForceUnits = "cm", EditCondition = "bEnabled"))
	float HalfHeight = 0.f;

	/** True: the cylinder centre is Offset (from the mesh root, X forward, Y right, Z up, as the Body Capsule track). False: the capsule's centre */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox", meta = (EditCondition = "bEnabled"))
	bool bUseOffset = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox", meta = (ForceUnits = "cm", EditCondition = "bEnabled && bUseOffset"))
	FVector Offset = FVector(0.0, 0.0, 90.0);
};

/**
 * The pushbox of a fighter in a state with no move timeline (downed, tumbling), as data (pushbox plan 4, owner 2026-11-15: a downed victim has
 * none, so the attacker can walk over it). Matched against the state tags the snapshot gathers for the entity (the core action state's tag and
 * what each snapshot contributor reports, e.g. the Defence plugin's knockdown), so no code names the state. A move's Pushbox track still wins.
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_PushboxStateRule
{
	GENERATED_BODY()

	/** The state tag, by name: an optional plugin's tag may register after the rules' defaults are built (as FGSTN_GrabRules::IgnoreTagNames) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox")
	FName StateTag;

	/** False: no pushbox while in the state (neither pushes nor is pushed) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox")
	bool bEnabled = false;

	/** Cylinder radius, 0 = the movement capsule's radius scaled by CapsuleRadiusScale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox", meta = (ClampMin = "0.0", ForceUnits = "cm", EditCondition = "bEnabled"))
	float Radius = 0.f;

	/** Cylinder half height, 0 = the movement capsule's */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox", meta = (ClampMin = "0.0", ForceUnits = "cm", EditCondition = "bEnabled"))
	float HalfHeight = 0.f;
};

/** How a character type meets other fighters (pushbox P4, owner 2026-11-16: data per character type) */
UENUM(BlueprintType)
enum class EGSTN_BodyCollisionMode : uint8
{
	/** Soft pushbox plus a rigid core nobody walks into (the fighter default) */
	PushboxAndCore,
	/** Soft pushbox only: can be walked into, slowly, and is pushed out */
	PushboxOnly,
	/** Neither: overlaps every fighter freely (a ghost) */
	None,
};

/**
 * One character type's body against other fighters, on the character (the combat component), not on the shared rules: a pair's core distance is
 * the sum of both radii, so different radii stay symmetric. How hard and how fast stays on FGSTN_PushboxSettings (one value for everyone)
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_BodyCollisionProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Collision")
	EGSTN_BodyCollisionMode Mode = EGSTN_BodyCollisionMode::PushboxAndCore;

	/** Rigid core radius, 0 = the standing capsule radius times FGSTN_PushboxSettings::CoreRadiusScale */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body Collision", meta = (ClampMin = "0.0", ForceUnits = "cm", EditCondition = "Mode == EGSTN_BodyCollisionMode::PushboxAndCore"))
	float CoreRadius = 0.f;
};

/** The state tags of an entity as the pushbox reads them, by name */
using FGSTN_PushboxStateTags = TArray<FName, TInlineAllocator<4>>;

/**
 * How hard pushboxes push, on the combat rules. One value for everyone on purpose: the two fighters of a pair compute their shares on their own
 * machines and frames, and equal settings make the shares equal and opposite with no messaging
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_PushboxSettings
{
	GENERATED_BODY()

	/** Defaults (pushbox plan 4, owner 2A): Combat.State.Knockdown and Combat.State.Tumble have no pushbox */
	FGSTN_PushboxSettings();

	/** Off: fighters overlap freely (their capsules ignore each other either way) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox")
	bool bEnabled = true;

	/** Fraction of a pair's overlap removed per frame (split half each). 0.25 at 62.5 Hz clears most of an overlap in about 10 frames */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PushGainPerFrame = 0.25f;

	/** The most a fighter is moved by pushes in one frame at a light overlap (3 cm = 187 cm/s at 62.5 Hz); grows with the overlap (PushCapGrowth) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxPushPerFrame = 3.f;

	/** Extra push cap per cm of overlap (P4: a deep overlap is pushed out faster). 0.25: 20 cm of overlap allows 8 cm per frame */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox", meta = (ClampMin = "0.0"))
	float PushCapGrowth = 0.25f;

	/**
	 * Approach clamp (P4, Smash-like): the share of a fighter's own move toward an overlapping pushbox it keeps, from 1 where the pushboxes start
	 * touching down to this at the rigid core (0 to the other's centre when either has no core). Velocity is clamped the same, so the walk slows
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ApproachKeptAtCore = 0.2f;

	/** Rigid core (P4): two cores never get closer than the sum of their radii by anyone's own move. Horizontal only; the world always wins */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox|Core")
	bool bCoreEnabled = true;

	/** Core radius when the character's profile gives none: its standing capsule radius times this (30 cm x 0.5 = 15 cm, pair 30 cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox|Core", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CoreRadiusScale = 0.5f;

	/** Cores already overlapping (a spawn, a landing beside a head, a pushbox switching on) part by at most this per frame, half each: no pop */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox|Core", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float CoreRecoverPerFrame = 4.f;

	/** Pushbox radius when no track gives one: the movement capsule's radius times this */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CapsuleRadiusScale = 0.8f;

	/** The pushbox per state with no move timeline; the first rule whose tag the entity has wins. A move's Pushbox track wins over these */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox")
	TArray<FGSTN_PushboxStateRule> StateRules;
};

/** A pushbox in the world: vertical (along Up) cylinder. Radius 0 = none */
struct FGSTN_PushboxCylinder
{
	FVector Center = FVector::ZeroVector;
	float Radius = 0.f;
	float HalfHeight = 0.f;
	/** Rigid core radius around the same axis, 0 = no core (soft only) */
	float CoreRadius = 0.f;

	bool IsActive() const { return Radius > 0.f && HalfHeight > 0.f; }
};

/** One fighter as the push reads it */
struct FGSTN_PushboxBody
{
	int32 EntityId = INDEX_NONE;
	FGSTN_PushboxCylinder Cylinder;
	/** In hitstop this frame: no pair it is part of pushes */
	bool bFrozen = false;
};

/** The pushbox rules, pure functions for the simulation and the tests */
struct GSTNCOMBATCORE_API FGSTN_Pushbox
{
	/** The first Pushbox track of Data holding Frame, null when none does (also for INDEX_NONE) */
	static const FGSTN_TimelineTrack_Pushbox* FindTrackAt(const FGSTN_CombatTimelineData& Data, int32 Frame);

	/**
	 * The pushbox of the frame in the mesh root's frame (Offset = its centre from the mesh root), from the movement capsule the character has
	 * (Capsule: the Body Capsule shape applied, or the standing one). False when the frame has no pushbox (a disabled track, or settings off)
	 */
	static bool ResolveLocal(const FGSTN_CombatTimelineData* Data, int32 TimelineFrame, const FGSTN_BodyCapsuleShape& Capsule,
		const FGSTN_PushboxSettings& Settings, FGSTN_BodyCapsuleShape& OutLocal);

	/** The same with the entity's state tags: Pushbox track of the move > first matching StateRules entry > the movement capsule */
	static bool ResolveLocal(const FGSTN_CombatTimelineData* Data, int32 TimelineFrame, const FGSTN_BodyCapsuleShape& Capsule,
		const FGSTN_PushboxSettings& Settings, TConstArrayView<FName> StateTags, FGSTN_BodyCapsuleShape& OutLocal);

	/** The first rule of Settings.StateRules whose tag is in StateTags, null when none */
	static const FGSTN_PushboxStateRule* FindStateRule(const FGSTN_PushboxSettings& Settings, TConstArrayView<FName> StateTags);

	/**
	 * Self's share of the pair's separation, a horizontal (perpendicular to Up) move. p = rA + rB - horizontal distance when the vertical spans
	 * overlap; share = min(Gain * p / 2, Cap / 2) away from Other. Coincident centres: along a fixed axis, the lower entity id one way, the other
	 * the other, so the two shares stay opposite. Zero when either is frozen or inactive, or they do not overlap
	 */
	static FVector PairShare(const FGSTN_PushboxBody& Self, const FGSTN_PushboxBody& Other, const FGSTN_PushboxSettings& Settings, const FVector& Up);

	/** The sum of Self's shares against every other body (Self's own id skipped), clamped to MaxPushPerFrame + PushCapGrowth x the deepest overlap */
	static FVector ComputePush(const FGSTN_PushboxBody& Self, TConstArrayView<FGSTN_PushboxBody> Others, const FGSTN_PushboxSettings& Settings, const FVector& Up);

	/**
	 * P4 approach clamp + rigid core: Self's own move of the frame (Move, from Self.Cylinder at the start of the frame), corrected against the
	 * others where they were at the start of the frame (the step snapshot, the same on every machine and in a resim). Only the horizontal part
	 * changes, and only along the line between the centres (never up, never sliding sideways). Per overlapping other, in the order given:
	 * the move toward it is scaled from 1 (pushboxes touching) to ApproachKeptAtCore (cores touching); then the end may close at most half
	 * the free gap to the core sum (the other may close the other half in the same frame), or, when already inside, must be the start distance
	 * plus half CoreRecoverPerFrame away. Two passes. Frozen others
	 * still block. Velocity (optional) loses the same inward part. Self with no core still gets the approach clamp
	 */
	static FVector ConstrainMove(const FGSTN_PushboxBody& Self, const FVector& Move, TConstArrayView<FGSTN_PushboxBody> Others,
		const FGSTN_PushboxSettings& Settings, const FVector& Up, FVector* InOutVelocity = nullptr);
};
