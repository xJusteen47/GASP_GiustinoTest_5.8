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
	Flying
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

	/**
	 * The throw as a GSTNProjectile shot: speed, gravity, lifetime, hit shape, hit data, max hits. Origin, direction and owner are filled
	 * by the throw ability. Damage, knockback, speed and arc are TEST VALUES (the owner sets the feel and the numbers of real kits)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Held|Throw")
	FGSTN_ProjectileParams Throw;

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
};

/** One object that could be picked up */
struct FGSTN_PickupCandidate
{
	/** Stable id of the object on the host (registration order): the tie-break between two objects at the same distance */
	int32 ObjectId = INDEX_NONE;
	FVector Location = FVector::ZeroVector;
	float Radius = 0.f;
	bool bAvailable = true;
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

	/** Where a dropped object rests before the floor trace: DropForwardCm in front of the holder, at its height */
	static FVector DropPoint(const FTransform& Holder, const FGSTN_HeldBreakRules& Rules);
};
