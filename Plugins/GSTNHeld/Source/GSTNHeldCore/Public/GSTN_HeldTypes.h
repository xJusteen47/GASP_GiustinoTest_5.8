// Held objects, pure core (no GAS, no Network Prediction).

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Movement/GSTN_BallisticMode.h"

#include "GSTN_HeldTypes.generated.h"

GSTNHELDCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogGSTNHeld, Log, All);

/** Where an object is in its life. At rest it is Idle again (after a landing too): there is no separate Landed state yet */
UENUM(BlueprintType)
enum class EGSTN_HeldObjectState : uint8
{
	/** At rest in the world, dormant, can be picked up */
	Idle,
	/** In a holder's hand: drawn at the holder on every machine */
	Held,
	/** Thrown: drawn with its projectile until the projectile hits, lands or expires */
	Flying,
	/** Broken by its impact (M10 G7, EGSTN_HeldImpactBehaviour::Break): hidden, no collision, no pickup record; back at its home after its respawn time */
	Broken
};

/**
 * What a thrown object does when its flight ends (M10 G7, plan M10.21: props use their ImpactBehaviour; Explode = a gameplay effect, later data).
 * Decided by the host when the flight meets the world (WorldStatic) or its shot hits a fighter
 */
UENUM(BlueprintType)
enum class EGSTN_HeldImpactBehaviour : uint8
{
	/** Comes to rest on the floor below the contact (the slice 1 behaviour) */
	Land,
	/** Breaks (world contact or a fighter hit): gone until it respawns at its home (BreakRespawnSeconds, 0 = never) */
	Break,
	/** Rebounds off the world with the restitution, as a new shot with the same hit and credit; too slow or too many bounces = Land */
	Bounce,
	/** Stays where it met the world (a wall too), at rest and pickable there; a fighter hit = Land */
	Stick
};

/** Why a hold ended (logs, the harness, a later Combat.Event.Held.* event) */
UENUM(BlueprintType)
enum class EGSTN_HeldBreakReason : uint8
{
	None,
	/** The holder put it down (the grab button again) */
	Dropped,
	/** The holder threw it */
	Thrown,
	/** The holder got a tag of the break rules (hit, launched, grabbed) */
	HolderState,
	/** The holder got an event of the break rules (KO) */
	HolderEvent,
	/** The holder is gone */
	HolderGone
};

/**
 * When a holder lets go of its object without wanting to (plan 7, break rules). Tags and events are matched by name on the holder, so the rules
 * need no dependency on the plugins that project them (GSTNLink's Combat.Link.Role.Grabbed, the combat tick's hitstun): a tag no plugin
 * projects simply never matches. Defaults in DefaultGSTNHeld.ini (UGSTN_HeldSettings), an object can override them
 */
USTRUCT(BlueprintType)
struct GSTNHELDCORE_API FGSTN_HeldBreakRules
{
	GENERATED_BODY()

	/** The holder drops the object when it has any of these tags at the end of a frame. TEST VALUES: hitstun, tumble, grabbed */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Break")
	FGameplayTagContainer DropWhenHolderHasAny;

	/** The holder drops the object when an authoritative event of one of these types names it as target. TEST VALUE: Combat.Event.KO */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Break")
	FGameplayTagContainer DropOnHolderEvents;

	/** A dropped object (put down or broken) is placed this far in front of the holder, on the floor. TEST VALUE */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Break", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float DropForwardCm = 60.f;
};

/** Where a thrown object goes (the aim of UGSTN_GA_HeldThrow, chosen at its release) */
UENUM(BlueprintType)
enum class EGSTN_HeldThrowAim : uint8
{
	/** Along the left stick (the facing with the stick centred): a tap */
	Stick,
	/** Along the camera: the button held long enough to aim (reticle shown), or the reticle toggled on */
	Camera
};

/** How the strong button throws a held object (owner's button design 2026-11-10, subject to playtesting). Defaults in DefaultGSTNHeld.ini */
USTRUCT(BlueprintType)
struct GSTNHELDCORE_API FGSTN_HeldThrowInput
{
	GENERATED_BODY()

	/** A press shorter than this is a tap (stick throw); held this long the throw aims with the camera and the reticle shows. TEST VALUE */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Throw", meta = (ClampMin = "1"))
	int32 AimHoldFrames = 12;

	/** Held this long, the throw goes on its own (an aim never blocks the holder forever). TEST VALUE */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Throw", meta = (ClampMin = "1"))
	int32 MaxAimFrames = 300;

	/** With the reticle toggled on (FGSTN_InputCmd::bPrecisionAim) a tap aims with the camera too */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Throw")
	bool bCameraAimWhenReticleOn = true;
};

/**
 * Commands a holder loses while carrying an object of a size class (M10 G6, plan rules table): every kit binding whose command matches one of
 * Commands (hierarchically: Command.Attack bans every attack) gets the class's Carrying tag in its ignored tags when the kit is compiled, so while
 * carrying the binding does not hold. With OnlyBindingsRequiring set, only the bindings that require that context tag are touched (the air
 * attacks of a kit: Input.Context.Airborne). The held commands themselves (Command.Held.*) are never banned
 */
USTRUCT(BlueprintType)
struct GSTNHELDCORE_API FGSTN_HeldCommandBan
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Size", meta = (Categories = "Command"))
	FGameplayTagContainer Commands;

	/** Empty = every binding of the commands; set = only those whose requirements require this tag */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Size")
	FGameplayTag OnlyBindingsRequiring;
};

/**
 * A size class of held objects (M10 G6; spec 6, plan default M10.2: four classes as tags). What carrying an object of the class costs the holder
 * (the carry load: speed and jump scaled by the object's weight over the holder's capacity), which commands it loses, whether a holder must be
 * strong enough to lift it, and how hard it throws it. Every number is a TEST VALUE until the owner sets the feel (open questions of m10-slice6)
 */
USTRUCT(BlueprintType)
struct GSTNHELDCORE_API FGSTN_HeldSizeClass
{
	GENERATED_BODY()

	/** The class (Combat.Held.Size.*) an object's tuning names */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Size", meta = (Categories = "Combat.Held.Size"))
	FGameplayTag SizeTag;

	/** Projected on the holder beside Combat.Held.Carrying while it holds an object of the class (Combat.Held.Carrying.*) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Size", meta = (Categories = "Combat.Held.Carrying"))
	FGameplayTag CarryingTag;

	/** Fraction of the walk speed lost at full load (object weight = holder capacity); the loss scales linearly with the load. TEST VALUE */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Size", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SpeedLossAtFullLoad = 0.f;

	/** Fraction of the jump speed lost at full load. TEST VALUE */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Size", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float JumpLossAtFullLoad = 0.f;

	/** Only a holder whose carry capacity covers the object's weight lifts it (spec: Huge needs MaxCarryWeight); else the pickup skips it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Size")
	bool bRequiresCapacity = false;

	/** The throw's speed is the object's Throw.Speed times this. TEST VALUE */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Size", meta = (ClampMin = "0.0"))
	float ThrowSpeedScale = 1.f;

	/** What the holder cannot do while carrying (see FGSTN_HeldCommandBan) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Size")
	TArray<FGSTN_HeldCommandBan> Bans;
};

/**
 * What one object is worth to a holder and a thrower (M10 slice 1 subset of the plan's UGSTN_ObjectDefinition: no size class, weight,
 * drop-on-hit rule or impact behaviour yet). Every default is a TEST VALUE until the owner sets real kits (handover m10-slice1).
 */
USTRUCT(BlueprintType)
struct GSTNHELDCORE_API FGSTN_HeldObjectTuning
{
	GENERATED_BODY()

	FGSTN_HeldObjectTuning();

	/** A holder whose snapshot position is within this distance of the object can pick it up. TEST VALUE */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Pickup", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float PickupRadius = 200.f;

	/**
	 * Where the object sits in the holder's actor space while held (X forward, Z up, from the capsule centre). Presentation only: the
	 * plan's spike S2 decides between this root offset and a hand socket. TEST VALUE
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Hold", meta = (ForceUnits = "cm"))
	FVector HoldOffset = FVector(45.0, 0.0, 70.0);

	/** Half height of the object at rest: a landed object is placed this far above the floor it lands on */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Rest", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float RestHalfHeight = 20.f;

	/** The object's size class (Combat.Held.Size.*, UGSTN_HeldSettings::SizeClasses); none = no load, no bans, normal throw. TEST VALUE: Light */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Size", meta = (Categories = "Combat.Held.Size"))
	FGameplayTag SizeTag;

	/** The object's weight: over the holder's carry capacity it makes the carry load (FGSTN_HeldLoad). TEST VALUE */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Size", meta = (ClampMin = "0.0", ForceUnits = "kg"))
	float WeightKg = 5.f;

	/**
	 * The throw as a GSTNProjectile shot: speed, gravity, lifetime, hit shape, hit data, max hits. Origin, direction and owner are filled
	 * by the throw ability. Damage, knockback, speed and arc are TEST VALUES (the owner sets the feel and the numbers of real kits)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Throw")
	FGSTN_ProjectileParams Throw;

	/** What the object does when its flight ends (M10 G7). TEST VALUE: Land */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Impact")
	EGSTN_HeldImpactBehaviour ImpactBehaviour = EGSTN_HeldImpactBehaviour::Land;

	/** Bounce: the speed kept after a rebound (the reflected velocity times this). TEST VALUE */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Impact", meta = (ClampMin = "0.0", ClampMax = "1.0", EditCondition = "ImpactBehaviour == EGSTN_HeldImpactBehaviour::Bounce"))
	float BounceRestitution = 0.45f;

	/** Bounce: rebounds per throw before it lands. TEST VALUE */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Impact", meta = (ClampMin = "0", EditCondition = "ImpactBehaviour == EGSTN_HeldImpactBehaviour::Bounce"))
	int32 MaxBounces = 2;

	/** Bounce: a rebound slower than this lands instead. TEST VALUE */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Impact", meta = (ClampMin = "0.0", ForceUnits = "cm/s", EditCondition = "ImpactBehaviour == EGSTN_HeldImpactBehaviour::Bounce"))
	float MinBounceSpeed = 200.f;

	/** Break: seconds before a broken object is back at its home (where it first came to rest); 0 = never. TEST VALUE */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Impact", meta = (ClampMin = "0.0", ForceUnits = "s", EditCondition = "ImpactBehaviour == EGSTN_HeldImpactBehaviour::Break"))
	float BreakRespawnSeconds = 3.f;

	/** Use BreakRules instead of the project's defaults (UGSTN_HeldSettings::DefaultBreakRules): an object that survives a hit in hand, say */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Break")
	bool bOverrideBreakRules = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Break", meta = (EditCondition = "bOverrideBreakRules"))
	FGSTN_HeldBreakRules BreakRules;
};

/** One pickup request the host received for a frame (a holder asking, from where) */
struct FGSTN_PickupRequest
{
	/** Combat entity id of the holder: the race tie-break after the frame */
	int32 HolderId = INDEX_NONE;
	/** The simulation frame the request was made on (the earliest wins) */
	int32 Frame = INDEX_NONE;
	FVector Location = FVector::ZeroVector;
	/**
	 * The object the holder's interact focus named (M10 slice 5), INDEX_NONE = none. The request takes it when it is free and in reach; else
	 * (taken by an earlier request, gone, out of reach) the nearest free object in reach, as without a focus; nothing in reach = a fumble
	 */
	int32 TargetObjectId = INDEX_NONE;
	/** The holder's effective carry capacity (M10 G6): an object whose class requires capacity is skipped when heavier than this */
	float CarryCapacityKg = TNumericLimits<float>::Max();
};

/** One object that could be picked up */
struct FGSTN_PickupCandidate
{
	/** Stable id of the object on the host (registration order): the tie-break between two objects at the same distance */
	int32 ObjectId = INDEX_NONE;
	FVector Location = FVector::ZeroVector;
	float Radius = 0.f;
	bool bAvailable = true;
	/** M10 G6: the object's weight, and whether its size class needs a holder strong enough (FGSTN_HeldSizeClass::bRequiresCapacity) */
	float WeightKg = 0.f;
	bool bRequiresCapacity = false;
};

struct FGSTN_PickupAssignment
{
	int32 HolderId = INDEX_NONE;
	int32 ObjectId = INDEX_NONE;

	bool operator==(const FGSTN_PickupAssignment& Other) const { return HolderId == Other.HolderId && ObjectId == Other.ObjectId; }
};

/** Stateless rules of held objects */
struct GSTNHELDCORE_API FGSTN_HeldRules
{
	/**
	 * The pickup race (plan default M10.5): requests are served by earliest frame, then lower holder id; each takes its target (TargetObjectId)
	 * when it names one, else the nearest available object within that object's radius (equal distance: lower object id); a taken object is
	 * gone for the next request. A holder gets
	 * at most one object (its first request). Same inputs, same assignments, in the order served
	 */
	static void ResolvePickups(TConstArrayView<FGSTN_PickupRequest> Requests, TConstArrayView<FGSTN_PickupCandidate> Candidates,
		TArray<FGSTN_PickupAssignment>& OutAssignments);

	/**
	 * The object a pickup from From would take if nobody else asked in that frame: ResolvePickups with that one request, so the projected
	 * pickup target and the prompt name exactly the object the host would give. The candidate's ObjectId, INDEX_NONE when none is in reach
	 */
	static int32 FindPickupTarget(const FVector& From, TConstArrayView<FGSTN_PickupCandidate> Candidates);

	/** Where a held object is drawn: the holder's transform with the hold offset applied in its space (rotation = the holder's) */
	static FTransform HoldTransform(const FTransform& Holder, const FVector& HoldOffset);

	/** Does the holder lose its object for a state it has (DropWhenHolderHasAny)? Empty rules never break */
	static bool BreaksOnState(const FGameplayTagContainer& HolderTags, const FGSTN_HeldBreakRules& Rules);

	/** Does an authoritative event of this type naming the holder break the hold (DropOnHolderEvents, hierarchical match)? */
	static bool BreaksOnEvent(const FGameplayTag& EventType, const FGSTN_HeldBreakRules& Rules);

	/**
	 * The aim of a throw released after HeldFrames frames of the button (0 = released on the press frame): the camera once the button was held
	 * AimHoldFrames, or with the reticle toggled on (bPrecisionAim) when the input allows it; the stick otherwise
	 */
	static EGSTN_HeldThrowAim ChooseThrowAim(int32 HeldFrames, bool bPrecisionAim, const FGSTN_HeldThrowInput& Input);

	/** Whether a press still held after HeldFrames frames shows the aim reticle (the same threshold as the camera aim) */
	static bool IsAiming(int32 HeldFrames, const FGSTN_HeldThrowInput& Input) { return HeldFrames >= Input.AimHoldFrames; }

	/**
	 * What an impact does (M10 G7): the tuning's behaviour for a world contact (Normal = the contact normal) or a fighter hit (bWorld false: Bounce and
	 * Stick land, Break breaks). A Bounce with BouncesSoFar >= MaxBounces, or whose rebound (Velocity reflected on Normal, times BounceRestitution)
	 * is slower than MinBounceSpeed, lands; else OutVelocity is the rebound. Pure
	 */
	static EGSTN_HeldImpactBehaviour ResolveImpact(const FGSTN_HeldObjectTuning& Tuning, bool bWorld, const FVector& Velocity, const FVector& Normal,
		int32 BouncesSoFar, FVector& OutVelocity);

	/** Where a dropped object rests before the floor trace: DropForwardCm in front of the holder, at its height */
	static FVector DropPoint(const FTransform& Holder, const FGSTN_HeldBreakRules& Rules);
};

/**
 * The carry load (M10 G6). The host computes it when it forms a hold and writes it as the Held link's payload byte, so it is sync state: the holder's
 * link effect (UGSTN_GE_HoldLoad, the Held type's role effect) and the projected Carrying.<size> tag read it on every machine and in every
 * re-simulation without looking at the object. Byte layout: the size class index (0..3, UGSTN_HeldSettings::SizeClasses) in the top two bits, the
 * load ratio (object weight / holder capacity, clamped to 0..1) in the low six (steps of 1/63). Payload 0 = class 0 with no load
 */
struct GSTNHELDCORE_API FGSTN_HeldLoad
{
	static constexpr int32 MaxSizeClasses = 4;
	static constexpr int32 RatioSteps = 63;

	static uint8 EncodePayload(int32 SizeIndex, float LoadRatio);
	static void DecodePayload(uint8 Payload, int32& OutSizeIndex, float& OutLoadRatio);
	/** The payload back from the effect's set-by-caller magnitude (Payload / 255) */
	static uint8 PayloadFromMagnitude(float Magnitude) { return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Magnitude * 255.f), 0, 255)); }

	/** The capacity a holder lifts with: its MaxCarryWeight, else the default (the attribute's default is 0) */
	static float EffectiveCapacity(float CapacityKg, float DefaultCapacityKg) { return CapacityKg > 0.f ? CapacityKg : DefaultCapacityKg; }
	/** Weight over the effective capacity, 0..1 (no capacity at all: 1 for any weight) */
	static float LoadRatio(float WeightKg, float CapacityKg, float DefaultCapacityKg);

	/** Can a holder of this effective capacity lift the object (its class requires capacity: weight <= capacity)? A null class always lifts */
	static bool CanLift(const FGSTN_HeldSizeClass* SizeClass, float WeightKg, float EffectiveCapacityKg);

	/** The multipliers of the walk speed and the jump speed for a class at a load ratio (1 = no loss). A null class = 1 */
	static float SpeedScale(const FGSTN_HeldSizeClass* SizeClass, float LoadRatio);
	static float JumpScale(const FGSTN_HeldSizeClass* SizeClass, float LoadRatio);
};
