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
 * How hard pushboxes push, on the combat rules. One value for everyone on purpose: the two fighters of a pair compute their shares on their own
 * machines and frames, and equal settings make the shares equal and opposite with no messaging
 */
USTRUCT(BlueprintType)
struct GSTNCOMBATCORE_API FGSTN_PushboxSettings
{
	GENERATED_BODY()

	/** Off: fighters overlap freely (their capsules ignore each other either way) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox")
	bool bEnabled = true;

	/** Fraction of a pair's overlap removed per frame (split half each). 0.25 at 62.5 Hz clears most of an overlap in about 10 frames */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PushGainPerFrame = 0.25f;

	/** The most a fighter is moved by pushes in one frame (3 cm = 187 cm/s at 62.5 Hz) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float MaxPushPerFrame = 3.f;

	/** Pushbox radius when no track gives one: the movement capsule's radius times this */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pushbox", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float CapsuleRadiusScale = 0.8f;
};

/** A pushbox in the world: vertical (along Up) cylinder. Radius 0 = none */
struct FGSTN_PushboxCylinder
{
	FVector Center = FVector::ZeroVector;
	float Radius = 0.f;
	float HalfHeight = 0.f;

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

	/**
	 * Self's share of the pair's separation, a horizontal (perpendicular to Up) move. p = rA + rB - horizontal distance when the vertical spans
	 * overlap; share = min(Gain * p / 2, Cap / 2) away from Other. Coincident centres: along a fixed axis, the lower entity id one way, the other
	 * the other, so the two shares stay opposite. Zero when either is frozen or inactive, or they do not overlap
	 */
	static FVector PairShare(const FGSTN_PushboxBody& Self, const FGSTN_PushboxBody& Other, const FGSTN_PushboxSettings& Settings, const FVector& Up);

	/** The sum of Self's shares against every other body (Self's own id skipped), clamped to MaxPushPerFrame */
	static FVector ComputePush(const FGSTN_PushboxBody& Self, TConstArrayView<FGSTN_PushboxBody> Others, const FGSTN_PushboxSettings& Settings, const FVector& Up);
};
